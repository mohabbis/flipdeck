/**
 * @file nfc_bridge.c
 * @brief NFC tag scanning bridge implementation
 *
 * ############################################################################
 * # UNVERIFIED AGAINST REAL FIRMWARE HEADERS
 * #
 * # This file was written against function signatures confirmed from
 * # lib/nfc/{nfc_scanner,nfc_poller,nfc_device}.h in the flipperdevices
 * # firmware source, but there was no uFBT/fbt toolchain available to
 * # actually compile it against those headers. The riskiest guess is the
 * # bridge from NfcPoller's `const NfcDeviceData*` (nfc_poller_get_data)
 * # to a UID via a scratch NfcDevice (nfc_device_set_data +
 * # nfc_device_get_uid) - firmware code may have a more direct accessor.
 * # `NfcCommand`/`NfcGenericEvent`'s exact field names are inferred from
 * # common Flipper poller-callback conventions, not confirmed. Build this
 * # with a real `fbt`/`ufbt` and expect to fix compile errors here first.
 * ############################################################################
 */

#include "nfc_bridge.h"
#include "profile_manager.h"
#include <furi.h>
#include <string.h>

#ifndef UFBT_HOST_BUILD
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>

static Nfc* nfc_handle = NULL;
static NfcScanner* scanner = NULL;
static NfcPoller* poller = NULL;
static FuriMutex* nfc_bridge_mutex = NULL;
static char pending_uid_hex[FLIPDECK_NFC_UID_HEX_LEN];
static bool pending_uid_ready = false;

static void nfc_bridge_free_poller(void) {
    if(poller) {
        nfc_poller_stop(poller);
        nfc_poller_free(poller);
        poller = NULL;
    }
}

static void nfc_bridge_free_scanner(void) {
    if(scanner) {
        nfc_scanner_stop(scanner);
        nfc_scanner_free(scanner);
        scanner = NULL;
    }
}

// Runs on the NFC worker thread. Reads exactly once, latches the UID behind
// the mutex for nfc_bridge_poll_tag() to pick up on the main thread, then
// tells the poller to stop - this bridge is single-shot per scan session by
// design (rescan is an explicit nfc_bridge_start_scan() call).
static NfcCommand nfc_bridge_poller_callback(NfcGenericEvent event, void* context) {
    UNUSED(context);
    UNUSED(event);

    const NfcDeviceData* data = nfc_poller_get_data(poller);
    if(data) {
        NfcDevice* device = nfc_device_alloc();
        nfc_device_set_data(device, nfc_poller_get_protocol(poller), data);

        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(device, &uid_len);

        furi_mutex_acquire(nfc_bridge_mutex, FuriWaitForever);
        if(profile_manager_hex_encode_uid(uid, uid_len, pending_uid_hex, sizeof(pending_uid_hex))) {
            pending_uid_ready = true;
        }
        furi_mutex_release(nfc_bridge_mutex);

        nfc_device_free(device);
    }

    return NfcCommandStop;
}

// Runs on the NFC worker thread. A protocol was detected during the generic
// scan; hand off to a protocol-specific poller to actually read the UID.
static void nfc_bridge_scanner_callback(NfcScannerEvent event, void* context) {
    UNUSED(context);
    if(event.type != NfcScannerEventTypeDetected) return;
    if(event.data.protocol_num == 0) return;

    nfc_bridge_free_scanner();

    poller = nfc_poller_alloc(nfc_handle, event.data.protocols[0]);
    nfc_poller_start(poller, nfc_bridge_poller_callback, NULL);
}

bool nfc_bridge_init(void) {
    nfc_handle = nfc_alloc();
    nfc_bridge_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return nfc_handle != NULL && nfc_bridge_mutex != NULL;
}

void nfc_bridge_deinit(void) {
    nfc_bridge_stop_scan();
    if(nfc_bridge_mutex) {
        furi_mutex_free(nfc_bridge_mutex);
        nfc_bridge_mutex = NULL;
    }
    if(nfc_handle) {
        nfc_free(nfc_handle);
        nfc_handle = NULL;
    }
}

void nfc_bridge_start_scan(void) {
    if(scanner || poller) return; // already scanning/reading
    if(!nfc_handle) return;

    furi_mutex_acquire(nfc_bridge_mutex, FuriWaitForever);
    pending_uid_ready = false;
    furi_mutex_release(nfc_bridge_mutex);

    scanner = nfc_scanner_alloc(nfc_handle);
    nfc_scanner_start(scanner, nfc_bridge_scanner_callback, NULL);
}

void nfc_bridge_stop_scan(void) {
    nfc_bridge_free_poller();
    nfc_bridge_free_scanner();
}

bool nfc_bridge_poll_tag(char* uid_hex_out, size_t out_size) {
    bool ready = false;

    furi_mutex_acquire(nfc_bridge_mutex, FuriWaitForever);
    if(pending_uid_ready) {
        strncpy(uid_hex_out, pending_uid_hex, out_size - 1);
        uid_hex_out[out_size - 1] = '\0';
        pending_uid_ready = false;
        ready = true;
    }
    furi_mutex_release(nfc_bridge_mutex);

    if(ready) nfc_bridge_stop_scan();
    return ready;
}

#else /* UFBT_HOST_BUILD */

// This file is hardware-only and isn't part of the host test build (see
// tests/host/Makefile), but stubs are provided so nothing breaks if a future
// host target ever pulls it in.
bool nfc_bridge_init(void) {
    return false;
}
void nfc_bridge_deinit(void) {
}
void nfc_bridge_start_scan(void) {
}
void nfc_bridge_stop_scan(void) {
}
bool nfc_bridge_poll_tag(char* uid_hex_out, size_t out_size) {
    UNUSED(uid_hex_out);
    UNUSED(out_size);
    return false;
}

#endif
