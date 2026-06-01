/**
 * @file usb_hid.c
 * @brief USB HID communication implementation
 */

#include "usb_hid.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>

// USB HID state
static bool s_usb_connected = false;

// Key name to HID keycode mapping
static uint8_t key_name_to_code(const char* name) {
    if(strcmp(name, "A") == 0) return 4;
    if(strcmp(name, "B") == 0) return 5;
    if(strcmp(name, "C") == 0) return 6;
    if(strcmp(name, "D") == 0) return 7;
    if(strcmp(name, "E") == 0) return 8;
    if(strcmp(name, "F") == 0) return 9;
    if(strcmp(name, "G") == 0) return 10;
    if(strcmp(name, "H") == 0) return 11;
    if(strcmp(name, "I") == 0) return 12;
    if(strcmp(name, "J") == 0) return 13;
    if(strcmp(name, "K") == 0) return 14;
    if(strcmp(name, "L") == 0) return 15;
    if(strcmp(name, "M") == 0) return 16;
    if(strcmp(name, "N") == 0) return 17;
    if(strcmp(name, "O") == 0) return 18;
    if(strcmp(name, "P") == 0) return 19;
    if(strcmp(name, "Q") == 0) return 20;
    if(strcmp(name, "R") == 0) return 21;
    if(strcmp(name, "S") == 0) return 22;
    if(strcmp(name, "T") == 0) return 23;
    if(strcmp(name, "U") == 0) return 24;
    if(strcmp(name, "V") == 0) return 25;
    if(strcmp(name, "W") == 0) return 26;
    if(strcmp(name, "X") == 0) return 27;
    if(strcmp(name, "Y") == 0) return 28;
    if(strcmp(name, "Z") == 0) return 29;
    if(strcmp(name, "0") == 0) return 39;
    if(strcmp(name, "1") == 0) return 40;
    if(strcmp(name, "2") == 0) return 41;
    if(strcmp(name, "3") == 0) return 42;
    if(strcmp(name, "4") == 0) return 43;
    if(strcmp(name, "5") == 0) return 44;
    if(strcmp(name, "6") == 0) return 45;
    if(strcmp(name, "7") == 0) return 46;
    if(strcmp(name, "8") == 0) return 47;
    if(strcmp(name, "9") == 0) return 48;
    if(strcmp(name, "ENTER") == 0) return 40;
    if(strcmp(name, "ESCAPE") == 0) return 41;
    if(strcmp(name, "BACKSPACE") == 0) return 42;
    if(strcmp(name, "TAB") == 0) return 43;
    if(strcmp(name, "SPACE") == 0) return 49;
    if(strcmp(name, "RIGHT") == 0) return 79;
    if(strcmp(name, "LEFT") == 0) return 80;
    if(strcmp(name, "DOWN") == 0) return 81;
    if(strcmp(name, "UP") == 0) return 82;
    if(strcmp(name, "F5") == 0) return 65;
    if(strcmp(name, "F1") == 0) return 58;
    if(strcmp(name, "F2") == 0) return 59;
    if(strcmp(name, "F3") == 0) return 60;
    if(strcmp(name, "F4") == 0) return 61;
    if(strcmp(name, "F6") == 0) return 66;
    if(strcmp(name, "F7") == 0) return 67;
    if(strcmp(name, "F8") == 0) return 68;
    if(strcmp(name, "F9") == 0) return 69;
    if(strcmp(name, "F10") == 0) return 70;
    if(strcmp(name, "F11") == 0) return 71;
    if(strcmp(name, "F12") == 0) return 72;
    // Special characters
    if(strcmp(name, "`") == 0 || strcmp(name, "~") == 0) return 52;
    if(strcmp(name, "-") == 0 || strcmp(name, "_") == 0) return 55;
    if(strcmp(name, "=") == 0 || strcmp(name, "+") == 0) return 56;
    return 0;
}

// Modifier key mappings
static uint8_t modifier_name_to_code(const char* name) {
    // Support both uppercase and lowercase
    char upper[16];
    for(int i = 0; name[i] && i < 15; i++) {
        upper[i] = toupper(name[i]);
    }
    upper[15] = '\0';
    
    if(strcmp(upper, "CTRL") == 0 || strcmp(upper, "LEFTCTRL") == 0) return 1;
    if(strcmp(upper, "SHIFT") == 0 || strcmp(upper, "LEFTSHIFT") == 0) return 2;
    if(strcmp(upper, "ALT") == 0 || strcmp(upper, "LEFTALT") == 0) return 4;
    if(strcmp(upper, "GUI") == 0 || strcmp(upper, "LEFTGUI") == 0 || strcmp(upper, "WIN") == 0 || strcmp(upper, "CMD") == 0) return 8;
    return 0;
}

bool usb_hid_is_connected(void) {
    s_usb_connected = furi_hal_usb_is_enabled();
    return s_usb_connected;
}

bool usb_hid_send_string(const char* text) {
    if(!s_usb_connected) {
        FURI_LOG_W("FlipDeck", "USB not connected");
        return false;
    }
    
    FURI_LOG_I("FlipDeck", "Sending string: %s", text);
    
    for(uint32_t i = 0; text[i] != '\0'; i++) {
        char single[2] = {text[i], '\0'};
        if(!usb_hid_send_key(single)) {
            return false;
        }
        furi_delay_ms(5);
    }
    
    return true;
}

bool usb_hid_send_key(const char* keyName) {
    if(!s_usb_connected) {
        return false;
    }
    
    uint8_t key = key_name_to_code(keyName);
    if(key == 0) {
        FURI_LOG_W("FlipDeck", "Unknown key: %s", keyName);
        return false;
    }
    
    // Create and send keyboard report
    furi_hal_usb_hid_keyboard_report_t report = {0};
    report.keys[0] = key;
    
    furi_hal_usb_hid_send_keyboard_report(&report);
    furi_delay_ms(50);  // Press duration
    
    // Release key
    furi_hal_usb_hid_send_keyboard_report(&report);
    furi_delay_ms(10);
    
    return true;
}

bool usb_hid_send_key_combo(const char* combo) {
    if(!s_usb_connected) {
        return false;
    }
    
    FURI_LOG_I("FlipDeck", "Sending key combo: %s", combo);
    
    // Parse combo (e.g., "CTRL+SHIFT+F5")
    char work[64];
    strncpy(work, combo, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    
    uint8_t modifiers = 0;
    char* key_part = work;
    char* plus = strchr(work, '+');
    
    while(plus) {
        *plus = '\0';
        modifiers |= modifier_name_to_code(work);
        work = plus + 1;
        plus = strchr(work, '+');
    }
    key_part = work;
    
    uint8_t key = key_name_to_code(key_part);
    if(key == 0) {
        FURI_LOG_W("FlipDeck", "Unknown key in combo: %s", key_part);
        return false;
    }
    
    // Send with modifiers
    furi_hal_usb_hid_keyboard_report_t report = {0};
    report.modifiers = modifiers;
    report.keys[0] = key;
    
    furi_hal_usb_hid_send_keyboard_report(&report);
    furi_delay_ms(50);
    
    // Release
    report.modifiers = 0;
    report.keys[0] = 0;
    furi_hal_usb_hid_send_keyboard_report(&report);
    
    return true;
}

void usb_hid_send_report(furi_hal_usb_hid_keyboard_report_t* report) {
    furi_hal_usb_hid_send_keyboard_report(report);
}