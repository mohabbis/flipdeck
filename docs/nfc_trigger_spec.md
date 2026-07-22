# Spec: NFC Tag Triggers for FlipDeck

Status: **Phase 1 implemented** (`flipdeck_app.h`/`.c`, `flipdeck_ui.h`/`.c`,
`profile_manager.h`/`.c`, `nfc_bridge.h`/`.c`, `application.fam`).

No `fbt`/`ufbt` toolchain is available in this environment (its SDK download
is blocked by the sandbox's network policy), so this hasn't gone through a
real build. As a substitute: `nfc_bridge.c` was syntax/type-checked with
`gcc -fsyntax-only -Wall -Wextra -Wpedantic` against the actual, current
`lib/nfc/{nfc,nfc_device,nfc_scanner,nfc_poller,protocols/*}.h` fetched from
`flipperdevices/flipperzero-firmware` (not hand-typed guesses at their
contents) and came back with zero warnings — every function signature and
struct field it uses matches the real headers exactly. `flipdeck_ui.c`/
`flipdeck_app.c` were checked the same way against a hand-built fake GUI
header set, and `profile_manager.c`'s new logic runs through the real host
test suite (188/188 passing). None of that substitutes for an actual build +
hardware flash, which is still the next step before trusting this fully —
see `nfc_bridge.c`'s file header comment for exactly what remains unconfirmed
(linking, runtime behavior, any Momentum-specific header differences).

Two deliberate deviations from the plan below, made during implementation:
- No separate `FlipDeckState_NfcBind`. Binding reuses `CategoryBrowser`/
  `ActionBrowser` directly via a `nfc_binding_uid_hex` flag on `FlipDeckUi`
  that repurposes OK to "bind here" instead of "send this" — simpler than a
  parallel state, and the existing views already do everything binding needs.
- `nfc_bridge.c` exposes `nfc_bridge_poll_tag()` (called once per main-loop
  tick) rather than a raw `NfcBridgeTagCallback`. NFC detection callbacks
  run on the NFC worker thread, not the UI thread, so handing UI state
  straight to a hardware callback would be a cross-thread mutation bug; the
  poll-based API latches the result behind a mutex instead.

## What this is, and isn't

FlipDeck today has two *output* surfaces: USB HID keystrokes (`usb_hid.c`) and
a UART bridge to the WiFi Dev Board (`uart_bridge.c`). NFC would add a third
*input* surface — a physical trigger, alongside the existing button
navigation, for firing an action. It does not touch USB or Bluetooth; the
Flipper has no BLE HID support in this codebase today (see `usb_hid.h`,
`uart_bridge.h` — nothing else exists). NFC and USB HID are independent and
composable: tap a tag → FlipDeck resolves it to an action → sends over
whichever `target` that action already specifies.

## UX flow

1. Category browser gets a third synthetic row (alongside the existing
   `* Favorites` row from the last PR): **`~ NFC Scan`**, always present.
2. Selecting it enters a new state, `FlipDeckState_NfcScan`, which shows
   "Hold tag to back of Flipper" and starts the NFC field.
3. On tag detect:
   - **Known UID** (found in the mapping file) → jump straight to the
     existing `FlipDeckState_SendConfirm` screen for the mapped action,
     exactly as if the user had picked it from the action browser. The
     command/label is shown and OK is still required — **NFC-triggered
     sends always require the confirm screen, regardless of the
     `confirm_before_send` setting or the long-press quick-send shortcut.**
     This is the one safety call worth being explicit about: an
     NFC UID is trivially cloneable (any $5 reader/writer or another
     Flipper), so physical tag possession is a much weaker signal of
     intent than a person consciously holding OK on a chosen list item.
     The confirm screen is what keeps "someone waved a tag near my
     Flipper" from being equivalent to "I picked this command."
   - **Unknown UID** → show "Unknown tag — bind it?" with Back to cancel
     or OK to jump into the category/action browser to pick which action
     this tag should map to (writes the mapping on selection).
4. Back from the scan screen stops the NFC field and returns to the
   category browser, same as any other row.

## Data model

New file, not part of `settings.json` (this is data, not a preference, and
can grow independently): `/ext/apps_data/flipdeck/nfc_tags.json`

```json
{
  "tags": [
    { "uid": "04A1B2C3", "category_id": "git", "label": "Git Status" }
  ]
}
```

- `uid`: uppercase hex string of the tag's UID bytes (NFC-A/ISO14443 UIDs are
  4, 7, or 10 bytes — store the hex form, not raw bytes, since it round-trips
  through JSON cleanly and is what `nfc_device_get_uid()` gives you the
  length for).
- `category_id` + `label`: same shape as `FlipDeckFavorite` (added in the
  favorites PR) — deliberately reused rather than inventing a new pairing
  scheme. A mapping is "a UID that resolves to a favorite-shaped reference,"
  so the action itself is always re-read fresh from the category file, never
  duplicated/stale.

```c
// profile_manager.h
#define FLIPDECK_MAX_NFC_TAGS 16
#define FLIPDECK_NFC_UID_HEX_LEN 21  // 10 bytes -> 20 hex chars + NUL

typedef struct {
    char uid_hex[FLIPDECK_NFC_UID_HEX_LEN];
    char category_id[32];
    char label[64];
} FlipDeckNfcTag;
```

