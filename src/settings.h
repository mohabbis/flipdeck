/**
 * @file settings.h
 * @brief Settings management for FlipDeck
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <furi.h>

/**
 * @brief Settings structure
 */
typedef struct {
    uint32_t send_delay_ms;
    bool confirm_before_send;
    bool auto_connect_usb;
} FlipDeckSettings;

/**
 * @brief Load settings from storage
 * @param settings Settings structure to populate
 * @return true if loaded successfully
 */
bool settings_load(FlipDeckSettings* settings);

/**
 * @brief Save settings to storage
 * @param settings Settings to save
 * @return true if saved successfully
 */
bool settings_save(FlipDeckSettings* settings);

/**
 * @brief Reset settings to defaults
 * @param settings Settings structure to populate
 */
void settings_reset_to_defaults(FlipDeckSettings* settings);

#endif // SETTINGS_H