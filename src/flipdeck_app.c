/**
 * @file flipdeck_app.c
 * @brief FlipDeck Application Entry Point
 */

#include "flipdeck_app.h"
#include "flipdeck_ui.h"
#include "profile_manager.h"
#include "usb_hid.h"
#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static FlipDeckApp* g_app_ctx = NULL;

// Forward declarations
static void flipdeck_load_settings(void);
static void flipdeck_load_categories(void);

bool flipdeck_app_init(void* p) {
    (void)p;
    FURI_LOG_I("FlipDeck", "Initializing FlipDeck application");
    
    // Allocate app context
    g_app_ctx = malloc(sizeof(FlipDeckApp));
    if(!g_app_ctx) {
        FURI_LOG_E("FlipDeck", "Failed to allocate app context");
        return false;
    }
    
    // Initialize context
    memset(g_app_ctx, 0, sizeof(FlipDeckApp));
    g_app_ctx->state = FlipDeckState_CategoryBrowser;
    g_app_ctx->selected_action_index = 0;
    g_app_ctx->usb_connected = false;
    
    // Load settings
    flipdeck_load_settings();
    
    // Load categories from SD card
    flipdeck_load_categories();
    
    // Check USB connection
    g_app_ctx->usb_connected = usb_hid_is_connected();
    
    // Create UI
    flipdeck_ui_init(g_app_ctx);
    
    FURI_LOG_I("FlipDeck", "Initialization complete");
    return true;
}

static void flipdeck_load_settings(void) {
    if(profile_manager_load_settings(&g_app_ctx->settings)) {
        FURI_LOG_I("FlipDeck", "Settings loaded from SD card");
    } else {
        FURI_LOG_I("FlipDeck", "Using default settings");
        g_app_ctx->settings.send_delay_ms = 100;
        g_app_ctx->settings.confirm_before_send = true;
        g_app_ctx->settings.auto_detect_usb = true;
    }
}

static void flipdeck_load_categories(void) {
    if(profile_manager_list_categories(NULL, &g_app_ctx->category_count)) {
        FURI_LOG_I("FlipDeck", "Found %" PRIu32 " categories", g_app_ctx->category_count);
    } else {
        FURI_LOG_W("FlipDeck", "No categories found");
        g_app_ctx->category_count = 0;
    }
}

void flipdeck_app_free(void* p) {
    (void)p;
    FURI_LOG_I("FlipDeck", "Shutting down FlipDeck application");
    
    if(g_app_ctx) {
        flipdeck_ui_free();
        free(g_app_ctx);
        g_app_ctx = NULL;
    }
}

void flipdeck_app_loop(void* p) {
    (void)p;
    
    // Poll USB connection status
    g_app_ctx->usb_connected = usb_hid_is_connected();
    
    switch(g_app_ctx->state) {
        case FlipDeckState_Idle:
            break;
            
        case FlipDeckState_CategoryBrowser:
            flipdeck_ui_handle_category_browser();
            break;
            
        case FlipDeckState_ActionBrowser:
            flipdeck_ui_handle_action_browser();
            break;
            
        case FlipDeckState_ActionDetail:
            flipdeck_ui_handle_confirm();
            break;
            
        case FlipDeckState_SendConfirm:
            flipdeck_ui_handle_confirm();
            break;
            
        case FlipDeckState_LongSnippetWarning:
            flipdeck_ui_handle_long_snippet_warning();
            break;
            
        case FlipDeckState_Settings:
            flipdeck_ui_handle_settings();
            break;
    }
}

FlipDeckApp* flipdeck_app_get_context(void) {
    return g_app_ctx;
}

void flipdeck_app_set_state(FlipDeckState new_state) {
    if(g_app_ctx) {
        g_app_ctx->state = new_state;
    }
}

// Main entry point for uFBT
int32_t flipdeck_app(void* p) {
    if(!flipdeck_app_init(p)) {
        return -1;
    }

    while(flipdeck_app_get_context()) {
        flipdeck_app_loop(p);
        furi_delay_ms(50);
    }

    flipdeck_app_free(p);
    return 0;
}