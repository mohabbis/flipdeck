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

// Key name to HID keycode mapping (using HID_KEYBOARD_* constants from firmware)
static uint16_t key_name_to_code(const char* name) {
    // Support single characters directly (a-z, A-Z, 0-9)
    if(strlen(name) == 1) {
        char c = name[0];
        if(c >= 'A' && c <= 'Z') return c - 'A' + HID_KEYBOARD_A;
        if(c >= 'a' && c <= 'z') return c - 'a' + HID_KEYBOARD_A;
        if(c >= '0' && c <= '9') return c - '0' + HID_KEYBOARD_0;
        if(c == '`') return HID_KEYBOARD_GRAVE_ACCENT;
        if(c == '-') return HID_KEYBOARD_MINUS;
        if(c == '=') return HID_KEYBOARD_EQUAL_SIGN;
        return 0;
    }
    
    // Full key names (uppercase)
    if(strcmp(name, "A") == 0) return HID_KEYBOARD_A;
    if(strcmp(name, "B") == 0) return HID_KEYBOARD_B;
    if(strcmp(name, "C") == 0) return HID_KEYBOARD_C;
    if(strcmp(name, "D") == 0) return HID_KEYBOARD_D;
    if(strcmp(name, "E") == 0) return HID_KEYBOARD_E;
    if(strcmp(name, "F") == 0) return HID_KEYBOARD_F;
    if(strcmp(name, "G") == 0) return HID_KEYBOARD_G;
    if(strcmp(name, "H") == 0) return HID_KEYBOARD_H;
    if(strcmp(name, "I") == 0) return HID_KEYBOARD_I;
    if(strcmp(name, "J") == 0) return HID_KEYBOARD_J;
    if(strcmp(name, "K") == 0) return HID_KEYBOARD_K;
    if(strcmp(name, "L") == 0) return HID_KEYBOARD_L;
    if(strcmp(name, "M") == 0) return HID_KEYBOARD_M;
    if(strcmp(name, "N") == 0) return HID_KEYBOARD_N;
    if(strcmp(name, "O") == 0) return HID_KEYBOARD_O;
    if(strcmp(name, "P") == 0) return HID_KEYBOARD_P;
    if(strcmp(name, "Q") == 0) return HID_KEYBOARD_Q;
    if(strcmp(name, "R") == 0) return HID_KEYBOARD_R;
    if(strcmp(name, "S") == 0) return HID_KEYBOARD_S;
    if(strcmp(name, "T") == 0) return HID_KEYBOARD_T;
    if(strcmp(name, "U") == 0) return HID_KEYBOARD_U;
    if(strcmp(name, "V") == 0) return HID_KEYBOARD_V;
    if(strcmp(name, "W") == 0) return HID_KEYBOARD_W;
    if(strcmp(name, "X") == 0) return HID_KEYBOARD_X;
    if(strcmp(name, "Y") == 0) return HID_KEYBOARD_Y;
    if(strcmp(name, "Z") == 0) return HID_KEYBOARD_Z;
    if(strcmp(name, "0") == 0) return HID_KEYBOARD_0;
    if(strcmp(name, "1") == 0) return HID_KEYBOARD_1;
    if(strcmp(name, "2") == 0) return HID_KEYBOARD_2;
    if(strcmp(name, "3") == 0) return HID_KEYBOARD_3;
    if(strcmp(name, "4") == 0) return HID_KEYBOARD_4;
    if(strcmp(name, "5") == 0) return HID_KEYBOARD_5;
    if(strcmp(name, "6") == 0) return HID_KEYBOARD_6;
    if(strcmp(name, "7") == 0) return HID_KEYBOARD_7;
    if(strcmp(name, "8") == 0) return HID_KEYBOARD_8;
    if(strcmp(name, "9") == 0) return HID_KEYBOARD_9;
    if(strcmp(name, "ENTER") == 0) return HID_KEYBOARD_RETURN;
    if(strcmp(name, "ESCAPE") == 0) return HID_KEYBOARD_ESCAPE;
    if(strcmp(name, "BACKSPACE") == 0) return HID_KEYBOARD_DELETE;
    if(strcmp(name, "TAB") == 0) return HID_KEYBOARD_TAB;
    if(strcmp(name, "SPACE") == 0) return HID_KEYBOARD_SPACEBAR;
    if(strcmp(name, "RIGHT") == 0) return HID_KEYBOARD_RIGHT_ARROW;
    if(strcmp(name, "LEFT") == 0) return HID_KEYBOARD_LEFT_ARROW;
    if(strcmp(name, "DOWN") == 0) return HID_KEYBOARD_DOWN_ARROW;
    if(strcmp(name, "UP") == 0) return HID_KEYBOARD_UP_ARROW;
    if(strcmp(name, "F4") == 0) return HID_KEYBOARD_F4;
    if(strcmp(name, "F5") == 0) return HID_KEYBOARD_F5;
    if(strcmp(name, "F6") == 0) return HID_KEYBOARD_F6;
    if(strcmp(name, "F7") == 0) return HID_KEYBOARD_F7;
    if(strcmp(name, "F8") == 0) return HID_KEYBOARD_F8;
    if(strcmp(name, "F9") == 0) return HID_KEYBOARD_F9;
    if(strcmp(name, "F10") == 0) return HID_KEYBOARD_F10;
    if(strcmp(name, "F11") == 0) return HID_KEYBOARD_F11;
    if(strcmp(name, "F12") == 0) return HID_KEYBOARD_F12;
    return 0;
}

// Modifier key mappings (using KEY_MOD_* constants from firmware)
static uint16_t modifier_name_to_code(const char* name) {
    // Support both uppercase and lowercase
    char upper[16];
    int i;
    for(i = 0; name[i] && i < 15; i++) {
        upper[i] = toupper((unsigned char)name[i]);
    }
    upper[i] = '\0';  // terminate at actual end of string, not always at [15]
    
    if(strcmp(upper, "CTRL") == 0 || strcmp(upper, "LEFTCTRL") == 0) return KEY_MOD_LEFT_CTRL;
    if(strcmp(upper, "SHIFT") == 0 || strcmp(upper, "LEFTSHIFT") == 0) return KEY_MOD_LEFT_SHIFT;
    if(strcmp(upper, "ALT") == 0 || strcmp(upper, "LEFTALT") == 0) return KEY_MOD_LEFT_ALT;
    if(strcmp(upper, "GUI") == 0 || strcmp(upper, "LEFTGUI") == 0 || strcmp(upper, "WIN") == 0 || strcmp(upper, "CMD") == 0) return KEY_MOD_LEFT_GUI;
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
    char* plus = strchr(key_part, '+');

    while(plus) {
        *plus = '\0';
        modifiers |= modifier_name_to_code(key_part);
        key_part = plus + 1;
        plus = strchr(key_part, '+');
    }
    
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