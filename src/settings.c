/**
 * @file settings.c
 * @brief Settings implementation for JSON-based storage
 */

#include "settings.h"
#include "profile_manager.h"
#include <furi.h>
#include <furi_hal.h>

// Settings are now managed by profile_manager.c using JSON format
// This file provides backward compatibility wrappers

bool settings_load(FlipDeckSettings* settings) {
    return profile_manager_load_settings(settings);
}

bool settings_save(FlipDeckSettings* settings) {
    return profile_manager_save_settings(settings);
}

void settings_reset_to_defaults(FlipDeckSettings* settings) {
    settings->send_delay_ms = 100;
    settings->confirm_before_send = true;
    settings->auto_connect_usb = true;
}