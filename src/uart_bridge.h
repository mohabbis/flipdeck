/**
 * @file uart_bridge.h
 * @brief UART bridge to the Flipper WiFi Dev Board (ESP32-S2)
 */

#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include <furi.h>
#include <furi_hal.h>

/**
 * @brief Initialize the UART bridge to the WiFi dev board
 */
void uart_bridge_init(void);

/**
 * @brief Release the UART bridge resources
 */
void uart_bridge_deinit(void);

/**
 * @brief Check if the WiFi dev board UART bridge is available
 * @return true if the UART bridge has been initialized
 */
bool uart_bridge_is_connected(void);

/**
 * @brief Send a string to the WiFi dev board over UART
 * @param text String to send (a trailing "\r\n" is appended if missing)
 * @return true if sent successfully
 */
bool uart_bridge_send_string(const char* text);

#endif // UART_BRIDGE_H
