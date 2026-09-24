/**
 * @file fd_ble.h
 * @brief Byte-stream transport to the Mac over BLE.
 *
 * FlipDeck runs its own BLE profile (serial GATT service + device info +
 * battery) instead of the stock serial profile, because the BT service takes
 * over the stock profile for RPC on every connection. See ARCHITECTURE.md.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct FdBle FdBle;

/** Called from the BT thread; must be quick (post to a queue). */
typedef void (*FdBleEventCallback)(void* context);

/**
 * Starts the FlipDeck profile and advertising. `on_rx` fires when bytes
 * arrive, `on_status` when the connection state changes.
 */
FdBle* fd_ble_alloc(FdBleEventCallback on_rx, FdBleEventCallback on_status, void* context);

/** Disconnects and restores the default BT profile and pairing keys. */
void fd_ble_free(FdBle* ble);

bool fd_ble_is_connected(FdBle* ble);

/** Non-blocking read of received bytes. */
size_t fd_ble_read(FdBle* ble, uint8_t* buffer, size_t capacity);

/** Discards everything buffered (on a new connection). */
void fd_ble_discard(FdBle* ble);

/**
 * Call after `fd_ble_read` has fully drained the buffer: re-arms the Mac's
 * flow-control credit.
 */
void fd_ble_rx_drained(FdBle* ble);

/**
 * Sends `len` bytes as indications of at most `chunk` bytes each, waiting
 * for each to be confirmed. Blocks for at most ~1 s per chunk.
 */
bool fd_ble_send(FdBle* ble, const uint8_t* data, size_t len, uint16_t chunk);
