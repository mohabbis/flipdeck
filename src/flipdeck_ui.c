/**
 * @file flipdeck_ui.c
 * @brief FlipDeck User Interface Implementation
 */

#include "flipdeck_ui.h"
#include "flipdeck_app.h"
#include "profile_manager.h"
#include "usb_hid.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/view.h>
#include <gui/view_manager.h>

typedef struct {
    FlipDeckApp* app_ctx;
    View* view;
    ViewDispatcher* view_dispenser;
    char category_ids[FLIPDECK_MAX_CATEGORIES][32];
    FlipDeckProfileCategory current_category;
} FlipDeckUi;

static FlipDeckUi ui;

// Forward declarations
static void flipdeck_ui_draw_category_browser(void* ctx, VContext vctx);
static bool flipdeck_ui_input_category_browser(void* ctx, InputEvent* event);
static void flipdeck_ui_draw_action_browser(void* ctx, VContext vctx);
static bool flipdeck_ui_input_action_browser(void* ctx, InputEvent* event);
static void flipdeck_ui_draw_confirm(void* ctx, VContext vctx);
static bool flipdeck_ui_input_confirm(void* ctx, InputEvent* event);
static void flipdeck_ui_draw_settings(void* ctx, VContext vctx);
static bool flipdeck_ui_input_settings(void* ctx, InputEvent* event);
static void flipdeck_ui_send_action(FlipDeckAction* action);
static void flipdeck_ui_draw_long_snippet_warning(void* ctx, VContext vctx);
static bool flipdeck_ui_input_long_snippet_warning(void* ctx, InputEvent* event);

void flipdeck_ui_init(FlipDeckApp* app_ctx) {
    FURI_LOG_I("FlipDeck", "Initializing UI");
    
    ui.app_ctx = app_ctx;
    memset(&ui.current_category, 0, sizeof(FlipDeckProfileCategory));
    
    // Load category list
    profile_manager_list_categories(ui.category_ids, &app_ctx->category_count);
    
    // Allocate views
    ui.view = view_alloc();
    ui.view_dispenser = view_dispatcher_alloc();
    
    // Category browser view
    view_set_callback_context(ui.view, &ui);
    view_set_draw_callback(ui.view, flipdeck_ui_draw_category_browser);
    view_set_input_callback(ui.view, flipdeck_ui_input_category_browser);
    view_dispatcher_add_view(ui.view_dispenser, 0, ui.view);
}

void flipdeck_ui_free(void) {
    FURI_LOG_I("FlipDeck", "Freeing UI");
    
    view_dispatcher_free(ui.view_dispenser);
    view_free(ui.view);
}

void flipdeck_ui_handle_category_browser(furi_t furi) {
    view_dispatcher_attach_view(ui.view_dispenser, furi_gui_view_manager(furi));
}

void flipdeck_ui_handle_action_browser(furi_t furi) {
    view_set_draw_callback(ui.view, flipdeck_ui_draw_action_browser);
    view_set_input_callback(ui.view, flipdeck_ui_input_action_browser);
    view_dispatcher_attach_view(ui.view_dispenser, furi_gui_view_manager(furi));
}

void flipdeck_ui_handle_confirm(furi_t furi) {
    view_set_draw_callback(ui.view, flipdeck_ui_draw_confirm);
    view_set_input_callback(ui.view, flipdeck_ui_input_confirm);
    view_dispatcher_attach_view(ui.view_dispenser, furi_gui_view_manager(furi));
}

void flipdeck_ui_handle_settings(furi_t furi) {
    view_set_draw_callback(ui.view, flipdeck_ui_draw_settings);
    view_set_input_callback(ui.view, flipdeck_ui_input_settings);
    view_dispatcher_attach_view(ui.view_dispenser, furi_gui_view_manager(furi));
}

void flipdeck_ui_handle_long_snippet_warning(furi_t furi) {
    view_set_draw_callback(ui.view, flipdeck_ui_draw_long_snippet_warning);
    view_set_input_callback(ui.view, flipdeck_ui_input_long_snippet_warning);
    view_dispatcher_attach_view(ui.view_dispenser, furi_gui_view_manager(furi));
}

