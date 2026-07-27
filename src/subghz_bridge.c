/**
 * @file subghz_bridge.c
 * @brief Sub-GHz remote/keyfob scanning bridge implementation
 *
 * ############################################################################
 * # VERIFIED AGAINST REAL FIRMWARE HEADERS AND A REAL REFERENCE
 * # IMPLEMENTATION, NOT AGAINST REAL FIRMWARE
 * #
 * # There is no fbt/ufbt toolchain available in this environment (its SDK
 * # download is blocked by network policy), so this could not go through a
 * # real build. As a substitute: every function/struct/macro this file uses
 * # was fetched live from flipperdevices/flipperzero-firmware (dev branch)
 * # and syntax/type-checked with gcc -fsyntax-only -Wall -Wextra -Wpedantic
 * # against those real headers - not guessed. Beyond that, the *sequence*
 * # of calls below (reset/idle/load-preset/set-frequency -> environment+
 * # receiver+worker wiring -> start -> stop) mirrors, nearly line for line,
 * # applications/main/subghz/helpers/subghz_txrx.c - the official Sub-GHz
 * # app's own internal helper for doing exactly this (that file goes
 * # through the newer subghz_devices_* abstraction instead of
 * # furi_hal_subghz_* directly; see the note below this header for why this
 * # file deliberately doesn't). That file is private to that app (not
 * # includable from an external .fap), so it could not be reused directly,
 * # but its logic was read in full and this file's control flow
 * # deliberately matches it rather than improvising a new sequence.
 * #
 * # RECEIVE ONLY - deliberately, structurally, not just "not implemented
 * # yet". No furi_hal_subghz_start_async_tx / _set_tx call appears anywhere
 * # below. This means: (a) no regional TX power/frequency regulation
 * # applies, because nothing is ever transmitted; (b) this file cannot be
 * # used to replay or clone a signal against a garage door, gate, or car,
 * # even by accident, because replay requires transmission and this bridge
 * # has no transmit code path to repurpose.
 * #
 * # Binding uses the full/default protocol registry (subghz_protocol_
 * # registry) and matches on exact-string equality of the decoded protocol
 * # text. Fixed-code remotes (common cheap doorbells, garage/gate remotes,
 * # RF relay switches) decode to byte-identical text on every press and
 * # bind reliably. Rolling-code remotes (many modern car/garage keyfobs)
 * # decode to DIFFERENT text every press by design, so they simply never
 * # re-match after the first bind - this is the correct, safe failure mode,
 * # not a bug: a design that could match/replay a rolling code would be a
 * # real security problem, and this one structurally cannot.
 * #
 * # What is NOT covered by the header/reference-source check above: linking
 * # against the real compiled library (application.fam's fap_libs question,
 * # see that file's comment - this is a materially bigger open risk than
 * # NFC's equivalent turned out to be), runtime timing/threading behavior,
 * # and whether this specific device's firmware build differs from the
 * # fetched dev-branch snapshot. Build and flash this on real hardware
 * # before trusting it end-to-end.
 * ############################################################################
 */

#include "subghz_bridge.h"
#include "profile_manager.h"
#include <furi.h>
#include <string.h>

#ifndef UFBT_HOST_BUILD
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/subghz_worker.h>

// Deliberately uses furi_hal_subghz_* (the direct HAL for the Flipper's
// built-in CC1101) rather than the newer subghz_devices_* abstraction layer
// real firmware app code goes through. subghz_devices_* exists to support
// pluggable external CC1101 modules, which pulls in the firmware's ELF
// plugin-loader subsystem (lib/subghz/devices/types.h ->
// lib/flipper_application/...) as a transitive dependency - real
// complexity this feature doesn't need, since it only ever targets the
// built-in radio. furi_hal_subghz_* is the lower layer subghz_devices_*
// itself calls into for the internal radio, so this is not a shortcut
// around anything, just skipping an abstraction layer that has nothing to
// offer a fixed-hardware, RX-only, internal-radio-only use case.

#define SUBGHZ_BRIDGE_FREQUENCY 433920000UL

static SubGhzEnvironment* subghz_environment_handle = NULL;
static SubGhzReceiver* subghz_receiver_handle = NULL;
static SubGhzWorker* subghz_worker_handle = NULL;
static FuriMutex* subghz_bridge_mutex = NULL;
static bool subghz_hal_ready = false;
static char pending_signature_hex[FLIPDECK_SUBGHZ_SIG_HEX_LEN];
static bool pending_signature_ready = false;

static void subghz_bridge_free_session(void) {
    if(subghz_worker_handle) {
        if(subghz_worker_is_running(subghz_worker_handle)) {
            subghz_worker_stop(subghz_worker_handle);
            furi_hal_subghz_stop_async_rx();
        }
        subghz_worker_free(subghz_worker_handle);
        subghz_worker_handle = NULL;
    }
    if(subghz_receiver_handle) {
        subghz_receiver_free(subghz_receiver_handle);
        subghz_receiver_handle = NULL;
    }
    if(subghz_hal_ready) {
        furi_hal_subghz_idle();
    }
}

