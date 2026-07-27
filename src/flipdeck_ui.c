/**
 * @file flipdeck_ui.c
 * @brief FlipDeck User Interface Implementation
 */

#include "flipdeck_ui.h"
#include "nfc_bridge.h"
#include "profile_manager.h"
#include "subghz_bridge.h"
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
    FlipDeckView_NfcScan,
    FlipDeckView_SubghzScan,
    FlipDeckView_Confirm,
    FlipDeckView_LongSnippetWarning,
    FlipDeckView_Settings,
} FlipDeckView;

// Which hardware trigger source (if any) is currently waiting for the user
// to pick a category/action target for an unrecognized token. At most one
// bind can be pending at a time - a tagged union rather than one flag per
// source makes that structurally true instead of "shouldn't happen".
typedef enum {
    FlipDeckBindKind_None,
    FlipDeckBindKind_Nfc,
    FlipDeckBindKind_Subghz,
} FlipDeckBindKind;

typedef struct {
    FlipDeckBindKind kind;
    // Holds either an NFC UID or a Sub-GHz signature hash, whichever kind
    // is pending. Sized for the larger of the two (FLIPDECK_NFC_UID_HEX_LEN).
    char id_hex[FLIPDECK_NFC_UID_HEX_LEN];
} FlipDeckPendingBind;

typedef struct {
    FlipDeckApp* app_ctx;
    Gui* gui;
    ViewPort* view_port;
    FlipDeckView current_view;
    char category_ids[FLIPDECK_MAX_CATEGORIES][32];
    FlipDeckProfileCategory current_category;
    // Source category id for each entry in current_category.actions, so a
    // favorite can be looked up/toggled regardless of whether the actions
    // came from one category or were flattened from the favorites/NFC/
    // Sub-GHz list.
    char action_category_ids[FLIPDECK_MAX_ACTIONS_PER_CATEGORY][32];
    uint32_t category_scroll_offset;
    uint32_t action_scroll_offset;
    uint32_t settings_index;
    uint32_t settings_scroll_offset;
    // NFC tag mappings, (re)loaded each time the NFC scan screen is entered.
    FlipDeckNfcTag nfc_tags[FLIPDECK_MAX_NFC_TAGS];
    uint32_t nfc_tag_count;
    bool nfc_scan_active;
    char nfc_scan_status[32];
    // Sub-GHz remote mappings, (re)loaded each time the Sub-GHz scan screen
    // is entered - mirrors the NFC fields above exactly.
    FlipDeckSubghzRemote subghz_remotes[FLIPDECK_MAX_SUBGHZ_REMOTES];
    uint32_t subghz_remote_count;
    bool subghz_scan_active;
    char subghz_scan_status[32];
    // Non-empty (kind != None) while picking a category/action target for a
    // freshly-detected unknown token from either trigger source. See
    // flipdeck_ui_is_binding().
    FlipDeckPendingBind pending_bind;
} FlipDeckUi;

#define FLIPDECK_FAVORITES_ROW_ID "__favorites__"
#define FLIPDECK_NFC_TAG_ROW_ID "__nfc_tag__"
#define FLIPDECK_SUBGHZ_REMOTE_ROW_ID "__subghz_remote__"

static void flipdeck_ui_open_favorites(FlipDeckUi* ui_ctx);

static FlipDeckUi ui;

