/**
 * @file uart_bridge.c
 * @brief UART bridge implementation for the Flipper WiFi Dev Board
 */

#include "uart_bridge.h"
#include <furi_hal_serial.h>
#include <string.h>

// WiFi dev board console runs at 115200 baud over the GPIO UART (pins 13/14)
#define UART_BRIDGE_BAUD_RATE 115200

static FuriHalSerialHandle* s_serial_handle = NULL;

void uart_bridge_init(void) {
    s_serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!s_serial_handle) {
        FURI_LOG_W("FlipDeck", "WiFi devboard UART unavailable");
        return;
    }

    furi_hal_serial_init(s_serial_handle, UART_BRIDGE_BAUD_RATE);
}

void uart_bridge_deinit(void) {
    if(!s_serial_handle) return;

    furi_hal_serial_deinit(s_serial_handle);
    furi_hal_serial_control_release(s_serial_handle);
    s_serial_handle = NULL;
}

bool uart_bridge_is_connected(void) {
    return s_serial_handle != NULL;
}

bool uart_bridge_send_string(const char* text) {
    if(!s_serial_handle || !text) return false;

    FURI_LOG_I("FlipDeck", "Sending to WiFi devboard: %s", text);

    // The dev board console expects CRLF-terminated lines; strip any trailing
    // newline from the profile command and replace it with "\r\n".
    size_t len = strlen(text);
    if(len > 0 && text[len - 1] == '\n') len--;
    if(len > 0 && text[len - 1] == '\r') len--;

    if(len > 0) {
        furi_hal_serial_tx(s_serial_handle, (const uint8_t*)text, len);
    }
    furi_hal_serial_tx(s_serial_handle, (const uint8_t*)"\r\n", 2);
    furi_hal_serial_tx_wait_complete(s_serial_handle);

    return true;
}
