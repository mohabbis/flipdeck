/**
 * @file flipdeck.c
 * @brief FlipDeck for Flipper Zero: a portable status and control terminal
 * for FlipDeck on the Mac. Entry point and event loop.
 *
 * Threads: the GUI thread draws, the BT thread delivers bytes/status, and
 * this app thread does everything else. Shared state is guarded by `mutex`.
 */
#include "fd_ble.h"
#include "fd_state.h"
#include "fd_ui.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>

#define TAG "FlipDeck"
#define FD_TICK_MS 250

typedef enum {
    FdEventInput,
    FdEventData,
    FdEventLink,
} FdEventType;

typedef struct {
    FdEventType type;
    InputEvent input;
} FdAppEvent;

typedef struct {
    FdState state;
    FdUi ui;
    FdLineReader reader;
    FdOutbox out;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    FdBle* ble;
} FdApp;

static void fd_draw_callback(Canvas* canvas, void* context) {
    FdApp* app = context;
    if(furi_mutex_acquire(app->mutex, 50) != FuriStatusOk) return;
    fd_ui_draw(&app->ui, &app->state, canvas, furi_get_tick());
    furi_mutex_release(app->mutex);
}

static void fd_input_callback(InputEvent* input, void* context) {
    FdApp* app = context;
    FdAppEvent event = {.type = FdEventInput, .input = *input};
    furi_message_queue_put(app->queue, &event, 0);
}

static void fd_ble_data_callback(void* context) {
    FdApp* app = context;
    FdAppEvent event = {.type = FdEventData};
    // Non-blocking: if the queue is full, the loop drains the buffer anyway.
    furi_message_queue_put(app->queue, &event, 0);
}

static void fd_ble_link_callback(void* context) {
    FdApp* app = context;
    FdAppEvent event = {.type = FdEventLink};
    // Only a wake-up: the loop reads the link state itself every tick, so a
    // dropped event can't desynchronize it, and the BT thread never blocks.
    furi_message_queue_put(app->queue, &event, 0);
}

/** Moves queued outgoing frames out under the mutex, then sends without it. */
static void fd_flush(FdApp* app) {
    char frames[sizeof(app->out.data)];
    size_t len;
    uint16_t mtu;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    len = app->out.len;
    memcpy(frames, app->out.data, len);
    app->out.len = 0;
    mtu = app->state.mtu;
    furi_mutex_release(app->mutex);
    if(len > 0 && !fd_ble_send(app->ble, (const uint8_t*)frames, len, mtu)) {
        FURI_LOG_W(TAG, "Couldn't send %u bytes", (unsigned)len);
    }
}

int32_t flipdeck_app(void* p) {
    UNUSED(p);
    FdApp* app = malloc(sizeof(FdApp));
    memset(app, 0, sizeof(FdApp));
    fd_state_init(&app->state, furi_hal_random_get());
    fd_ui_init(&app->ui);
    fd_line_reset(&app->reader);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(16, sizeof(FdAppEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, fd_draw_callback, app);
    view_port_input_callback_set(app->view_port, fd_input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->ble = fd_ble_alloc(fd_ble_data_callback, fd_ble_link_callback, app);

    bool running = true;
    while(running) {
        FdAppEvent event;
        FuriStatus status = furi_message_queue_get(app->queue, &event, FD_TICK_MS);
        uint32_t changes = FdChangeNone;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        uint32_t now = furi_get_tick();
        if(status == FuriStatusOk && event.type == FdEventInput) {
            if(fd_ui_input(&app->ui, &app->state, &event.input, now, &app->out) & FdUiExit) running = false;
        }

        bool connected = fd_ble_is_connected(app->ble);
        if(connected && app->state.link == FdLinkOffline) {
            // Drop any partial frame left over from a previous connection.
            fd_ble_discard(app->ble);
            fd_line_reset(&app->reader);
            fd_state_link_up(&app->state, now);
            changes |= FdChangeRedraw;
            FURI_LOG_I(TAG, "Mac connected");
        } else if(!connected && app->state.link != FdLinkOffline) {
            bool had_request = app->state.request == FdRequestPending;
            fd_state_link_down(&app->state);
            changes |= FdChangeRedraw | (had_request ? FdChangeResult : FdChangeNone);
            FURI_LOG_I(TAG, "Mac disconnected");
        }

        // Always drain received bytes (events may have been coalesced).
        uint8_t buffer[128];
        size_t received;
        bool drained_any = false;
        while((received = fd_ble_read(app->ble, buffer, sizeof(buffer))) > 0) {
            changes |= fd_state_receive(&app->state, &app->reader, buffer, received, now, &app->out);
            drained_any = true;
        }
        changes |= fd_state_tick(&app->state, now);
        if(changes & FdChangeResult) fd_ui_on_result(&app->ui, &app->state, now);
        if(changes & FdChangeAlert) fd_ui_on_alert(&app->ui);
        furi_mutex_release(app->mutex);

        if(drained_any) fd_ble_rx_drained(app->ble);
        fd_flush(app);
        if(changes & FdChangeAlert) {
            notification_message(app->notifications, &sequence_double_vibro);
            notification_message(app->notifications, &sequence_blink_red_100);
        }
        // Redraw every tick so durations and staleness stay current.
        view_port_update(app->view_port);
    }

    fd_ble_free(app->ble);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
