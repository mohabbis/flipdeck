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
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FlipDeckApp* app_ctx;
    View* view;
    Gui* gui;
    char category_ids[FLIPDECK_MAX_CATEGORIES][32];
    FlipDeckProfileCategory current_category;
} FlipDeckUi;

static FlipDeckUi ui;

// Forward declarations
static void flipdeck_ui_draw_category_browser(Canvas* canvas);
static bool flipdeck_ui_input_category_browser(InputEvent* event);
static void flipdeck_ui_draw_action_browser(Canvas* canvas);
static bool flipdeck_ui_input_action_browser(InputEvent* event);
static void flipdeck_ui_draw_confirm(Canvas* canvas);
static bool flipdeck_ui_input_confirm(InputEvent* event);
static void flipdeck_ui_draw_settings(Canvas* canvas);
static bool flipdeck_ui_input_settings(InputEvent* event);
static void flipdeck_ui_send_action(FlipDeckAction* action);
static void flipdeck_ui_draw_long_snippet_warning(Canvas* canvas);
static bool flipdeck_ui_input_long_snippet_warning(InputEvent* event);

void flipdeck_ui_init(FlipDeckApp* app_ctx) {
    FURI_LOG_I("FlipDeck", "Initializing UI");
    
    ui.app_ctx = app_ctx;
    ui.gui = furi_record_open("gui");
    memset(&ui.current_category, 0, sizeof(FlipDeckProfileCategory));
    
    // Load category list
    profile_manager_list_categories(ui.category_ids, &app_ctx->category_count);
    
    // Allocate views
    ui.view = view_alloc();
    
    // Category browser view
    view_set_context(ui.view, &ui);
    view_allocate_model(ui.view, ViewModelTypeLocking, sizeof(FlipDeckUi));
    view_set_draw_callback(ui.view, (ViewDrawCallback)flipdeck_ui_draw_category_browser);
    view_set_input_callback(ui.view, flipdeck_ui_input_category_browser);
    
    // Add view to GUI
    gui_add_view(ui.gui, ui.view);
}

void flipdeck_ui_free(void) {
    FURI_LOG_I("FlipDeck", "Freeing UI");
    
    if(ui.gui) {
        gui_remove_view(ui.gui, ui.view);
        furi_record_close("gui");
    }
    view_free(ui.view);
}

void flipdeck_ui_handle_category_browser(void) {
    view_set_draw_callback(ui.view, (ViewDrawCallback)flipdeck_ui_draw_category_browser);
    view_set_input_callback(ui.view, flipdeck_ui_input_category_browser);
}

void flipdeck_ui_handle_action_browser(void) {
    view_set_draw_callback(ui.view, (ViewDrawCallback)flipdeck_ui_draw_action_browser);
    view_set_input_callback(ui.view, flipdeck_ui_input_action_browser);
}

void flipdeck_ui_handle_confirm(void) {
    view_set_draw_callback(ui.view, (ViewDrawCallback)flipdeck_ui_draw_confirm);
    view_set_input_callback(ui.view, flipdeck_ui_input_confirm);
}

void flipdeck_ui_handle_settings(void) {
    view_set_draw_callback(ui.view, (ViewDrawCallback)flipdeck_ui_draw_settings);
    view_set_input_callback(ui.view, flipdeck_ui_input_settings);
}

void flipdeck_ui_handle_long_snippet_warning(void) {
    view_set_draw_callback(ui.view, (ViewDrawCallback)flipdeck_ui_draw_long_snippet_warning);
    view_set_input_callback(ui.view, flipdeck_ui_input_long_snippet_warning);
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
            memcpy(&ui.current_category.actions[ui.app_ctx->selected_action_index], 
                   action, sizeof(FlipDeckAction));
            ui.app_ctx->state = FlipDeckState_LongSnippetWarning;
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
static void flipdeck_ui_draw_category_browser(Canvas* canvas) {
    FlipDeckApp* app = ui.app_ctx;
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "FlipDeck");
    
    canvas_draw_str(canvas, 88, 10, app->usb_connected ? "[OK]" : "[X]");
    
    elements_line(canvas, 0, 20, 128, 20);
    
    canvas_set_font(canvas, FontSecondary);
    for(uint32_t i = 0; i < app->category_count && i < 5; i++) {
        int y = 30 + (i * 12);
        if(i == app->current_category_index) {
            elements_frame(canvas, 0, y - 9, 127, 11);
        }
        canvas_draw_str(canvas, 5, y, ui.category_ids[i]);
    }
    
    canvas_draw_str(canvas, 0, 60, "OK:Select|Menu:Settings");
}

static bool flipdeck_ui_input_category_browser(InputEvent* event) {
    FlipDeckApp* app = ui.app_ctx;
    
    if(event->type != InputTypeShort) return false;
    
    switch(event->key) {
        case InputKeyUp:
            if(app->current_category_index > 0) app->current_category_index--;
            return true;
        case InputKeyDown:
            if(app->current_category_index < app->category_count - 1) app->current_category_index++;
            return true;
        case InputKeyOk:
            profile_manager_load_category(
                ui.category_ids[app->current_category_index], 
                &ui.current_category);
            strncpy(app->current_category_id, ui.category_ids[app->current_category_index], 31);
            app->state = FlipDeckState_ActionBrowser;
            return true;
        case InputKeyMenu:
            app->state = FlipDeckState_Settings;
            return true;
        default:
            break;
    }
    return false;
}

