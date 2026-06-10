/**
 * @file flipdeck_ui.c
 * @brief FlipDeck User Interface Implementation
 */

#include "flipdeck_ui.h"
#include "profile_manager.h"
#include "uart_bridge.h"
#include "usb_hid.h"
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    FlipDeckView_CategoryBrowser,
    FlipDeckView_ActionBrowser,
    FlipDeckView_Confirm,
    FlipDeckView_LongSnippetWarning,
    FlipDeckView_Settings,
} FlipDeckView;

typedef struct {
    FlipDeckApp* app_ctx;
    Gui* gui;
    ViewPort* view_port;
    FlipDeckView current_view;
    char category_ids[FLIPDECK_MAX_CATEGORIES][32];
    FlipDeckProfileCategory current_category;
} FlipDeckUi;

static FlipDeckUi ui;

static void flipdeck_ui_draw_callback(Canvas* canvas, void* context);
static void flipdeck_ui_input_callback(InputEvent* event, void* context);
static void flipdeck_ui_draw_category_browser(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_action_browser(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_confirm(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_settings(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_long_snippet_warning(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_input_category_browser(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_action_browser(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_confirm(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_settings(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_long_snippet_warning(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_send_action(FlipDeckUi* ui_ctx, FlipDeckAction* action);

void flipdeck_ui_init(FlipDeckApp* app_ctx) {
    FURI_LOG_I("FlipDeck", "Initializing UI");

    memset(&ui, 0, sizeof(ui));
    ui.app_ctx = app_ctx;
    ui.current_view = FlipDeckView_CategoryBrowser;

    profile_manager_list_categories(ui.category_ids, &app_ctx->category_count);

    ui.view_port = view_port_alloc();
    view_port_draw_callback_set(ui.view_port, flipdeck_ui_draw_callback, &ui);
    view_port_input_callback_set(ui.view_port, flipdeck_ui_input_callback, &ui);

    ui.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(ui.gui, ui.view_port, GuiLayerFullscreen);
}

void flipdeck_ui_free(void) {
    FURI_LOG_I("FlipDeck", "Freeing UI");

    if(ui.gui && ui.view_port) {
        gui_remove_view_port(ui.gui, ui.view_port);
    }
    if(ui.view_port) {
        view_port_free(ui.view_port);
        ui.view_port = NULL;
    }
    if(ui.gui) {
        furi_record_close(RECORD_GUI);
        ui.gui = NULL;
    }
}

void flipdeck_ui_handle_category_browser(void) {
    ui.current_view = FlipDeckView_CategoryBrowser;
    view_port_update(ui.view_port);
}

void flipdeck_ui_handle_action_browser(void) {
    ui.current_view = FlipDeckView_ActionBrowser;
    view_port_update(ui.view_port);
}

void flipdeck_ui_handle_action_detail(void) {
    flipdeck_ui_handle_confirm();
}

void flipdeck_ui_handle_confirm(void) {
    ui.current_view = FlipDeckView_Confirm;
    view_port_update(ui.view_port);
}

void flipdeck_ui_handle_settings(void) {
    ui.current_view = FlipDeckView_Settings;
    view_port_update(ui.view_port);
}

void flipdeck_ui_handle_long_snippet_warning(void) {
    ui.current_view = FlipDeckView_LongSnippetWarning;
    view_port_update(ui.view_port);
}

static void flipdeck_ui_draw_callback(Canvas* canvas, void* context) {
    FlipDeckUi* ui_ctx = context;
    canvas_clear(canvas);

    switch(ui_ctx->current_view) {
        case FlipDeckView_CategoryBrowser:
            flipdeck_ui_draw_category_browser(canvas, ui_ctx);
            break;
        case FlipDeckView_ActionBrowser:
            flipdeck_ui_draw_action_browser(canvas, ui_ctx);
            break;
        case FlipDeckView_Confirm:
            flipdeck_ui_draw_confirm(canvas, ui_ctx);
            break;
        case FlipDeckView_LongSnippetWarning:
            flipdeck_ui_draw_long_snippet_warning(canvas, ui_ctx);
            break;
        case FlipDeckView_Settings:
            flipdeck_ui_draw_settings(canvas, ui_ctx);
            break;
    }
}

static void flipdeck_ui_input_callback(InputEvent* event, void* context) {
    FlipDeckUi* ui_ctx = context;

    switch(ui_ctx->current_view) {
        case FlipDeckView_CategoryBrowser:
            flipdeck_ui_input_category_browser(ui_ctx, event);
            break;
        case FlipDeckView_ActionBrowser:
            flipdeck_ui_input_action_browser(ui_ctx, event);
            break;
        case FlipDeckView_Confirm:
            flipdeck_ui_input_confirm(ui_ctx, event);
            break;
        case FlipDeckView_LongSnippetWarning:
            flipdeck_ui_input_long_snippet_warning(ui_ctx, event);
            break;
        case FlipDeckView_Settings:
            flipdeck_ui_input_settings(ui_ctx, event);
            break;
    }

    view_port_update(ui_ctx->view_port);
}

static void flipdeck_ui_draw_header(Canvas* canvas, const char* title) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, title);
    canvas_draw_line(canvas, 0, 14, 128, 14);
}

static void flipdeck_ui_draw_category_browser(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    flipdeck_ui_draw_header(canvas, "FlipDeck");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 92, 10, app->usb_connected ? "USB" : "NO USB");

    for(uint32_t i = 0; i < app->category_count && i < 4; i++) {
        int32_t y = 26 + (i * 10);
        if(i == app->current_category_index) {
            elements_frame(canvas, 0, y - 8, 124, 10);
        }
        canvas_draw_str(canvas, 5, y, ui_ctx->category_ids[i]);
    }

    elements_button_center(canvas, "Open");
    elements_button_right(canvas, "Settings");
}

static void flipdeck_ui_draw_action_browser(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    flipdeck_ui_draw_header(canvas, ui_ctx->current_category.name);

    canvas_set_font(canvas, FontSecondary);
    for(uint32_t i = 0; i < ui_ctx->current_category.action_count && i < 4; i++) {
        int32_t y = 26 + (i * 10);
        if(i == app->selected_action_index) {
            elements_frame(canvas, 0, y - 8, 124, 10);
        }
        canvas_draw_str(canvas, 5, y, ui_ctx->current_category.actions[i].label);
    }

    elements_button_left(canvas, "Back");
    elements_button_center(canvas, "Send");
}

static void flipdeck_ui_draw_confirm(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckAction* action =
        &ui_ctx->current_category.actions[ui_ctx->app_ctx->selected_action_index];

    flipdeck_ui_draw_header(canvas, "Send Command?");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, action->label);
    canvas_draw_str(canvas, 5, 42, action->value);
    elements_button_left(canvas, "Cancel");
    elements_button_center(canvas, "Send");
}

static void flipdeck_ui_draw_settings(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    flipdeck_ui_draw_header(canvas, "Settings");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, app->usb_connected ? "USB connected" : "USB disconnected");
    canvas_draw_str(canvas, 5, 42, "Back returns to profiles");
    elements_button_left(canvas, "Back");
}

static void flipdeck_ui_draw_long_snippet_warning(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckAction* action =
        &ui_ctx->current_category.actions[ui_ctx->app_ctx->selected_action_index];

    flipdeck_ui_draw_header(canvas, "Long Snippet");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, action->label);
    canvas_draw_str(canvas, 5, 42, "May take a while to send");
    elements_button_left(canvas, "Cancel");
    elements_button_center(canvas, "Send");
}

static void flipdeck_ui_input_category_browser(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(event->type != InputTypeShort) return;

    switch(event->key) {
        case InputKeyUp:
            if(app->current_category_index > 0) app->current_category_index--;
            break;
        case InputKeyDown:
            if(app->current_category_index + 1 < app->category_count) app->current_category_index++;
            break;
        case InputKeyOk:
            if(app->category_count == 0) break;
            profile_manager_load_category(
                ui_ctx->category_ids[app->current_category_index],
                &ui_ctx->current_category);
            strncpy(app->current_category_id, ui_ctx->category_ids[app->current_category_index], 31);
            app->current_category_id[31] = '\0';
            app->selected_action_index = 0;
            app->state = FlipDeckState_ActionBrowser;
            break;
        case InputKeyRight:
            app->state = FlipDeckState_Settings;
            break;
        default:
            break;
    }
}

static void flipdeck_ui_input_action_browser(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(event->type != InputTypeShort) return;

    uint32_t action_count = ui_ctx->current_category.action_count;
    if(action_count == 0 && event->key != InputKeyBack) return;

    switch(event->key) {
        case InputKeyUp:
            if(app->selected_action_index > 0) app->selected_action_index--;
            break;
        case InputKeyDown:
            if(app->selected_action_index + 1 < action_count) app->selected_action_index++;
            break;
        case InputKeyOk: {
            FlipDeckAction* action = &ui_ctx->current_category.actions[app->selected_action_index];
            if(action->confirm || app->settings.confirm_before_send) {
                app->state = FlipDeckState_SendConfirm;
            } else {
                flipdeck_ui_send_action(ui_ctx, action);
            }
            break;
        }
        case InputKeyBack:
            app->state = FlipDeckState_CategoryBrowser;
            app->selected_action_index = 0;
            break;
        default:
            break;
    }
}

static void flipdeck_ui_input_confirm(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(event->type != InputTypeShort) return;

    switch(event->key) {
        case InputKeyOk:
            flipdeck_ui_send_action(ui_ctx, &ui_ctx->current_category.actions[app->selected_action_index]);
            app->state = FlipDeckState_ActionBrowser;
            break;
        case InputKeyBack:
            app->state = FlipDeckState_ActionBrowser;
            break;
        default:
            break;
    }
}

static void flipdeck_ui_input_settings(FlipDeckUi* ui_ctx, InputEvent* event) {
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        ui_ctx->app_ctx->state = FlipDeckState_CategoryBrowser;
    }
}

static void flipdeck_ui_input_long_snippet_warning(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(event->type != InputTypeShort) return;

    switch(event->key) {
        case InputKeyOk: {
            FlipDeckAction* action = &ui_ctx->current_category.actions[app->selected_action_index];
            if(action->target == FlipDeckActionTarget_WifiUart) {
                uart_bridge_send_string(action->value);
            } else {
                usb_hid_send_string(action->value);
            }
            snprintf(app->status_message, sizeof(app->status_message), "Sent!");
            app->state = FlipDeckState_ActionBrowser;
            break;
        }
        case InputKeyBack:
            app->state = FlipDeckState_ActionBrowser;
            break;
        default:
            break;
    }
}

static void flipdeck_ui_send_action(FlipDeckUi* ui_ctx, FlipDeckAction* action) {
    if(!action) return;

    if(!profile_manager_validate_action(action)) {
        FURI_LOG_W("FlipDeck", "Blocked unsafe action");
        return;
    }

    if(action->type == FlipDeckActionType_Text) {
        if(strlen(action->value) > FLIPDECK_MAX_SNIPPET_LENGTH_WARN) {
            ui_ctx->app_ctx->state = FlipDeckState_LongSnippetWarning;
            return;
        }
        if(action->target == FlipDeckActionTarget_WifiUart) {
            uart_bridge_send_string(action->value);
        } else {
            usb_hid_send_string(action->value);
        }
    } else if(action->type == FlipDeckActionType_Key) {
        usb_hid_send_key(action->value);
    } else if(action->type == FlipDeckActionType_KeyCombo) {
        usb_hid_send_key_combo(action->value);
    }

    snprintf(ui_ctx->app_ctx->status_message, sizeof(ui_ctx->app_ctx->status_message), "Sent!");
}