Kept separate from `FlipDeckSettings` (unlike favorites) because:
`settings.json` is rewritten wholesale on every settings change, and 16
tag mappings at ~90 bytes JSON each would nearly double that file's size
for no reason — favorites already share `settings.json` because they're
few (max 6) and conceptually a preference; tags are a growing dataset.

```c
// profile_manager.h — mirrors the favorites API added previously
bool profile_manager_load_nfc_tags(FlipDeckNfcTag* tags, uint32_t* count);
bool profile_manager_save_nfc_tags(const FlipDeckNfcTag* tags, uint32_t count);
const FlipDeckNfcTag* profile_manager_find_nfc_tag(
    const FlipDeckNfcTag* tags, uint32_t count, const char* uid_hex);
bool profile_manager_hex_encode_uid(const uint8_t* uid, size_t uid_len, char* out, size_t out_size);
```

`profile_manager_hex_encode_uid` is the one genuinely new piece of logic
(favorites/settings parsing already has a template to copy for the JSON
array load/save). It's pure string logic — fully host-testable without
hardware, same as the favorites toggle logic was.

## New module: `nfc_bridge.c` / `nfc_bridge.h`

Same shape as `uart_bridge.c` — a thin wrapper isolating the hardware calls
from the UI so `flipdeck_ui.c` never touches `Nfc*`/`NfcScanner*` directly:

```c
// nfc_bridge.h
typedef void (*NfcBridgeTagCallback)(const char* uid_hex, void* context);

bool nfc_bridge_init(void);
void nfc_bridge_deinit(void);
void nfc_bridge_start_scan(NfcBridgeTagCallback callback, void* context);
void nfc_bridge_stop_scan(void);
```

Internally: `nfc_bridge_start_scan` calls `nfc_scanner_alloc(nfc)` +
`nfc_scanner_start()`. The scanner callback fires
`NfcScannerEventTypeDetected` with a `protocol_num`/`protocols[]` list —
that's protocol detection, not a full read. To get the UID, on detection
start an `NfcPoller` for the first detected protocol, read once, then pull
the device via `nfc_poller_get_data()` → `nfc_device_get_uid(device, &len)`,
hex-encode it, and hand that string up through `NfcBridgeTagCallback`. Stop
both scanner and poller once a UID is delivered (single-shot per screen
visit, not continuous — re-entering the NFC scan screen restarts it).

This file is the one part of this spec that can't be host-tested at all —
`Nfc`/`NfcScanner`/`NfcPoller` have no meaningful stub semantics, so
`nfc_bridge.c` needs on-device verification the way `usb_hid.c`'s actual
USB transmission does today (the host tests only cover `key_name_to_code`
et al., not real HID I/O).

## State machine changes

```c
// flipdeck_app.h
typedef enum {
    FlipDeckState_Idle,
    FlipDeckState_CategoryBrowser,
    FlipDeckState_ActionBrowser,
    FlipDeckState_NfcScan,        // new
    FlipDeckState_NfcBind,        // new — "assign this tag to an action" sub-flow
    FlipDeckState_SendConfirm,
    FlipDeckState_LongSnippetWarning,
    FlipDeckState_Settings,
} FlipDeckState;
```

`FlipDeckState_NfcScan` draws the "hold tag" prompt and owns the
scanner/poller lifecycle (start on enter, stop on Back or on successful
detect). `FlipDeckState_NfcBind` is entered only for an unknown UID and
reuses the category/action browser views in a "picking mode" — on OK over
an action, write the new `FlipDeckNfcTag` entry and return to
`NfcScan` (so you can bind several tags in one sitting) rather than
kicking back out to the category browser each time.

## Safety checklist (mirrors the existing safety-rules.json posture)

- NFC-triggered sends go through `profile_manager_validate_action()` exactly
  like every other send path — no separate/weaker check.
- NFC-triggered sends **always** land on `SendConfirm`, never quick-send,
  regardless of settings. This is the one deliberate asymmetry with the
  button-driven flow and should be called out in `docs/flight_manual.md`
  and `CLAUDE.md`'s safety section if implemented.
- `FLIPDECK_MAX_NFC_TAGS` bounds the mapping file the same way
  `FLIPDECK_MAX_FAVORITES` bounds favorites — no unbounded growth on SD
  card corruption or a malformed file.
- Binding a tag never edits `safety-rules.json` or bypasses the audit —
  it only creates a `(uid → existing category_id/label)` pointer.

## Build/config note

`nfc`, `nfc_scanner`, `nfc_poller`, `nfc_device` live in the firmware's NFC
library, which is a real dependency, not auto-available like `furi`/`gui`.
`application.fam` needs the relevant `requires` entry (check current
firmware's `nfc` app manifest for the exact library target name) or the
`fbt`/`ufbt` build will fail to link — this is worth confirming against a
real firmware checkout before writing code, since manifest dependency names
do shift between firmware versions.

## Suggested phasing

1. **Phase 1** (this spec): read-only binding — map a tag to an existing
   action, tap to jump to confirm. Everything above.
2. **Phase 2** (future, not spec'd here): write FlipDeck's own blank
   NFC tags via `nfc_device_save()` so a tag can be a physical macro key
   with a printed label, rather than only recognizing tags you already own.