// Action browser - Profile screen
static void flipdeck_ui_draw_action_browser(Canvas* canvas) {
    FlipDeckApp* app = ui.app_ctx;
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, ui.current_category.name);
    
    elements_line(canvas, 0, 20, 128, 20);
    
    canvas_set_font(canvas, FontSecondary);
    for(uint32_t i = 0; i < ui.current_category.action_count && i < 5; i++) {
        int y = 30 + (i * 12);
        if(i == app->selected_action_index) {
            elements_frame(canvas, 0, y - 9, 127, 11);
        }
        canvas_draw_str(canvas, 5, y, ui.current_category.actions[i].label);
    }
    
    canvas_draw_str(canvas, 0, 60, "OK:Send|Back:Categories");
}

static bool flipdeck_ui_input_action_browser(InputEvent* event) {
    FlipDeckApp* app = ui.app_ctx;
    
    if(event->type != InputTypeShort) return false;
    
    uint32_t action_count = ui.current_category.action_count;
    if(action_count == 0) return false;
    
    switch(event->key) {
        case InputKeyUp:
            if(app->selected_action_index > 0) app->selected_action_index--;
            return true;
        case InputKeyDown:
            if(app->selected_action_index < action_count - 1) app->selected_action_index++;
            return true;
        case InputKeyOk:
            {
                FlipDeckAction* action = &ui.current_category.actions[app->selected_action_index];
                if(action->confirm) {
                    app->state = FlipDeckState_SendConfirm;
                } else {
                    flipdeck_ui_send_action(action);
                    snprintf(app->status_message, sizeof(app->status_message), "Sent!");
                }
            }
            return true;
        case InputKeyBack:
            app->state = FlipDeckState_CategoryBrowser;
            app->selected_action_index = 0;
            return true;
        default:
            break;
    }
    return false;
}

// Confirmation screen
static void flipdeck_ui_draw_confirm(Canvas* canvas) {
    FlipDeckAction* action = &ui.current_category.actions[ui.app_ctx->selected_action_index];
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "Send Command?");
    
    elements_line(canvas, 0, 20, 128, 20);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 35, action->label);
    
    // Truncate value if too long
    char display_value[64];
    strncpy(display_value, action->value, sizeof(display_value) - 1);
    display_value[sizeof(display_value) - 1] = '\0';
    canvas_draw_str(canvas, 5, 48, display_value);
    
    canvas_draw_str(canvas, 0, 60, "OK:Send|Back:Cancel");
}

static bool flipdeck_ui_input_confirm(InputEvent* event) {
    FlipDeckApp* app = ui.app_ctx;
    
    if(event->type != InputTypeShort) return false;
    
    switch(event->key) {
        case InputKeyOk:
            flipdeck_ui_send_action(&ui.current_category.actions[app->selected_action_index]);
            snprintf(app->status_message, sizeof(app->status_message), "Sent!");
            app->state = FlipDeckState_ActionBrowser;
            return true;
        case InputKeyBack:
            app->state = FlipDeckState_ActionBrowser;
            return true;
        default:
            break;
    }
    return false;
}

// Settings screen
static void flipdeck_ui_draw_settings(Canvas* canvas) {
    FlipDeckApp* app = ui.app_ctx;
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "Settings");
    
    elements_line(canvas, 0, 20, 128, 20);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 35, "USB:");
    canvas_draw_str(canvas, 60, 35, app->usb_connected ? "[OK]" : "[DISC]");
    
    canvas_draw_str(canvas, 5, 50, "Press Back to exit");
}

static bool flipdeck_ui_input_settings(InputEvent* event) {
    FlipDeckApp* app = ui.app_ctx;
    
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->state = FlipDeckState_CategoryBrowser;
        return true;
    }
    return false;
}

// Long snippet warning screen
static void flipdeck_ui_draw_long_snippet_warning(Canvas* canvas) {
    FlipDeckAction* action = &ui.current_category.actions[ui.app_ctx->selected_action_index];
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "Long Snippet!");
    
    elements_line(canvas, 0, 20, 128, 20);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 35, "Length:");
    
    char len_str[16];
    snprintf(len_str, sizeof(len_str), "%d chars", (int)strlen(action->value));
    canvas_draw_str(canvas, 50, 35, len_str);
    
    canvas_draw_str(canvas, 0, 55, "OK:Send Anyway");
    canvas_draw_str(canvas, 0, 65, "Back:Cancel");
}

static bool flipdeck_ui_input_long_snippet_warning(InputEvent* event) {
    FlipDeckApp* app = ui.app_ctx;
    
    if(event->type != InputTypeShort) return false;
    
    switch(event->key) {
        case InputKeyOk:
            // Send the stored action
            FlipDeckAction* action = &ui.current_category.actions[app->selected_action_index];
            usb_hid_send_string(action->value);
            snprintf(app->status_message, sizeof(app->status_message), "Sent!");
            app->state = FlipDeckState_ActionBrowser;
            return true;
        case InputKeyBack:
            app->state = FlipDeckState_ActionBrowser;
            return true;
        default:
            break;
    }
    return false;
}