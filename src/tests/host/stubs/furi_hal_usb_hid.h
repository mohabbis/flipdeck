#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} furi_hal_usb_hid_keyboard_report_t;

static inline void furi_hal_usb_hid_send_keyboard_report(furi_hal_usb_hid_keyboard_report_t* r) {
    (void)r;
}
static inline bool furi_hal_usb_is_enabled(void) { return false; }
