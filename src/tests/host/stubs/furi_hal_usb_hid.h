#pragma once

#include <stdint.h>
#include <stdbool.h>


#ifndef HID_KEYBOARD_A
#define HID_KEYBOARD_A 4
#define HID_KEYBOARD_B 5
#define HID_KEYBOARD_C 6
#define HID_KEYBOARD_D 7
#define HID_KEYBOARD_E 8
#define HID_KEYBOARD_F 9
#define HID_KEYBOARD_G 10
#define HID_KEYBOARD_H 11
#define HID_KEYBOARD_I 12
#define HID_KEYBOARD_J 13
#define HID_KEYBOARD_K 14
#define HID_KEYBOARD_L 15
#define HID_KEYBOARD_M 16
#define HID_KEYBOARD_N 17
#define HID_KEYBOARD_O 18
#define HID_KEYBOARD_P 19
#define HID_KEYBOARD_Q 20
#define HID_KEYBOARD_R 21
#define HID_KEYBOARD_S 22
#define HID_KEYBOARD_T 23
#define HID_KEYBOARD_U 24
#define HID_KEYBOARD_V 25
#define HID_KEYBOARD_W 26
#define HID_KEYBOARD_X 27
#define HID_KEYBOARD_Y 28
#define HID_KEYBOARD_Z 29
#define HID_KEYBOARD_1 30
#define HID_KEYBOARD_2 31
#define HID_KEYBOARD_3 32
#define HID_KEYBOARD_4 33
#define HID_KEYBOARD_5 34
#define HID_KEYBOARD_6 35
#define HID_KEYBOARD_7 36
#define HID_KEYBOARD_8 37
#define HID_KEYBOARD_9 38
#define HID_KEYBOARD_0 39
#define HID_KEYBOARD_RETURN 40
#define HID_KEYBOARD_ESCAPE 41
#define HID_KEYBOARD_DELETE 42
#define HID_KEYBOARD_TAB 43
#define HID_KEYBOARD_SPACEBAR 44
#define HID_KEYBOARD_MINUS 45
#define HID_KEYBOARD_EQUAL_SIGN 46
#define HID_KEYBOARD_GRAVE_ACCENT 53
#define HID_KEYBOARD_F4 61
#define HID_KEYBOARD_F5 62
#define HID_KEYBOARD_F6 63
#define HID_KEYBOARD_F7 64
#define HID_KEYBOARD_F8 65
#define HID_KEYBOARD_F9 66
#define HID_KEYBOARD_F10 67
#define HID_KEYBOARD_F11 68
#define HID_KEYBOARD_F12 69
#define HID_KEYBOARD_RIGHT_ARROW 79
#define HID_KEYBOARD_LEFT_ARROW 80
#define HID_KEYBOARD_DOWN_ARROW 81
#define HID_KEYBOARD_UP_ARROW 82
#endif

/* Real firmware places modifiers in the upper byte of the uint16_t button
 * value (HID_KEYBOARD_* keycodes occupy the lower byte, 0-255), so these
 * must not collide with keycode values. */
#ifndef KEY_MOD_LEFT_CTRL
#define KEY_MOD_LEFT_CTRL (1 << 8)
#define KEY_MOD_LEFT_SHIFT (1 << 9)
#define KEY_MOD_LEFT_ALT (1 << 10)
#define KEY_MOD_LEFT_GUI (1 << 11)
#endif

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} furi_hal_usb_hid_keyboard_report_t;

static inline void furi_hal_usb_hid_send_keyboard_report(furi_hal_usb_hid_keyboard_report_t* r) {
    (void)r;
}
static inline bool furi_hal_usb_is_enabled(void) { return false; }
static inline bool furi_hal_hid_is_connected(void) { return false; }
static inline bool furi_hal_hid_kb_press(uint16_t button) {
    (void)button;
    return true;
}
static inline bool furi_hal_hid_kb_release(uint16_t button) {
    (void)button;
    return true;
}
static inline bool furi_hal_hid_kb_release_all(void) { return true; }
