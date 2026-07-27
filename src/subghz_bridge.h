/**
 * @file subghz_bridge.h
 * @brief Sub-GHz (433MHz) remote/keyfob scanning bridge for FlipDeck
 *
 * Mirrors nfc_bridge.h's shape: isolates the hardware-specific Sub-GHz calls
 * behind a small interface so flipdeck_ui.c never touches SubGhz or
 * FuriHalSubGhz types directly.
 *
 * RECEIVE ONLY. This module never transmits — see subghz_bridge.c's file
 * header for why that boundary is deliberate and load-bearing, not just an
 * unfinished feature.
 *
 * Detection happens on the Sub-GHz worker thread (decoder callbacks), not
 * the UI thread. Same as nfc_bridge.h, this bridge latches the result behind
 * a mutex and exposes it via subghz_bridge_poll_signal(), meant to be called
 * once per flipdeck_app_loop() tick while a scan is active.
 */

#ifndef SUBGHZ_BRIDGE_H
#define SUBGHZ_BRIDGE_H

#include <furi.h>
#include <stddef.h>

/**
 * @brief Acquire the Sub-GHz hardware handle and allocate the reusable
 * protocol environment. Call once at app startup.
 * @return true if the handle was acquired successfully
 */
bool subghz_bridge_init(void);

/**
 * @brief Release Sub-GHz resources. Call once at app shutdown.
 */
void subghz_bridge_deinit(void);

/**
 * @brief Begin listening for a signal. Idempotent - calling this while a
 * scan is already in progress is a no-op.
 */
void subghz_bridge_start_scan(void);

/**
 * @brief Stop any in-progress scan without waiting for a result.
 */
void subghz_bridge_stop_scan(void);

/**
 * @brief Check whether a signal was decoded since the last call, or since
 * subghz_bridge_start_scan(). Call this once per main-loop tick while
 * FlipDeckState_SubghzScan is active.
 *
 * On a true return, scanning has already been stopped (single-shot per
 * scan session) - call subghz_bridge_start_scan() again to re-arm.
 *
 * @param signature_hex_out Buffer to receive the fingerprint hex string
 *        (size at least FLIPDECK_SUBGHZ_SIG_HEX_LEN, see profile_manager.h)
 * @param out_size Size of signature_hex_out
 * @return true if a signal was decoded and signature_hex_out is valid
 */
bool subghz_bridge_poll_signal(char* signature_hex_out, size_t out_size);

#endif // SUBGHZ_BRIDGE_H
