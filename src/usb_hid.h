/**
 * @file usb_hid.h
 * @brief USB HID communication layer for FlipDeck
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <furi.h>
#include <furi_hal.h>

/**
 * @brief Check if USB HID is connected and ready
 * @return true if USB is connected and available
 */
bool usb_hid_is_connected(void);

/**
 * @brief Send a keyboard string over USB HID
 * @param text String to send
 * @return true if sent successfully
 */
bool usb_hid_send_string(const char* text);

/**
 * @brief Send a single key press
 * @param keyName Key name string (e.g., "ENTER", "SPACE", "A")
 * @return true if sent successfully
 */
bool usb_hid_send_key(const char* keyName);

/**
 * @brief Send a key combination (e.g., "CTRL+C", "SHIFT+F5")
 * @param combo Key combination string
 * @return true if sent successfully
 */
bool usb_hid_send_key_combo(const char* combo);

#endif // USB_HID_H