// Runs on the Sub-GHz worker thread when a protocol decoder in the registry
// fully matches a received signal. Hashes the decoded text and latches it
// behind the mutex for subghz_bridge_poll_signal() to pick up on the main
// thread - same pattern as nfc_bridge_poller_callback's pending_uid_hex.
static void subghz_bridge_receiver_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    UNUSED(context);

    FuriString* text = furi_string_alloc();
    if(subghz_protocol_decoder_base_get_string(decoder_base, text)) {
        char signature_hex[FLIPDECK_SUBGHZ_SIG_HEX_LEN];
        if(profile_manager_hash_subghz_signature(
               furi_string_get_cstr(text), signature_hex, sizeof(signature_hex))) {
            furi_mutex_acquire(subghz_bridge_mutex, FuriWaitForever);
            strncpy(pending_signature_hex, signature_hex, sizeof(pending_signature_hex) - 1);
            pending_signature_hex[sizeof(pending_signature_hex) - 1] = '\0';
            pending_signature_ready = true;
            furi_mutex_release(subghz_bridge_mutex);
        }
    }
    furi_string_free(text);
}

bool subghz_bridge_init(void) {
    subghz_bridge_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!subghz_bridge_mutex) return false;

    furi_hal_subghz_init();
    subghz_hal_ready = true;

    subghz_environment_handle = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(subghz_environment_handle, &subghz_protocol_registry);

    return true;
}

void subghz_bridge_deinit(void) {
    subghz_bridge_stop_scan();

    if(subghz_bridge_mutex) {
        furi_mutex_free(subghz_bridge_mutex);
        subghz_bridge_mutex = NULL;
    }
    if(subghz_environment_handle) {
        subghz_environment_free(subghz_environment_handle);
        subghz_environment_handle = NULL;
    }
    if(subghz_hal_ready) {
        furi_hal_subghz_sleep();
        subghz_hal_ready = false;
    }
}

void subghz_bridge_start_scan(void) {
    if(subghz_worker_handle || subghz_receiver_handle) return; // already scanning
    if(!subghz_hal_ready || !subghz_environment_handle) return;
    if(!furi_hal_subghz_is_frequency_valid(SUBGHZ_BRIDGE_FREQUENCY)) return;

    furi_mutex_acquire(subghz_bridge_mutex, FuriWaitForever);
    pending_signature_ready = false;
    furi_mutex_release(subghz_bridge_mutex);

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset((uint8_t*)subghz_device_cc1101_preset_ook_650khz_async_regs);
    furi_hal_subghz_set_frequency(SUBGHZ_BRIDGE_FREQUENCY);
    furi_hal_subghz_flush_rx();

    subghz_receiver_handle = subghz_receiver_alloc_init(subghz_environment_handle);
    subghz_receiver_set_rx_callback(subghz_receiver_handle, subghz_bridge_receiver_callback, NULL);

    subghz_worker_handle = subghz_worker_alloc();
    // Cast subghz_receiver_reset/subghz_receiver_decode directly to the
    // worker's callback types, exactly as the real firmware's own
    // subghz_txrx.c helper does - the worker passes its context (set to
    // subghz_receiver_handle below) as the callback's leading
    // void*/SubGhzReceiver* argument, and the two pointer types are
    // ABI-compatible.
    subghz_worker_set_overrun_callback(
        subghz_worker_handle, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
    subghz_worker_set_pair_callback(
        subghz_worker_handle, (SubGhzWorkerPairCallback)subghz_receiver_decode);
    subghz_worker_set_context(subghz_worker_handle, subghz_receiver_handle);

    furi_hal_subghz_start_async_rx(subghz_worker_rx_callback, subghz_worker_handle);
    subghz_worker_start(subghz_worker_handle);
}

void subghz_bridge_stop_scan(void) {
    subghz_bridge_free_session();
}

bool subghz_bridge_poll_signal(char* signature_hex_out, size_t out_size) {
    bool ready = false;

    furi_mutex_acquire(subghz_bridge_mutex, FuriWaitForever);
    if(pending_signature_ready) {
        strncpy(signature_hex_out, pending_signature_hex, out_size - 1);
        signature_hex_out[out_size - 1] = '\0';
        pending_signature_ready = false;
        ready = true;
    }
    furi_mutex_release(subghz_bridge_mutex);

    if(ready) subghz_bridge_stop_scan();
    return ready;
}

#else /* UFBT_HOST_BUILD */

// This file is hardware-only and isn't part of the host test build (see
// tests/host/Makefile), but stubs are provided so nothing breaks if a future
// host target ever pulls it in.
bool subghz_bridge_init(void) {
    return false;
}
void subghz_bridge_deinit(void) {
}
void subghz_bridge_start_scan(void) {
}
void subghz_bridge_stop_scan(void) {
}
bool subghz_bridge_poll_signal(char* signature_hex_out, size_t out_size) {
    UNUSED(signature_hex_out);
    UNUSED(out_size);
    return false;
}

#endif
