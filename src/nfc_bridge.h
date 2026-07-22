/**
 * @file nfc_bridge.h
 * @brief NFC tag scanning bridge for FlipDeck
 *
 * Mirrors uart_bridge.h's shape: isolates the hardware-specific NFC calls
 * behind a small interface so flipdeck_ui.c never touches Nfc/NfcScanner/
 * NfcPoller types directly.
 *
 * Detection happens on the NFC worker thread (scanner/poller callbacks), not
 * the UI thread. Rather than exposing that callback directly - which would
 * require every caller to reason about cross-thread UI state mutation - this
 * bridge latches the result behind a mutex and exposes it via
 * nfc_bridge_poll_tag(), meant to be called once per flipdeck_app_loop()
 * tick (already running at a steady 50ms cadence) while a scan is active.
 */

#ifndef NFC_BRIDGE_H
#define NFC_BRIDGE_H

#include <furi.h>
#include <stddef.h>

/**
 * @brief Acquire the NFC hardware handle. Call once at app startup.
 * @return true if the handle was acquired successfully
 */
bool nfc_bridge_init(void);

/**
 * @brief Release NFC resources. Call once at app shutdown.
 */
void nfc_bridge_deinit(void);

/**
 * @brief Begin scanning for a tag. Idempotent - calling this while a scan
 * is already in progress is a no-op.
 */
void nfc_bridge_start_scan(void);

/**
 * @brief Stop any in-progress scan/read without waiting for a result.
 */
void nfc_bridge_stop_scan(void);

/**
 * @brief Check whether a tag was detected since the last call, or since
 * nfc_bridge_start_scan(). Call this once per main-loop tick while
 * FlipDeckState_NfcScan is active.
 *
 * On a true return, scanning has already been stopped (single-shot per
 * scan session) - call nfc_bridge_start_scan() again to re-arm.
 *
 * @param uid_hex_out Buffer to receive the uppercase hex UID
 *        (size at least FLIPDECK_NFC_UID_HEX_LEN, see profile_manager.h)
 * @param out_size Size of uid_hex_out
 * @return true if a tag was detected and uid_hex_out is valid
 */
bool nfc_bridge_poll_tag(char* uid_hex_out, size_t out_size);

#endif // NFC_BRIDGE_H