// Send action to USB HID
static void flipdeck_ui_send_action(FlipDeckAction* action) {
    if(!action) return;
    
    // Safety validation
    if(!profile_manager_validate_action(action)) {
        FURI_LOG_W("FlipDeck", "Blocked unsafe action");
        return;
    }
    
    if(action->type == FlipDeckActionType_Text) {
        // Check if snippet is too long and warn
        uint32_t len = strlen(action->value);
        if(len > FLIPDECK_MAX_SNIPPET_LENGTH_WARN) {
            // Store the action and show warning
            FlipDeckUi* ui_ctx = &ui;
            memcpy(&ui_ctx->current_category.actions[ui_ctx->app_ctx->selected_action_index], 
                   action, sizeof(FlipDeckAction));
            ui_ctx->app_ctx->state = FlipDeckState_LongSnippetWarning;
            return;
        }
        usb_hid_send_string(action->value);
    } else if(action->type == FlipDeckActionType_Key) {
        usb_hid_send_key(action->value);
    } else if(action->type == FlipDeckActionType_KeyCombo) {
        usb_hid_send_key_combo(action->value);
    }
}

// Category browser - Main screen
static void flipdeck_ui_draw_category_browser(void* ctx, VContext vctx) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckApp* app = ui_ctx->app_ctx;
    
    canvas_set_font(vctx, CanvasFontMedium);
    canvas_draw_str(vctx, 0, 10, "FlipDeck");
    
    canvas_set_font(vctx, CanvasFontSmall);
    canvas_draw_str(vctx, 88, 10, app->usb_connected ? "[OK]" : "[X]");
    
    canvas_draw_line(vctx, 0, 20, 128, 20);
    
    canvas_set_font(vctx, CanvasFontSmall);
    for(uint32_t i = 0; i < app->category_count && i < 5; i++) {
        int y = 30 + (i * 10);
        if(i == app->current_category_index) {
            canvas_invert_rectangle(vctx, 0, y - 2, 128, 8);
        }
        canvas_draw_str(vctx, 5, y, ui_ctx->category_ids[i]);
    }
    
    canvas_set_font(vctx, CanvasFontSmall);
    canvas_draw_str(vctx, 0, 60, "OK:Select | MENU:Settings");
}

static bool flipdeck_ui_input_category_browser(void* ctx, InputEvent* event) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckApp* app = ui_ctx->app_ctx;
    
    if(event->type != InputType_Click) return false;
    
    switch(event->key) {
        case InputKey_Up:
            if(app->current_category_index > 0) app->current_category_index--;
            return true;
        case InputKey_Down:
            if(app->current_category_index < app->category_count - 1) app->current_category_index++;
            return true;
        case InputKey_OK:
            profile_manager_load_category(
                ui_ctx->category_ids[app->current_category_index], 
                &ui_ctx->current_category);
            strncpy(app->current_category_id, ui_ctx->category_ids[app->current_category_index], 31);
            app->state = FlipDeckState_ActionBrowser;
            return true;
        case InputKey_Menu:
            app->state = FlipDeckState_Settings;
            return true;
    }
    return false;
}

// Action browser - Profile screen
static void flipdeck_ui_draw_action_browser(void* ctx, VContext vctx) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckApp* app = ui_ctx->app_ctx;
    
    canvas_set_font(vctx, CanvasFontMedium);
    canvas_draw_str(vctx, 0, 10, ui_ctx->current_category.name);
    
    canvas_draw_line(vctx, 0, 20, 128, 20);
    
    canvas_set_font(vctx, CanvasFontSmall);
    for(uint32_t i = 0; i < ui_ctx->current_category.action_count && i < 5; i++) {
        int y = 30 + (i * 10);
        if(i == app->selected_action_index) {
            canvas_invert_rectangle(vctx, 0, y - 2, 128, 8);
        }
        canvas_draw_str(vctx, 5, y, ui_ctx->current_category.actions[i].label);
    }
    
    canvas_set_font(vctx, CanvasFontSmall);
    canvas_draw_str(vctx, 0, 60, "OK:Send | BACK:Categories");
}

static bool flipdeck_ui_input_action_browser(void* ctx, InputEvent* event) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckApp* app = ui_ctx->app_ctx;
    
    if(event->type != InputType_Click) return false;
    
    uint32_t action_count = ui_ctx->current_category.action_count;
    if(action_count == 0) return false;
    
    switch(event->key) {
        case InputKey_Up:
            if(app->selected_action_index > 0) app->selected_action_index--;
            return true;
        case InputKey_Down:
            if(app->selected_action_index < action_count - 1) app->selected_action_index++;
            return true;
        case InputKey_OK:
            {
                FlipDeckAction* action = &ui_ctx->current_category.actions[app->selected_action_index];
                if(action->confirm) {
                    app->state = FlipDeckState_SendConfirm;
                } else {
                    flipdeck_ui_send_action(action);
                    snprintf(app->status_message, sizeof(app->status_message), "Sent!");
                }
            }
            return true;
        case InputKey_Back:
            app->state = FlipDeckState_CategoryBrowser;
            app->selected_action_index = 0;
            return true;
    }
    return false;
}

