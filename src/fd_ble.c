/**
 * @file fd_ble.c
 * @brief FlipDeck BLE profile and serial transport (see fd_ble.h).
 */
#include "fd_ble.h"

#include <bt/bt_service/bt.h>
#include <furi.h>
#include <furi_ble/profile_interface.h>
#include <furi_hal_bt.h>
#include <furi_hal_version.h>
#include <services/battery_service.h>
#include <services/dev_info_service.h>
#include <services/serial_service.h>
#include <storage/storage.h>

#define TAG "FlipDeckBle"

/** Receive buffer; also the flow-control credit advertised to the Mac. */
#define FD_BLE_RX_BUFFER 1024
#define FD_BLE_FLAG_SENT (1u << 0)
#define FD_BLE_SEND_TIMEOUT_MS 1000
/** Pairing keys for the FlipDeck profile, kept apart from the phone app's. */
#define FD_BLE_KEYS_PATH APP_DATA_PATH(".flipdeck.keys")
/** 16-bit Device Information Service UUID, which this profile really exposes. */
#define FD_BLE_ADV_SERVICE_UUID 0x180A
#define FD_BLE_UUID_TYPE_16 0x01
/** Generic Computer appearance. */
#define FD_BLE_APPEARANCE 0x0080

typedef struct {
    FuriHalBleProfileBase base;
    BleServiceDevInfo* dev_info;
    BleServiceBattery* battery;
    BleServiceSerial* serial;
} FdBleProfile;

struct FdBle {
    Bt* bt;
    FdBleProfile* profile;
    FuriStreamBuffer* rx;
    FuriEventFlag* flags;
    volatile bool connected;
    FdBleEventCallback on_rx;
    FdBleEventCallback on_status;
    void* context;
};

// ---------------------------------------------------------------------------
// Profile

static const FuriHalBleProfileTemplate* const fd_ble_profile_template;

static FuriHalBleProfileBase* fd_ble_profile_start(FuriHalBleProfileParams params) {
    UNUSED(params);
    FdBleProfile* profile = malloc(sizeof(FdBleProfile));
    profile->base.config = fd_ble_profile_template;
    profile->dev_info = ble_svc_dev_info_start();
    profile->battery = ble_svc_battery_start(true);
    profile->serial = ble_svc_serial_start();
    return &profile->base;
}

static void fd_ble_profile_stop(FuriHalBleProfileBase* base) {
    furi_check(base);
    furi_check(base->config == fd_ble_profile_template);
    FdBleProfile* profile = (FdBleProfile*)base;
    ble_svc_serial_stop(profile->serial);
    ble_svc_battery_stop(profile->battery);
    ble_svc_dev_info_stop(profile->dev_info);
    free(profile);
}

static void fd_ble_profile_get_config(GapConfig* config, FuriHalBleProfileParams params) {
    UNUSED(params);
    furi_check(config);
    memset(config, 0, sizeof(GapConfig));
    config->adv_service.UUID_Type = FD_BLE_UUID_TYPE_16;
    config->adv_service.Service_UUID_16 = FD_BLE_ADV_SERVICE_UUID;
    config->appearance_char = FD_BLE_APPEARANCE;
    config->bonding_mode = true;
    config->pairing_method = GapPairingPinCodeShow;
    // Same connection parameters as the stock serial profile (7.5-45 ms).
    config->conn_param.conn_int_min = 0x06;
    config->conn_param.conn_int_max = 0x24;
    config->conn_param.slave_latency = 0;
    config->conn_param.supervisor_timeout = 0;

    // A distinct address from the stock (+0) and HID (+1) profiles, so the
    // Mac's bond for FlipDeck never collides with other pairings.
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));
    config->mac_address[2] += 2;

    // "\x09Flipper Name" (AD type prefix) -> "\x09FlipDeck Name"; the Mac
    // looks for the "FlipDeck" prefix.
    FuriString* name = furi_string_alloc_set(furi_hal_version_get_ble_local_device_name_ptr());
    furi_string_replace_str(name, "Flipper", "FlipDeck", 0);
    if(furi_string_size(name) >= sizeof(config->adv_name)) {
        furi_string_left(name, sizeof(config->adv_name) - 1);
    }
    memcpy(config->adv_name, furi_string_get_cstr(name), furi_string_size(name));
    furi_string_free(name);
}

static const FuriHalBleProfileTemplate fd_ble_profile_callbacks = {
    .start = fd_ble_profile_start,
    .stop = fd_ble_profile_stop,
    .get_gap_config = fd_ble_profile_get_config,
};

static const FuriHalBleProfileTemplate* const fd_ble_profile_template = &fd_ble_profile_callbacks;

// ---------------------------------------------------------------------------
// Serial service callbacks (BT thread)

