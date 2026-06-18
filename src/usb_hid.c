/**
 * @file usb_hid.c
 * @brief USB HID communication implementation
 */

#include "usb_hid.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>

#ifndef HID_KEYBOARD_OPEN_BRACKET
#define HID_KEYBOARD_OPEN_BRACKET 47
#endif
#ifndef HID_KEYBOARD_CLOSE_BRACKET
#define HID_KEYBOARD_CLOSE_BRACKET 48
#endif
#ifndef HID_KEYBOARD_BACKSLASH
#define HID_KEYBOARD_BACKSLASH 49
#endif
#ifndef HID_KEYBOARD_SEMICOLON
#define HID_KEYBOARD_SEMICOLON 51
#endif
#ifndef HID_KEYBOARD_APOSTROPHE
#define HID_KEYBOARD_APOSTROPHE 52
#endif
#ifndef HID_KEYBOARD_COMMA
#define HID_KEYBOARD_COMMA 54
#endif
#ifndef HID_KEYBOARD_DOT
#define HID_KEYBOARD_DOT 55
#endif
#ifndef HID_KEYBOARD_SLASH
#define HID_KEYBOARD_SLASH 56
#endif

// USB HID state
static bool s_usb_connected = false;

typedef struct {
    uint8_t key;
    uint16_t modifiers;
} HidKeyChord;

static bool char_to_chord(char c, HidKeyChord* chord) {
    if(!chord) return false;
    chord->key = 0;
    chord->modifiers = 0;

    if(c >= 'a' && c <= 'z') {
        chord->key = c - 'a' + HID_KEYBOARD_A;
        return true;
    }
    if(c >= 'A' && c <= 'Z') {
        chord->key = c - 'A' + HID_KEYBOARD_A;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    }
    if(c >= '1' && c <= '9') {
        chord->key = c - '1' + HID_KEYBOARD_1;
        return true;
    }
    if(c == '0') {
        chord->key = HID_KEYBOARD_0;
        return true;
    }

    switch(c) {
    case '\n':
    case '\r':
        chord->key = HID_KEYBOARD_RETURN;
        return true;
    case '\t':
        chord->key = HID_KEYBOARD_TAB;
        return true;
    case ' ':
        chord->key = HID_KEYBOARD_SPACEBAR;
        return true;
    case '-':
        chord->key = HID_KEYBOARD_MINUS;
        return true;
    case '_':
        chord->key = HID_KEYBOARD_MINUS;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '=':
        chord->key = HID_KEYBOARD_EQUAL_SIGN;
        return true;
    case '+':
        chord->key = HID_KEYBOARD_EQUAL_SIGN;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '[':
        chord->key = HID_KEYBOARD_OPEN_BRACKET;
        return true;
    case '{':
        chord->key = HID_KEYBOARD_OPEN_BRACKET;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case ']':
        chord->key = HID_KEYBOARD_CLOSE_BRACKET;
        return true;
    case '}':
        chord->key = HID_KEYBOARD_CLOSE_BRACKET;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '\\':
        chord->key = HID_KEYBOARD_BACKSLASH;
        return true;
    case '|':
        chord->key = HID_KEYBOARD_BACKSLASH;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case ';':
        chord->key = HID_KEYBOARD_SEMICOLON;
        return true;
    case ':':
        chord->key = HID_KEYBOARD_SEMICOLON;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '\'':
        chord->key = HID_KEYBOARD_APOSTROPHE;
        return true;
    case '"':
        chord->key = HID_KEYBOARD_APOSTROPHE;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '`':
        chord->key = HID_KEYBOARD_GRAVE_ACCENT;
        return true;
    case '~':
        chord->key = HID_KEYBOARD_GRAVE_ACCENT;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case ',':
        chord->key = HID_KEYBOARD_COMMA;
        return true;
    case '<':
        chord->key = HID_KEYBOARD_COMMA;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '.':
        chord->key = HID_KEYBOARD_DOT;
        return true;
    case '>':
        chord->key = HID_KEYBOARD_DOT;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '/':
        chord->key = HID_KEYBOARD_SLASH;
        return true;
    case '?':
        chord->key = HID_KEYBOARD_SLASH;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '!':
        chord->key = HID_KEYBOARD_1;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '@':
        chord->key = HID_KEYBOARD_2;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '#':
        chord->key = HID_KEYBOARD_3;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '$':
        chord->key = HID_KEYBOARD_4;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '%':
        chord->key = HID_KEYBOARD_5;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '^':
        chord->key = HID_KEYBOARD_6;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '&':
        chord->key = HID_KEYBOARD_7;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '*':
        chord->key = HID_KEYBOARD_8;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case '(':
        chord->key = HID_KEYBOARD_9;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    case ')':
        chord->key = HID_KEYBOARD_0;
        chord->modifiers = KEY_MOD_LEFT_SHIFT;
        return true;
    default:
        return false;
    }
}

// Key name to HID keycode mapping (using HID_KEYBOARD_* constants from firmware)
static uint16_t key_name_to_code(const char* name) {
    // Support single characters directly (US keyboard layout)
    if(strlen(name) == 1 && !(name[0] >= 'A' && name[0] <= 'Z')) {
        HidKeyChord chord;
        if(char_to_chord(name[0], &chord)) return chord.modifiers | chord.key;
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
    s_usb_connected = furi_hal_hid_is_connected();
    return s_usb_connected;
}

bool usb_hid_send_string(const char* text) {
    if(!s_usb_connected) {
        FURI_LOG_W("FlipDeck", "USB not connected");
        return false;
    }
    
    FURI_LOG_I("FlipDeck", "Sending string: %s", text);
    
    for(uint32_t i = 0; text[i] != '\0'; i++) {
        HidKeyChord chord;
        if(!char_to_chord(text[i], &chord)) {
            FURI_LOG_W("FlipDeck", "Unsupported character: 0x%02X", (uint8_t)text[i]);
            return false;
        }
        furi_hal_hid_kb_press(chord.modifiers | chord.key);
        furi_delay_ms(5);
        furi_hal_hid_kb_release_all();
        furi_delay_ms(5);
    }
    
    return true;
}

bool usb_hid_send_key(const char* keyName) {
    if(!s_usb_connected) {
        return false;
    }
    
    uint16_t key = key_name_to_code(keyName);
    if(key == 0) {
        FURI_LOG_W("FlipDeck", "Unknown key: %s", keyName);
        return false;
    }
    
    furi_hal_hid_kb_press(key);
    furi_delay_ms(50);  // Press duration
    furi_hal_hid_kb_release_all();
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
    
    uint16_t modifiers = 0;
    char* key_part = work;
    char* plus = strchr(key_part, '+');

    while(plus) {
        *plus = '\0';
        modifiers |= modifier_name_to_code(key_part);
        key_part = plus + 1;
        plus = strchr(key_part, '+');
    }
    
    uint16_t key = key_name_to_code(key_part);
    if(key == 0) {
        FURI_LOG_W("FlipDeck", "Unknown key in combo: %s", key_part);
        return false;
    }
    
    furi_hal_hid_kb_press(modifiers | key);
    furi_delay_ms(50);
    furi_hal_hid_kb_release_all();
    
    return true;
}