// Confirmation screen
static void flipdeck_ui_draw_confirm(void* ctx, VContext vctx) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckAction* action = &ui_ctx->current_category.actions[ui_ctx->app_ctx->selected_action_index];
    
    canvas_set_font(vctx, CanvasFontMedium);
    canvas_draw_str(vctx, 0, 10, "Send Command?");
    
    canvas_draw_line(vctx, 0, 20, 128, 20);
    
    canvas_set_font(vctx, CanvasFontSmall);
    canvas_draw_str(vctx, 5, 35, action->label);
    canvas_draw_str(vctx, 5, 50, action->value);
    
    canvas_draw_str(vctx, 0, 60, "[YES] Send | [NO] Cancel");
}

static bool flipdeck_ui_input_confirm(void* ctx, InputEvent* event) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckApp* app = ui_ctx->app_ctx;
    
    if(event->type != InputType_Click) return false;
    
    switch(event->key) {
        case InputKey_OK:
            flipdeck_ui_send_action(&ui_ctx->current_category.actions[app->selected_action_index]);
            snprintf(app->status_message, sizeof(app->status_message), "Sent!");
            app->state = FlipDeckState_ActionBrowser;
            return true;
        case InputKey_Back:
        case InputKey_Cancel:
            app->state = FlipDeckState_ActionBrowser;
            return true;
    }
    return false;
}

// Settings screen
static void flipdeck_ui_draw_settings(void* ctx, VContext vctx) {
    FlipDeckApp* app = ((FlipDeckUi*)ctx)->app_ctx;
    
    canvas_set_font(vctx, CanvasFontMedium);
    canvas_draw_str(vctx, 0, 10, "Settings");
    
    canvas_draw_line(vctx, 0, 20, 128, 20);
    
    canvas_set_font(vctx, CanvasFontSmall);
    canvas_draw_str(vctx, 5, 35, "USB:");
    canvas_draw_str(vctx, 88, 35, app->usb_connected ? "[OK]" : "[DISC]");
    
    canvas_draw_str(vctx, 5, 50, "Back to Categories");
}

static bool flipdeck_ui_input_settings(void* ctx, InputEvent* event) {
    FlipDeckApp* app = ((FlipDeckUi*)ctx)->app_ctx;
    
    if(event->type == InputType_Click && event->key == InputKey_Back) {
        app->state = FlipDeckState_CategoryBrowser;
        return true;
    }
    return false;
}

// Long snippet warning screen
static void flipdeck_ui_draw_long_snippet_warning(void* ctx, VContext vctx) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckAction* action = &ui_ctx->current_category.actions[ui_ctx->app_ctx->selected_action_index];
    
    canvas_set_font(vctx, CanvasFontMedium);
    canvas_draw_str(vctx, 0, 10, "Long Snippet!");
    
    canvas_draw_line(vctx, 0, 20, 128, 20);
    
    canvas_set_font(vctx, CanvasFontSmall);
    canvas_draw_str(vctx, 5, 35, "Length:");
    canvas_draw_str(vctx, 40, 35, action->value);
    
    canvas_draw_str(vctx, 0, 55, "[YES] Send Anyway");
    canvas_draw_str(vctx, 0, 65, "[NO] Cancel");
}

static bool flipdeck_ui_input_long_snippet_warning(void* ctx, InputEvent* event) {
    FlipDeckUi* ui_ctx = (FlipDeckUi*)ctx;
    FlipDeckApp* app = ui_ctx->app_ctx;
    
    if(event->type != InputType_Click) return false;
    
    switch(event->key) {
        case InputKey_OK:
            // Send the stored action
            FlipDeckAction* action = &ui_ctx->current_category.actions[app->selected_action_index];
            usb_hid_send_string(action->value);
            snprintf(app->status_message, sizeof(app->status_message), "Sent!");
            app->state = FlipDeckState_ActionBrowser;
            return true;
        case InputKey_Back:
        case InputKey_Cancel:
            app->state = FlipDeckState_ActionBrowser;
            return true;
    }
    return false;
}