static uint16_t fd_ble_serial_callback(SerialServiceEvent event, void* context) {
    FdBle* ble = context;
    if(event.event == SerialServiceEventTypeDataReceived) {
        size_t accepted = furi_stream_buffer_send(ble->rx, event.data.buffer, event.data.size, 0);
        if(accepted < event.data.size) {
            // Can't happen while the Mac respects the credit; the protocol
            // recovers (CRC failure -> SYNC) if it does.
            FURI_LOG_W(TAG, "RX overflow: dropped %u bytes", (unsigned)(event.data.size - accepted));
        }
        if(ble->on_rx) ble->on_rx(ble->context);
        return (uint16_t)furi_stream_buffer_spaces_available(ble->rx);
    }
    if(event.event == SerialServiceEventTypeDataSent) {
        furi_event_flag_set(ble->flags, FD_BLE_FLAG_SENT);
    }
    return 0;
}

static void fd_ble_status_callback(BtStatus status, void* context) {
    FdBle* ble = context;
    bool connected = status == BtStatusConnected;
    if(connected && ble->profile) {
        // Fresh credit for every connection (a previous one may have ended
        // mid-transfer with the credit partly used).
        ble_svc_serial_set_callbacks(ble->profile->serial, FD_BLE_RX_BUFFER, fd_ble_serial_callback, ble);
    }
    if(connected != ble->connected) {
        ble->connected = connected;
        if(ble->on_status) ble->on_status(ble->context);
    }
}

// ---------------------------------------------------------------------------
// Public API

FdBle* fd_ble_alloc(FdBleEventCallback on_rx, FdBleEventCallback on_status, void* context) {
    FdBle* ble = malloc(sizeof(FdBle));
    memset(ble, 0, sizeof(FdBle));
    ble->on_rx = on_rx;
    ble->on_status = on_status;
    ble->context = context;
    ble->rx = furi_stream_buffer_alloc(FD_BLE_RX_BUFFER, 1);
    ble->flags = furi_event_flag_alloc();
    ble->bt = furi_record_open(RECORD_BT);

    // Same sequence as the firmware's own HID app.
    bt_disconnect(ble->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(ble->bt, FD_BLE_KEYS_PATH);
    ble->profile = (FdBleProfile*)bt_profile_start(ble->bt, fd_ble_profile_template, NULL);
    furi_check(ble->profile, "Couldn't start the FlipDeck BLE profile");
    ble_svc_serial_set_callbacks(ble->profile->serial, FD_BLE_RX_BUFFER, fd_ble_serial_callback, ble);
    bt_set_status_changed_callback(ble->bt, fd_ble_status_callback, ble);
    furi_hal_bt_start_advertising();
    FURI_LOG_I(TAG, "Advertising");
    return ble;
}

void fd_ble_free(FdBle* ble) {
    bt_set_status_changed_callback(ble->bt, NULL, NULL);
    ble_svc_serial_set_callbacks(ble->profile->serial, 0, NULL, NULL);
    bt_disconnect(ble->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(ble->bt);
    // Stops our profile (fd_ble_profile_stop frees it) and restores the
    // stock one, so the phone app works again after FlipDeck exits.
    furi_check(bt_profile_restore_default(ble->bt));
    furi_record_close(RECORD_BT);
    furi_event_flag_free(ble->flags);
    furi_stream_buffer_free(ble->rx);
    free(ble);
}

bool fd_ble_is_connected(FdBle* ble) {
    return ble->connected;
}

size_t fd_ble_read(FdBle* ble, uint8_t* buffer, size_t capacity) {
    return furi_stream_buffer_receive(ble->rx, buffer, capacity, 0);
}

void fd_ble_discard(FdBle* ble) {
    uint8_t scratch[64];
    while(furi_stream_buffer_receive(ble->rx, scratch, sizeof(scratch), 0) > 0) {
    }
}

void fd_ble_rx_drained(FdBle* ble) {
    if(ble->profile) ble_svc_serial_notify_buffer_is_empty(ble->profile->serial);
}

bool fd_ble_send(FdBle* ble, const uint8_t* data, size_t len, uint16_t chunk) {
    if(!ble->connected || !ble->profile) return false;
    if(chunk == 0) chunk = 20;
    if(chunk > BLE_SVC_SERIAL_CHAR_VALUE_LEN_MAX) chunk = BLE_SVC_SERIAL_CHAR_VALUE_LEN_MAX;
    while(len > 0) {
        uint16_t n = (uint16_t)(len < chunk ? len : chunk);
        furi_event_flag_clear(ble->flags, FD_BLE_FLAG_SENT);
        if(!ble_svc_serial_update_tx(ble->profile->serial, (uint8_t*)data, n)) {
            FURI_LOG_W(TAG, "TX failed");
            return false;
        }
        // Wait for the Mac to confirm the indication before sending more.
        uint32_t flags = furi_event_flag_wait(ble->flags, FD_BLE_FLAG_SENT, FuriFlagWaitAny, FD_BLE_SEND_TIMEOUT_MS);
        if(flags & FuriFlagError) {
            FURI_LOG_W(TAG, "TX not confirmed");
            return false;
        }
        data += n;
        len -= n;
    }
    return true;
}