static void flipdeck_ui_draw_callback(Canvas* canvas, void* context);
static void flipdeck_ui_input_callback(InputEvent* event, void* context);
static void flipdeck_ui_draw_category_browser(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_action_browser(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_confirm(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_settings(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_long_snippet_warning(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_nfc_scan(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_draw_subghz_scan(Canvas* canvas, FlipDeckUi* ui_ctx);
static void flipdeck_ui_input_category_browser(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_action_browser(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_confirm(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_settings(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_long_snippet_warning(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_nfc_scan(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_input_subghz_scan(FlipDeckUi* ui_ctx, InputEvent* event);
static void flipdeck_ui_send_action(FlipDeckUi* ui_ctx, FlipDeckAction* action);
static void flipdeck_ui_set_send_status(FlipDeckApp* app, FlipDeckAction* action, bool ok);
static bool flipdeck_ui_has_favorites_row(FlipDeckApp* app);
static bool flipdeck_ui_is_binding(FlipDeckUi* ui_ctx);
static void flipdeck_ui_category_rows(
    FlipDeckUi* ui_ctx, bool* has_favorites_row_out, uint32_t* synthetic_offset_out);
static bool flipdeck_ui_load_single_action(
    FlipDeckUi* ui_ctx,
    const char* category_id,
    const char* label,
    const char* synthetic_category_name,
    const char* synthetic_category_id);

/* Returns the action at app_ctx->selected_action_index, or NULL if the index is
 * stale/out of range for the currently loaded category. Callers must treat NULL
 * as "nothing selected" rather than indexing the array directly. */
static FlipDeckAction* flipdeck_ui_get_selected_action(FlipDeckUi* ui_ctx) {
    uint32_t index = ui_ctx->app_ctx->selected_action_index;
    if(index >= ui_ctx->current_category.action_count) {
        FURI_LOG_W("FlipDeck", "Selected action index %lu out of range (count %lu)",
            (unsigned long)index,
            (unsigned long)ui_ctx->current_category.action_count);
        return NULL;
    }
    return &ui_ctx->current_category.actions[index];
}

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

    // The favorites row (and, while binding, the favorites/NFC rows both)
    // can appear/disappear between visits, so clamp a now-stale selection
    // back in range.
    bool has_favorites_row;
    uint32_t synthetic_offset;
    flipdeck_ui_category_rows(&ui, &has_favorites_row, &synthetic_offset);
    uint32_t total_rows = ui.app_ctx->category_count + synthetic_offset;
    if(total_rows == 0) {
        ui.app_ctx->current_category_index = 0;
    } else if(ui.app_ctx->current_category_index >= total_rows) {
        ui.app_ctx->current_category_index = total_rows - 1;
    }

    view_port_update(ui.view_port);
}

void flipdeck_ui_handle_action_browser(void) {
    ui.current_view = FlipDeckView_ActionBrowser;
    view_port_update(ui.view_port);
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

void flipdeck_ui_handle_nfc_scan(void) {
    FlipDeckApp* app = ui.app_ctx;
    ui.current_view = FlipDeckView_NfcScan;

    if(!ui.nfc_scan_active) {
        ui.nfc_tag_count = 0;
        profile_manager_load_nfc_tags(ui.nfc_tags, &ui.nfc_tag_count);
        ui.nfc_scan_status[0] = '\0';
        nfc_bridge_start_scan();
        ui.nfc_scan_active = true;
    }

    char uid_hex[FLIPDECK_NFC_UID_HEX_LEN];
    if(nfc_bridge_poll_tag(uid_hex, sizeof(uid_hex))) {
        // nfc_bridge_poll_tag() already stopped scanning on a successful read.
        ui.nfc_scan_active = false;

        const FlipDeckNfcTag* tag = profile_manager_find_nfc_tag(ui.nfc_tags, ui.nfc_tag_count, uid_hex);
        if(!tag) {
            // Unknown tag: hand off to the category browser in binding mode
            // to pick what it should map to.
            ui.pending_bind.kind = FlipDeckBindKind_Nfc;
            strncpy(ui.pending_bind.id_hex, uid_hex, sizeof(ui.pending_bind.id_hex) - 1);
            ui.pending_bind.id_hex[sizeof(ui.pending_bind.id_hex) - 1] = '\0';
            app->current_category_index = 0;
            app->state = FlipDeckState_CategoryBrowser;
        } else if(flipdeck_ui_load_single_action(
                      &ui, tag->category_id, tag->label, "NFC Tag", FLIPDECK_NFC_TAG_ROW_ID)) {
            // Known tag with a still-valid mapping: NFC-triggered sends
            // always land on the confirm screen, regardless of
            // confirm_before_send or the long-press quick-send shortcut -
            // a tag's UID is trivially cloneable, so it's a weaker signal
            // of intent than a person holding OK on a chosen list item.
            app->state = FlipDeckState_SendConfirm;
        } else {
            // Mapping points at a category/action that no longer exists
            // (e.g. the profile was edited since binding). Report and stay
            // on this screen rather than silently re-arming or guessing.
            snprintf(ui.nfc_scan_status, sizeof(ui.nfc_scan_status), "Mapped action missing");
        }
    }

    view_port_update(ui.view_port);
}

void flipdeck_ui_handle_subghz_scan(void) {
    FlipDeckApp* app = ui.app_ctx;
    ui.current_view = FlipDeckView_SubghzScan;

    if(!ui.subghz_scan_active) {
        ui.subghz_remote_count = 0;
        profile_manager_load_subghz_remotes(ui.subghz_remotes, &ui.subghz_remote_count);
        ui.subghz_scan_status[0] = '\0';
        subghz_bridge_start_scan();
        ui.subghz_scan_active = true;
    }

    char sig_hex[FLIPDECK_SUBGHZ_SIG_HEX_LEN];
    if(subghz_bridge_poll_signal(sig_hex, sizeof(sig_hex))) {
        // subghz_bridge_poll_signal() already stopped scanning on a hit.
        ui.subghz_scan_active = false;

        const FlipDeckSubghzRemote* remote =
            profile_manager_find_subghz_remote(ui.subghz_remotes, ui.subghz_remote_count, sig_hex);
        if(!remote) {
            // Unknown signature: hand off to the category browser in
            // binding mode, same as an unknown NFC tag.
            ui.pending_bind.kind = FlipDeckBindKind_Subghz;
            strncpy(ui.pending_bind.id_hex, sig_hex, sizeof(ui.pending_bind.id_hex) - 1);
            ui.pending_bind.id_hex[sizeof(ui.pending_bind.id_hex) - 1] = '\0';
            app->current_category_index = 0;
            app->state = FlipDeckState_CategoryBrowser;
        } else if(flipdeck_ui_load_single_action(
                      &ui,
                      remote->category_id,
                      remote->label,
                      "Sub-GHz Remote",
                      FLIPDECK_SUBGHZ_REMOTE_ROW_ID)) {
            // Known remote with a still-valid mapping: Sub-GHz-triggered
            // sends also always land on the confirm screen - for an even
            // stronger reason than NFC's UID-cloneability, since RF is
            // receivable from across a room with zero physical contact
            // with the device at all.
            app->state = FlipDeckState_SendConfirm;
        } else {
            snprintf(ui.subghz_scan_status, sizeof(ui.subghz_scan_status), "Mapped action missing");
        }
    }

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
        case FlipDeckView_NfcScan:
            flipdeck_ui_draw_nfc_scan(canvas, ui_ctx);
            break;
        case FlipDeckView_SubghzScan:
            flipdeck_ui_draw_subghz_scan(canvas, ui_ctx);
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
        case FlipDeckView_NfcScan:
            flipdeck_ui_input_nfc_scan(ui_ctx, event);
            break;
        case FlipDeckView_SubghzScan:
            flipdeck_ui_input_subghz_scan(ui_ctx, event);
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

static uint32_t flipdeck_ui_scroll_offset_for_selection(
    uint32_t selected_index,
    uint32_t current_offset,
    uint32_t total_count,
    uint32_t visible_rows) {
    if(total_count <= visible_rows) return 0;
    if(selected_index < current_offset) return selected_index;
    if(selected_index >= current_offset + visible_rows) {
        return selected_index - visible_rows + 1;
    }
    return current_offset;
}

// Whether a synthetic "Favorites" row is shown before the real categories.
static bool flipdeck_ui_has_favorites_row(FlipDeckApp* app) {
    return app->settings.favorite_count > 0;
}

// True while picking a category/action target for a freshly-detected
// unknown token from either trigger source (see flipdeck_ui_handle_nfc_scan
// / flipdeck_ui_handle_subghz_scan).
static bool flipdeck_ui_is_binding(FlipDeckUi* ui_ctx) {
    return ui_ctx->pending_bind.kind != FlipDeckBindKind_None;
}

// Layout of the synthetic rows shown before the real categories in the
// category browser: "* Favorites" (only when at least one favorite exists),
// "~ NFC Scan" (always present), "^ Sub-GHz Scan" (present unless the
// Sub-GHz bridge failed to initialize). All three are hidden while binding,
// since binding only wants a real category/action target, not another trip
// into Favorites or a second scan.
static void flipdeck_ui_category_rows(
    FlipDeckUi* ui_ctx, bool* has_favorites_row_out, uint32_t* synthetic_offset_out) {
    bool binding = flipdeck_ui_is_binding(ui_ctx);
    bool has_favorites_row = !binding && flipdeck_ui_has_favorites_row(ui_ctx->app_ctx);
    uint32_t nfc_row_count = binding ? 0 : 1;
    uint32_t subghz_row_count = (!binding && ui_ctx->app_ctx->subghz_available) ? 1 : 0;
    *has_favorites_row_out = has_favorites_row;
    *synthetic_offset_out = (has_favorites_row ? 1 : 0) + nfc_row_count + subghz_row_count;
}

static void flipdeck_ui_draw_category_browser(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    bool binding = flipdeck_ui_is_binding(ui_ctx);
    flipdeck_ui_draw_header(canvas, binding ? "Bind Tag To..." : "FlipDeck");

    canvas_set_font(canvas, FontSecondary);
    if(!binding) canvas_draw_str(canvas, 92, 10, app->usb_connected ? "USB" : "NO USB");

    bool has_favorites_row;
    uint32_t synthetic_offset;
    flipdeck_ui_category_rows(ui_ctx, &has_favorites_row, &synthetic_offset);
    bool has_nfc_row = !binding;
    bool has_subghz_row = !binding && app->subghz_available;
    uint32_t nfc_row_index = has_favorites_row ? 1u : 0u;
    uint32_t subghz_row_index = nfc_row_index + (has_nfc_row ? 1u : 0u);
    uint32_t total_rows = app->category_count + synthetic_offset;

    ui_ctx->category_scroll_offset = flipdeck_ui_scroll_offset_for_selection(
        app->current_category_index,
        ui_ctx->category_scroll_offset,
        total_rows,
        4);

    for(uint32_t row = 0; row < 4 && row + ui_ctx->category_scroll_offset < total_rows; row++) {
        uint32_t i = row + ui_ctx->category_scroll_offset;
        int32_t y = 26 + (row * 10);
        if(i == app->current_category_index) {
            elements_frame(canvas, 0, y - 8, 124, 10);
        }
        if(has_favorites_row && i == 0) {
            char label[24];
            snprintf(label, sizeof(label), "* Favorites (%lu)", (unsigned long)app->settings.favorite_count);
            canvas_draw_str(canvas, 5, y, label);
        } else if(has_nfc_row && i == nfc_row_index) {
            canvas_draw_str(canvas, 5, y, "~ NFC Scan");
        } else if(has_subghz_row && i == subghz_row_index) {
            canvas_draw_str(canvas, 5, y, "^ Sub-GHz Scan");
        } else {
            const char* cat_id = ui_ctx->category_ids[i - synthetic_offset];
            bool is_home = app->settings.startup_category[0] &&
                strcmp(app->settings.startup_category, cat_id) == 0;
            char label[40];
            snprintf(label, sizeof(label), "%s%s", is_home ? "> " : "", cat_id);
            canvas_draw_str(canvas, 5, y, label);
        }
    }

    if(total_rows > 4) {
        char range[16];
        snprintf(
            range,
            sizeof(range),
            "%lu/%lu",
            (unsigned long)(app->current_category_index + 1),
            (unsigned long)total_rows);
        canvas_draw_str(canvas, 96, 62, range);
    }

    if(binding) {
        elements_button_left(canvas, "Cancel");
        elements_button_center(canvas, "Open");
    } else {
        elements_button_center(canvas, "Open");
        elements_button_right(canvas, "Settings");
    }
}

static void flipdeck_ui_draw_action_browser(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;

    char title[80];
    if(app->settings.show_icons && ui_ctx->current_category.icon[0] != '\0') {
        snprintf(
            title,
            sizeof(title),
            "[%s] %s",
            ui_ctx->current_category.icon,
            ui_ctx->current_category.name);
    } else {
        strncpy(title, ui_ctx->current_category.name, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';
    }
    flipdeck_ui_draw_header(canvas, title);

    canvas_set_font(canvas, FontSecondary);
    ui_ctx->action_scroll_offset = flipdeck_ui_scroll_offset_for_selection(
        app->selected_action_index,
        ui_ctx->action_scroll_offset,
        ui_ctx->current_category.action_count,
        4);

    for(uint32_t row = 0; row < 4 && row + ui_ctx->action_scroll_offset < ui_ctx->current_category.action_count; row++) {
        uint32_t i = row + ui_ctx->action_scroll_offset;
        int32_t y = 26 + (row * 10);
        if(i == app->selected_action_index) {
            elements_frame(canvas, 0, y - 8, 124, 10);
        }
        bool favorited = profile_manager_is_favorite(
            &app->settings, ui_ctx->action_category_ids[i], ui_ctx->current_category.actions[i].label);
        char line[68];
        snprintf(line, sizeof(line), "%s%s", favorited ? "*" : "", ui_ctx->current_category.actions[i].label);
        canvas_draw_str(canvas, 5, y, line);
    }

    // A pending send result takes over the footer as a transient toast; it is
    // cleared on the next button press in the action browser input handler.
    if(app->status_message[0] != '\0') {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 54, 128, 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, 62, app->status_message);
        canvas_set_color(canvas, ColorBlack);
        return;
    }

    if(ui_ctx->current_category.action_count > 4) {
        char range[16];
        snprintf(
            range,
            sizeof(range),
            "%lu/%lu",
            (unsigned long)(app->selected_action_index + 1),
            (unsigned long)ui_ctx->current_category.action_count);
        canvas_draw_str(canvas, 96, 62, range);
    }

    elements_button_left(canvas, "Back");
    elements_button_center(canvas, "Send");
    elements_button_right(canvas, "Fav");
}

static void flipdeck_ui_draw_confirm(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
    if(!action) {
        ui_ctx->app_ctx->state = FlipDeckState_ActionBrowser;
        return;
    }

    flipdeck_ui_draw_header(canvas, "Send Command?");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, action->label);
    canvas_draw_str(canvas, 5, 42, action->value);
    elements_button_left(canvas, "Cancel");
    elements_button_center(canvas, "Send");
}

static void flipdeck_ui_draw_nfc_scan(Canvas* canvas, FlipDeckUi* ui_ctx) {
    flipdeck_ui_draw_header(canvas, "NFC Scan");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, "Hold tag to the back");
    canvas_draw_str(canvas, 5, 40, "of the Flipper...");
    if(ui_ctx->nfc_scan_status[0]) {
        canvas_draw_str(canvas, 5, 52, ui_ctx->nfc_scan_status);
    }
    elements_button_left(canvas, "Cancel");
}

static void flipdeck_ui_draw_subghz_scan(Canvas* canvas, FlipDeckUi* ui_ctx) {
    flipdeck_ui_draw_header(canvas, "Sub-GHz Scan");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, "Press the remote's");
    canvas_draw_str(canvas, 5, 40, "button... (receive only)");
    if(ui_ctx->subghz_scan_status[0]) {
        canvas_draw_str(canvas, 5, 52, ui_ctx->subghz_scan_status);
    }
    elements_button_left(canvas, "Cancel");
}

#define FLIPDECK_SETTINGS_COUNT 5
#define FLIPDECK_SETTINGS_MAX_DELAY_MS 500
#define FLIPDECK_SETTINGS_DELAY_STEP_MS 50

// Render one settings row as "Label: value" into out.
static void flipdeck_ui_settings_row_text(FlipDeckApp* app, uint32_t index, char* out, size_t out_size) {
    switch(index) {
        case 0:
            snprintf(out, out_size, "Confirm send: %s", app->settings.confirm_before_send ? "On" : "Off");
            break;
        case 1:
            snprintf(out, out_size, "USB auto-check: %s", app->settings.auto_detect_usb ? "On" : "Off");
            break;
        case 2:
            snprintf(out, out_size, "Show icons: %s", app->settings.show_icons ? "On" : "Off");
            break;
        case 3:
            snprintf(out, out_size, "Show descr: %s", app->settings.show_descriptions ? "On" : "Off");
            break;
        case 4:
            snprintf(out, out_size, "Send delay: %lums", (unsigned long)app->settings.send_delay_ms);
            break;
        default:
            out[0] = '\0';
            break;
    }
}

static void flipdeck_ui_draw_settings(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    flipdeck_ui_draw_header(canvas, "Settings");

    canvas_set_font(canvas, FontSecondary);
    if(ui_ctx->settings_index >= FLIPDECK_SETTINGS_COUNT) ui_ctx->settings_index = 0;
    ui_ctx->settings_scroll_offset = flipdeck_ui_scroll_offset_for_selection(
        ui_ctx->settings_index, ui_ctx->settings_scroll_offset, FLIPDECK_SETTINGS_COUNT, 4);

    for(uint32_t row = 0; row < 4 && row + ui_ctx->settings_scroll_offset < FLIPDECK_SETTINGS_COUNT; row++) {
        uint32_t i = row + ui_ctx->settings_scroll_offset;
        int32_t y = 26 + (row * 10);
        if(i == ui_ctx->settings_index) {
            elements_frame(canvas, 0, y - 8, 124, 10);
        }
        char line[40];
        flipdeck_ui_settings_row_text(app, i, line, sizeof(line));
        canvas_draw_str(canvas, 5, y, line);
    }

    elements_button_left(canvas, "Save");
    elements_button_center(canvas, "Change");
}

static void flipdeck_ui_draw_long_snippet_warning(Canvas* canvas, FlipDeckUi* ui_ctx) {
    FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
    if(!action) {
        ui_ctx->app_ctx->state = FlipDeckState_ActionBrowser;
        return;
    }

    flipdeck_ui_draw_header(canvas, "Long Snippet");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 30, action->label);
    canvas_draw_str(canvas, 5, 42, "May take a while to send");
    elements_button_left(canvas, "Cancel");
    elements_button_center(canvas, "Send");
}

// Load category_ids[cat_index] into current_category and switch to the
// action browser. Shared by the category browser's OK handler and the
// startup-category auto-open path so both stay in sync.
static void flipdeck_ui_open_category(FlipDeckUi* ui_ctx, uint32_t cat_index) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    profile_manager_load_category(ui_ctx->category_ids[cat_index], &ui_ctx->current_category);
    strncpy(app->current_category_id, ui_ctx->category_ids[cat_index], 31);
    app->current_category_id[31] = '\0';
    for(uint32_t i = 0; i < ui_ctx->current_category.action_count; i++) {
        strncpy(ui_ctx->action_category_ids[i], app->current_category_id, 31);
        ui_ctx->action_category_ids[i][31] = '\0';
    }
    app->selected_action_index = 0;
    ui_ctx->action_scroll_offset = 0;
    app->state = FlipDeckState_ActionBrowser;
}

// Long-press OK on a real category row (not the Favorites row) pins/unpins
// it as the startup category: on the next launch the app opens straight
// into it, skipping the category browser entirely. Pressing it again on the
// already-pinned category clears it.
static void flipdeck_ui_toggle_startup_category(FlipDeckUi* ui_ctx, uint32_t cat_index) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    const char* id = ui_ctx->category_ids[cat_index];

    if(strcmp(app->settings.startup_category, id) == 0) {
        app->settings.startup_category[0] = '\0';
    } else {
        strncpy(app->settings.startup_category, id, sizeof(app->settings.startup_category) - 1);
        app->settings.startup_category[sizeof(app->settings.startup_category) - 1] = '\0';
    }
    profile_manager_save_settings(&app->settings);
}

static void flipdeck_ui_input_category_browser(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    bool binding = flipdeck_ui_is_binding(ui_ctx);

    bool has_favorites_row;
    uint32_t synthetic_offset;
    flipdeck_ui_category_rows(ui_ctx, &has_favorites_row, &synthetic_offset);
    bool has_nfc_row = !binding;
    bool has_subghz_row = !binding && app->subghz_available;
    uint32_t nfc_row_index = has_favorites_row ? 1u : 0u;
    uint32_t subghz_row_index = nfc_row_index + (has_nfc_row ? 1u : 0u);
    uint32_t total_rows = app->category_count + synthetic_offset;

    if(!binding && event->type == InputTypeLong && event->key == InputKeyOk) {
        if(total_rows == 0) return;
        if(app->current_category_index < synthetic_offset) return; // favorites/nfc rows can't be "home"
        flipdeck_ui_toggle_startup_category(ui_ctx, app->current_category_index - synthetic_offset);
        return;
    }

    if(event->type != InputTypeShort) return;

    switch(event->key) {
        case InputKeyUp:
            if(app->current_category_index > 0) app->current_category_index--;
            break;
        case InputKeyDown:
            if(app->current_category_index + 1 < total_rows) app->current_category_index++;
            break;
        case InputKeyOk:
            if(total_rows == 0) break;
            if(has_favorites_row && app->current_category_index == 0) {
                flipdeck_ui_open_favorites(ui_ctx);
                break;
            }
            if(has_nfc_row && app->current_category_index == nfc_row_index) {
                app->state = FlipDeckState_NfcScan;
                break;
            }
            if(has_subghz_row && app->current_category_index == subghz_row_index) {
                app->state = FlipDeckState_SubghzScan;
                break;
            }
            flipdeck_ui_open_category(ui_ctx, app->current_category_index - synthetic_offset);
            break;
        case InputKeyLeft:
            if(binding) {
                ui_ctx->pending_bind.kind = FlipDeckBindKind_None;
                app->current_category_index = 0;
            }
            break;
        case InputKeyRight:
            if(!binding) app->state = FlipDeckState_Settings;
            break;
        default:
            break;
    }
}

// Called once at startup. If a startup/"home" category is pinned and still
// exists, opens straight into it so the very first screen the user sees is
// its action list rather than the category browser.
bool flipdeck_ui_try_open_startup_category(void) {
    FlipDeckApp* app = ui.app_ctx;
    if(!app || app->settings.startup_category[0] == '\0') return false;

    for(uint32_t i = 0; i < app->category_count; i++) {
        if(strcmp(ui.category_ids[i], app->settings.startup_category) != 0) continue;
        flipdeck_ui_open_category(&ui, i);
        bool has_favorites_row;
        uint32_t synthetic_offset;
        flipdeck_ui_category_rows(&ui, &has_favorites_row, &synthetic_offset);
        app->current_category_index = i + synthetic_offset;
        return true;
    }
    return false;
}

// Flatten every currently-favorited action (which may span several
// categories) into a single synthetic category so the existing action
// browser / send flow can be reused unmodified.
static void flipdeck_ui_open_favorites(FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;

    memset(&ui_ctx->current_category, 0, sizeof(ui_ctx->current_category));
    strncpy(ui_ctx->current_category.name, "Favorites", sizeof(ui_ctx->current_category.name) - 1);
    strncpy(ui_ctx->current_category.id, FLIPDECK_FAVORITES_ROW_ID, sizeof(ui_ctx->current_category.id) - 1);

    // Scratch buffer for the source category of each favorite. Static: too
    // large (~7KB) for the 4096-byte FAP stack, and this is only ever called
    // from the single-threaded UI input path, so it's safe to reuse.
    static FlipDeckProfileCategory scratch;

    for(uint32_t f = 0; f < app->settings.favorite_count; f++) {
        FlipDeckFavorite* fav = &app->settings.favorites[f];
        if(!profile_manager_load_category(fav->category_id, &scratch)) continue;

        for(uint32_t a = 0; a < scratch.action_count; a++) {
            if(strcmp(scratch.actions[a].label, fav->label) != 0) continue;
            if(ui_ctx->current_category.action_count >= FLIPDECK_MAX_ACTIONS_PER_CATEGORY) break;

            uint32_t dest = ui_ctx->current_category.action_count;
            ui_ctx->current_category.actions[dest] = scratch.actions[a];
            strncpy(ui_ctx->action_category_ids[dest], fav->category_id, 31);
            ui_ctx->action_category_ids[dest][31] = '\0';
            ui_ctx->current_category.action_count++;
            break;
        }
    }

    strncpy(app->current_category_id, FLIPDECK_FAVORITES_ROW_ID, 31);
    app->current_category_id[31] = '\0';
    app->selected_action_index = 0;
    ui_ctx->action_scroll_offset = 0;
    app->state = FlipDeckState_ActionBrowser;
}

// Load the single action (category_id, label) into current_category as a
// one-entry synthetic category, the same way flipdeck_ui_open_favorites
// flattens several - used when a known NFC tag or Sub-GHz remote resolves
// to an action, so the existing SendConfirm/send flow can be reused
// unmodified. synthetic_category_name/id let each trigger source label the
// resulting screen distinctly ("NFC Tag" vs "Sub-GHz Remote").
// Returns false if the category or the labeled action no longer exists
// (e.g. the profile was edited/deleted since the mapping was bound).
static bool flipdeck_ui_load_single_action(
    FlipDeckUi* ui_ctx,
    const char* category_id,
    const char* label,
    const char* synthetic_category_name,
    const char* synthetic_category_id) {
    // Static for the same reason as open_favorites' scratch buffer.
    static FlipDeckProfileCategory scratch;
    if(!profile_manager_load_category(category_id, &scratch)) return false;

    for(uint32_t a = 0; a < scratch.action_count; a++) {
        if(strcmp(scratch.actions[a].label, label) != 0) continue;

        memset(&ui_ctx->current_category, 0, sizeof(ui_ctx->current_category));
        strncpy(
            ui_ctx->current_category.name,
            synthetic_category_name,
            sizeof(ui_ctx->current_category.name) - 1);
        strncpy(
            ui_ctx->current_category.id,
            synthetic_category_id,
            sizeof(ui_ctx->current_category.id) - 1);
        ui_ctx->current_category.actions[0] = scratch.actions[a];
        ui_ctx->current_category.action_count = 1;
        strncpy(ui_ctx->action_category_ids[0], category_id, 31);
        ui_ctx->action_category_ids[0][31] = '\0';

        strncpy(ui_ctx->app_ctx->current_category_id, category_id, 31);
        ui_ctx->app_ctx->current_category_id[31] = '\0';
        ui_ctx->app_ctx->selected_action_index = 0;
        ui_ctx->action_scroll_offset = 0;
        return true;
    }
    return false;
}

// Toggle the favorite status of the currently selected action and persist it.
// Works the same whether browsing a real category or the flattened
// Favorites list, since action_category_ids[] always tracks the true source.
static void flipdeck_ui_toggle_favorite(FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    uint32_t index = app->selected_action_index;
    FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
    if(!action) return;

    bool now_favorited = profile_manager_toggle_favorite(
        &app->settings, ui_ctx->action_category_ids[index], action->label);
    profile_manager_save_settings(&app->settings);
    snprintf(
        app->status_message,
        sizeof(app->status_message),
        now_favorited ? "Added to favorites" : "Removed from favorites");
}

// Finish binding: write (pending_bind.id_hex -> selected action) as a new
// mapping in whichever list pending_bind.kind selects, persist it, and
// re-arm that trigger's scan screen so several tokens can be bound in one
// sitting.
static void flipdeck_ui_bind_selected_action(FlipDeckUi* ui_ctx) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
    if(!action) return;

    if(ui_ctx->pending_bind.kind == FlipDeckBindKind_Nfc) {
        if(ui_ctx->nfc_tag_count >= FLIPDECK_MAX_NFC_TAGS) {
            snprintf(app->status_message, sizeof(app->status_message), "NFC tag list full");
        } else {
            FlipDeckNfcTag* tag = &ui_ctx->nfc_tags[ui_ctx->nfc_tag_count];
            strncpy(tag->uid_hex, ui_ctx->pending_bind.id_hex, sizeof(tag->uid_hex) - 1);
            tag->uid_hex[sizeof(tag->uid_hex) - 1] = '\0';
            strncpy(tag->category_id, app->current_category_id, sizeof(tag->category_id) - 1);
            tag->category_id[sizeof(tag->category_id) - 1] = '\0';
            strncpy(tag->label, action->label, sizeof(tag->label) - 1);
            tag->label[sizeof(tag->label) - 1] = '\0';
            ui_ctx->nfc_tag_count++;
            profile_manager_save_nfc_tags(ui_ctx->nfc_tags, ui_ctx->nfc_tag_count);
            snprintf(ui_ctx->nfc_scan_status, sizeof(ui_ctx->nfc_scan_status), "Tag bound!");
        }
        app->state = FlipDeckState_NfcScan;
    } else if(ui_ctx->pending_bind.kind == FlipDeckBindKind_Subghz) {
        if(ui_ctx->subghz_remote_count >= FLIPDECK_MAX_SUBGHZ_REMOTES) {
            snprintf(app->status_message, sizeof(app->status_message), "Sub-GHz list full");
        } else {
            FlipDeckSubghzRemote* remote = &ui_ctx->subghz_remotes[ui_ctx->subghz_remote_count];
            strncpy(remote->signature_hex, ui_ctx->pending_bind.id_hex, sizeof(remote->signature_hex) - 1);
            remote->signature_hex[sizeof(remote->signature_hex) - 1] = '\0';
            strncpy(remote->category_id, app->current_category_id, sizeof(remote->category_id) - 1);
            remote->category_id[sizeof(remote->category_id) - 1] = '\0';
            strncpy(remote->label, action->label, sizeof(remote->label) - 1);
            remote->label[sizeof(remote->label) - 1] = '\0';
            ui_ctx->subghz_remote_count++;
            profile_manager_save_subghz_remotes(ui_ctx->subghz_remotes, ui_ctx->subghz_remote_count);
            snprintf(ui_ctx->subghz_scan_status, sizeof(ui_ctx->subghz_scan_status), "Remote bound!");
        }
        app->state = FlipDeckState_SubghzScan;
    }

    ui_ctx->pending_bind.kind = FlipDeckBindKind_None;
    app->current_category_index = 0;
}

static void flipdeck_ui_input_action_browser(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    bool binding = flipdeck_ui_is_binding(ui_ctx);

    // Long-press OK is a quick-send shortcut: it skips the separate confirm
    // screen entirely (safety validation in flipdeck_ui_send_action still
    // applies). Short-press OK below still respects confirm_before_send.
    // Disabled while binding an NFC tag - OK there means "bind to this
    // action", not "send it".
    if(!binding && event->type == InputTypeLong && event->key == InputKeyOk) {
        app->status_message[0] = '\0';
        FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
        if(action) flipdeck_ui_send_action(ui_ctx, action);
        return;
    }

    if(event->type != InputTypeShort) return;

    // Dismiss any lingering send-result toast on the next interaction. A new
    // send below re-sets it.
    app->status_message[0] = '\0';

    uint32_t action_count = ui_ctx->current_category.action_count;
    if(action_count == 0 && event->key != InputKeyBack) return;

    switch(event->key) {
        case InputKeyUp:
            if(app->selected_action_index > 0) app->selected_action_index--;
            break;
        case InputKeyDown:
            if(app->selected_action_index + 1 < action_count) app->selected_action_index++;
            break;
        case InputKeyOk:
            if(binding) {
                flipdeck_ui_bind_selected_action(ui_ctx);
                break;
            }
            {
                FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
                if(!action) break;
                if(action->confirm || app->settings.confirm_before_send) {
                    app->state = FlipDeckState_SendConfirm;
                } else {
                    flipdeck_ui_send_action(ui_ctx, action);
                }
            }
            break;
        case InputKeyRight:
            if(!binding) flipdeck_ui_toggle_favorite(ui_ctx);
            break;
        case InputKeyBack:
            if(binding) ui_ctx->pending_bind.kind = FlipDeckBindKind_None;
            app->state = FlipDeckState_CategoryBrowser;
            app->selected_action_index = 0;
            ui_ctx->action_scroll_offset = 0;
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
            flipdeck_ui_send_action(ui_ctx, flipdeck_ui_get_selected_action(ui_ctx));
            app->state = FlipDeckState_ActionBrowser;
            break;
        case InputKeyBack:
            app->state = FlipDeckState_ActionBrowser;
            break;
        default:
            break;
    }
}

static void flipdeck_ui_input_nfc_scan(FlipDeckUi* ui_ctx, InputEvent* event) {
    if(event->type != InputTypeShort) return;
    if(event->key != InputKeyBack) return;

    nfc_bridge_stop_scan();
    ui_ctx->nfc_scan_active = false;
    ui_ctx->nfc_scan_status[0] = '\0';
    ui_ctx->app_ctx->state = FlipDeckState_CategoryBrowser;
}

static void flipdeck_ui_input_subghz_scan(FlipDeckUi* ui_ctx, InputEvent* event) {
    if(event->type != InputTypeShort) return;
    if(event->key != InputKeyBack) return;

    subghz_bridge_stop_scan();
    ui_ctx->subghz_scan_active = false;
    ui_ctx->subghz_scan_status[0] = '\0';
    ui_ctx->app_ctx->state = FlipDeckState_CategoryBrowser;
}

// Toggle a boolean setting, or step the send delay. Left decreases the delay,
// any other adjust key (OK/Right) increases it; bools just flip.
static void flipdeck_ui_settings_adjust(FlipDeckApp* app, uint32_t index, InputKey key) {
    switch(index) {
        case 0:
            app->settings.confirm_before_send = !app->settings.confirm_before_send;
            break;
        case 1:
            app->settings.auto_detect_usb = !app->settings.auto_detect_usb;
            break;
        case 2:
            app->settings.show_icons = !app->settings.show_icons;
            break;
        case 3:
            app->settings.show_descriptions = !app->settings.show_descriptions;
            break;
        case 4: {
            uint32_t delay = app->settings.send_delay_ms;
            if(key == InputKeyLeft) {
                delay = (delay >= FLIPDECK_SETTINGS_DELAY_STEP_MS)
                    ? delay - FLIPDECK_SETTINGS_DELAY_STEP_MS : 0;
            } else {
                delay = (delay + FLIPDECK_SETTINGS_DELAY_STEP_MS <= FLIPDECK_SETTINGS_MAX_DELAY_MS)
                    ? delay + FLIPDECK_SETTINGS_DELAY_STEP_MS : FLIPDECK_SETTINGS_MAX_DELAY_MS;
            }
            app->settings.send_delay_ms = delay;
            break;
        }
        default:
            break;
    }
}

static void flipdeck_ui_input_settings(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(event->type != InputTypeShort) return;

    switch(event->key) {
        case InputKeyUp:
            if(ui_ctx->settings_index > 0) ui_ctx->settings_index--;
            break;
        case InputKeyDown:
            if(ui_ctx->settings_index + 1 < FLIPDECK_SETTINGS_COUNT) ui_ctx->settings_index++;
            break;
        case InputKeyOk:
        case InputKeyLeft:
        case InputKeyRight:
            flipdeck_ui_settings_adjust(app, ui_ctx->settings_index, event->key);
            break;
        case InputKeyBack:
            // Persist on the way out so toggled settings survive a relaunch.
            profile_manager_save_settings(&app->settings);
            app->state = FlipDeckState_CategoryBrowser;
            break;
        default:
            break;
    }
}

static void flipdeck_ui_input_long_snippet_warning(FlipDeckUi* ui_ctx, InputEvent* event) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(event->type != InputTypeShort) return;

    switch(event->key) {
        case InputKeyOk: {
            FlipDeckAction* action = flipdeck_ui_get_selected_action(ui_ctx);
            if(!action) {
                app->state = FlipDeckState_ActionBrowser;
                break;
            }
            bool ok;
            if(action->target == FlipDeckActionTarget_WifiUart) {
                ok = uart_bridge_send_string(action->value);
            } else {
                ok = usb_hid_send_string(action->value);
            }
            flipdeck_ui_set_send_status(app, action, ok);
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

// Record the outcome of a send as a short status_message (drawn as a footer
// toast in the action browser) and, on success, apply the post-send delay:
// the action's own delay_ms if set, otherwise the global send_delay_ms.
static void flipdeck_ui_set_send_status(FlipDeckApp* app, FlipDeckAction* action, bool ok) {
    if(ok) {
        snprintf(app->status_message, sizeof(app->status_message), "Sent");
        uint32_t delay = action->delay_ms ? action->delay_ms : app->settings.send_delay_ms;
        if(delay) furi_delay_ms(delay);
        return;
    }

    if(action->target == FlipDeckActionTarget_WifiUart) {
        snprintf(
            app->status_message, sizeof(app->status_message),
            uart_bridge_is_connected() ? "Send failed" : "Dev board off");
    } else {
        snprintf(
            app->status_message, sizeof(app->status_message),
            usb_hid_is_connected() ? "Send failed" : "USB not connected");
    }
}

static void flipdeck_ui_send_action(FlipDeckUi* ui_ctx, FlipDeckAction* action) {
    FlipDeckApp* app = ui_ctx->app_ctx;
    if(!action) return;

    if(!profile_manager_validate_action(action)) {
        FURI_LOG_W("FlipDeck", "Blocked unsafe action");
        snprintf(app->status_message, sizeof(app->status_message), "Blocked: unsafe");
        return;
    }

    bool ok = false;
    if(action->type == FlipDeckActionType_Text) {
        if(strlen(action->value) > FLIPDECK_MAX_SNIPPET_LENGTH_WARN) {
            app->state = FlipDeckState_LongSnippetWarning;
            return;
        }
        if(action->target == FlipDeckActionTarget_WifiUart) {
            ok = uart_bridge_send_string(action->value);
        } else {
            ok = usb_hid_send_string(action->value);
        }
    } else if(action->type == FlipDeckActionType_Key) {
        ok = usb_hid_send_key(action->value);
    } else if(action->type == FlipDeckActionType_KeyCombo) {
        ok = usb_hid_send_key_combo(action->value);
    }

    flipdeck_ui_set_send_status(app, action, ok);
}
