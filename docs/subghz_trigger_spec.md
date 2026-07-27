# Spec: Sub-GHz RF Remote Triggers for FlipDeck

Status: **Phase 1 implemented** (`flipdeck_app.h`/`.c`, `flipdeck_ui.h`/`.c`,
`profile_manager.h`/`.c`, `subghz_bridge.h`/`.c`, `application.fam`). A third
trigger surface alongside button navigation and NFC tags (`nfc_trigger_spec.md`).

`subghz_bridge.c` was syntax/type-checked with `gcc -fsyntax-only -Wall
-Wextra -Wpedantic` against the actual, current firmware headers (`furi_hal_
subghz.h`, `lib/subghz/{devices/cc1101_configs,environment,receiver,
subghz_worker,protocols/base,subghz_protocol_registry}.h`, plus their full
transitive dependency chain — over a dozen real headers fetched live from
`flipperdevices/flipperzero-firmware`, not guessed) and came back with zero
warnings attributable to this file's own code. Its control flow was also
cross-checked against `applications/main/subghz/helpers/subghz_txrx.c`, the
real firmware's own internal helper for doing exactly this (private to that
app, not includable from an external `.fap`, but read in full as a reference).
This is a *stronger* verification pass than NFC's — resolving it required
working through the real Sub-GHz header dependency tree far enough to
concretely answer two open questions that would otherwise have been guesses
(see "Build/config note" below) — but it is still not a real build. No
`fbt`/`ufbt` toolchain is available in this environment (its SDK download is
blocked by the sandbox's network policy — confirmed via the proxy status
endpoint, `update.flipperzero.one` → 403), so nothing here has been linked,
flashed, or run against real hardware. See `subghz_bridge.c`'s file header
for exactly what remains unconfirmed.

One deliberate deviation from the original plan, made during implementation:
`subghz_bridge.c` calls `furi_hal_subghz_*` (the direct HAL for the Flipper's
built-in CC1101) rather than the newer `subghz_devices_*` abstraction the
real firmware app uses. `subghz_devices_*` exists to support pluggable
*external* CC1101 modules, which transitively pulls in the firmware's ELF
plugin-loader subsystem (`lib/flipper_application/...`) — real complexity
this feature doesn't need, since it only ever targets the Flipper's built-in
radio. `furi_hal_subghz_*` is the lower layer `subghz_devices_*` itself calls
into for the internal radio, so this isn't a shortcut around anything, just
skipping an abstraction layer with nothing to offer a fixed-hardware,
RX-only, internal-radio-only use case. This also meaningfully *lowers* the
`application.fam` linkage risk relative to what was originally anticipated
(see "Build/config note").

## What this is, and isn't

FlipDeck's third *input* trigger surface, alongside button navigation and NFC
tags. **Receive only — deliberately, structurally, not just "not implemented
yet."** No transmit function is called anywhere in `subghz_bridge.c`. This
means: (a) no regional TX power/frequency regulation applies, because
nothing is ever transmitted; (b) this feature *cannot* be used to replay or
clone a signal against a garage door, gate, or car, even by accident, because
replay requires transmission and this bridge has no transmit code path to
repurpose.

Binding uses the firmware's full/default protocol registry
(`subghz_protocol_registry`) and matches on **exact-string equality** of the
decoded protocol text (`subghz_protocol_decoder_base_get_string()`, which
works identically across every decoder in the registry — no per-protocol
accessor code needed). Fixed-code remotes (common cheap doorbells,
garage/gate remotes, RF relay switches) decode to byte-identical text on
every press and bind reliably. Rolling-code remotes (many modern car/garage
keyfobs) decode to *different* text every press by design, so they simply
never re-match after the first bind. This is the correct, safe failure mode,
not a bug to fix later: a design that could match/replay a rolling code
would be a real security problem, and this one structurally cannot.

## UX flow

1. Category browser gets a fourth synthetic row, after the existing
   `* Favorites` and `~ NFC Scan` rows: **`^ Sub-GHz Scan`**. Unlike NFC's
   always-shown row, this one is conditional on `FlipDeckApp.subghz_available`
   (set from `subghz_bridge_init()`'s return value) — since Sub-GHz's
   real-firmware linkage is a bigger open question than NFC's turned out to
   be, a row that's silently dead on some builds would be worse than no row.
2. Selecting it enters `FlipDeckState_SubghzScan`, which shows "Press the
   remote's button... (receive only)" and starts the radio listening at
   433.92MHz with an OOK 650kHz async preset (`subghz_device_cc1101_preset_
   ook_650khz_async_regs` — the real firmware's own preset register table,
   fetched from `lib/subghz/devices/cc1101_configs.h`).
3. On signal decode:
   - **Known signature** (found in the mapping file) → jump straight to
     `FlipDeckState_SendConfirm` for the mapped action, exactly like a known
     NFC tag. **Sub-GHz-triggered sends always require the confirm screen**,
     for an even stronger reason than NFC's UID-cloneability: RF is
     receivable from across a room with zero physical contact with the
     device at all — an even weaker signal of deliberate intent than a
     person tapping an NFC tag to the Flipper.
   - **Unknown signature** → hand off into the category/action browser in
     binding mode to pick what it should map to, identical flow to an
     unknown NFC tag.
4. Back from the scan screen stops the radio and returns to the category
   browser.

## Data model

New file, sibling to `nfc_tags.json`, not part of `settings.json` (same
reasoning as NFC — a growing dataset, not a preference):
`/ext/apps_data/flipdeck/subghz_remotes.json`

```json
{ "remotes": [ { "signature": "F9E6E6EF197C2B25", "category_id": "git", "label": "Git Status" } ] }
```

**Fingerprint = 64-bit FNV-1a hash of the decoded string, not the raw
string.** The decoded text's length isn't bounded by anything in this
codebase's control (protocol name + key/serial/bit-count, variable length),
and truncating a fixed-size buffer risks two different remotes silently
colliding on a shared prefix — worse than a hash collision because it's
systematic, not random. A fixed-width hash also keeps the struct the same
shape as `FlipDeckNfcTag` and avoids needing JSON string-escaping logic that
doesn't exist yet in this codebase's mini-JSON writer (decoded text could
contain `"`/`\`).

```c
// profile_manager.h
#define FLIPDECK_MAX_SUBGHZ_REMOTES 16
#define FLIPDECK_SUBGHZ_SIG_HEX_LEN 17  // 64-bit FNV-1a -> 16 hex chars + NUL

typedef struct {
    char signature_hex[FLIPDECK_SUBGHZ_SIG_HEX_LEN];
    char category_id[32];
    char label[64];
} FlipDeckSubghzRemote;

bool profile_manager_load_subghz_remotes(FlipDeckSubghzRemote* remotes, uint32_t* count);
bool profile_manager_save_subghz_remotes(const FlipDeckSubghzRemote* remotes, uint32_t count);
const FlipDeckSubghzRemote* profile_manager_find_subghz_remote(
    const FlipDeckSubghzRemote* remotes, uint32_t count, const char* signature_hex);
bool profile_manager_hash_subghz_signature(const char* text, char* out, size_t out_size);
```

`profile_manager_hash_subghz_signature` (standard FNV-1a, seed
`0xcbf29ce484222325ULL`, prime `0x100000001b3ULL`) is pure string logic,
fully host-tested with known input→output vectors — same host-testability
story as `profile_manager_hex_encode_uid` had for NFC.

## New module: `subghz_bridge.c` / `subghz_bridge.h`

Same shape as `nfc_bridge.c` — mutex-latched single-shot poll API, hardware
fully isolated from `flipdeck_ui.c`:

```c
// subghz_bridge.h
bool subghz_bridge_init(void);
void subghz_bridge_deinit(void);
void subghz_bridge_start_scan(void);
void subghz_bridge_stop_scan(void);
bool subghz_bridge_poll_signal(char* signature_hex_out, size_t out_size);
```

Pipeline per scan session: `furi_hal_subghz_reset()` → `idle()` →
`load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs)` →
`set_frequency(433920000)` → `flush_rx()` → allocate `SubGhzReceiver` (via a
`SubGhzEnvironment` allocated once at init, holding the protocol registry) →
allocate `SubGhzWorker`, wire its pair/overrun callbacks directly to
`subghz_receiver_decode`/`subghz_receiver_reset` (cast to the worker's
callback types — the same pointer-cast trick the real firmware's own
`subghz_txrx.c` uses, since the worker's context becomes the receiver
pointer) → `furi_hal_subghz_start_async_rx(subghz_worker_rx_callback,
worker)` → `subghz_worker_start()`. On a decode, the receiver callback (worker
thread) hashes the decoded text and latches it behind a mutex — same pattern
as `nfc_bridge.c`'s `pending_uid_hex`/`pending_uid_ready` — for
`subghz_bridge_poll_signal()` to pick up on the main thread each tick.

Like `nfc_bridge.c`, this file is excluded entirely from the host test build
— `SubGhzWorker`/`SubGhzEnvironment`/`SubGhzReceiver`/`FuriHalSubGhz*` have
no meaningful stub semantics.

## State machine changes

```c
// flipdeck_app.h
typedef enum {
    FlipDeckState_Idle, FlipDeckState_CategoryBrowser, FlipDeckState_ActionBrowser,
    FlipDeckState_NfcScan, FlipDeckState_SubghzScan,   // new
    FlipDeckState_SendConfirm, FlipDeckState_LongSnippetWarning, FlipDeckState_Settings,
} FlipDeckState;
```

No separate `SubghzBind` state — same "no separate Bind state" deviation NFC
already established. Binding for *either* trigger source now shares one
generalized mechanism: `FlipDeckUi.nfc_binding_uid_hex` (NFC-only, from the
previous PR) was replaced with a tagged union,
`FlipDeckPendingBind { FlipDeckBindKind kind; char id_hex[...]; }`, so at most
one bind can be pending at a time — structurally, not just "shouldn't
happen." `flipdeck_ui_bind_selected_action()` branches on `pending_bind.kind`
to write into either `nfc_tags[]` or `subghz_remotes[]`.

## Safety checklist

- Sub-GHz-triggered sends go through `profile_manager_validate_action()`
  exactly like every other send path — no separate/weaker check.
- Sub-GHz-triggered sends **always** land on `SendConfirm`, never quick-send,
  regardless of settings — see UX flow above for why this is an even
  stronger requirement than NFC's.
- **Receive only.** No transmit call anywhere in `subghz_bridge.c` — see
  "What this is, and isn't" above for why this structurally rules out replay
  attacks, not just by policy.
- Binding matches on exact decoded-string equality, so rolling-code remotes
  simply never re-bind — a safe failure mode by construction, not a gap to
  close later.
- `FLIPDECK_MAX_SUBGHZ_REMOTES` bounds the mapping file the same way
  `FLIPDECK_MAX_NFC_TAGS`/`FLIPDECK_MAX_FAVORITES` bound their own lists.
- Binding a remote never edits `safety-rules.json` or bypasses the audit —
  it only creates a `(signature → existing category_id/label)` pointer.

## Build/config note

Two concrete unknowns from the original plan were resolved by reading real
firmware source rather than left as guesses:
- **Preset registers**: `subghz_device_cc1101_preset_ook_650khz_async_regs`,
  an `extern const uint8_t[]` declared in `lib/subghz/devices/cc1101_configs.h`.
- **Protocol registry**: `subghz_protocol_registry`, an `extern const
  SubGhzProtocolRegistry` declared in `lib/subghz/subghz_protocol_registry.h`.

The `application.fam` linkage question (whether an external `.fap` needs a
`fap_libs`/`requires` entry to resolve Sub-GHz symbols) is genuinely lower
risk than originally anticipated, precisely because this file uses
`furi_hal_subghz_*` (same HAL tier as `furi_hal_usb_hid.h`, which this app
already uses via `usb_hid.c` with no special manifest entry) instead of the
official Sub-GHz app's `subghz_devices_*` + `fap_libs=["assets", "hwdrivers"]`
+ `requires=["region"]`. Still unconfirmed without an actual link — see
`application.fam`'s comment for the full reasoning and fallback guidance.

## Suggested phasing

1. **Phase 1** (this spec): read-only binding — map a fixed-code remote's
   button to an existing action, press to jump to confirm. Everything above.
2. No Sub-GHz analog to NFC's hypothetical "Phase 2" (writing blank tags) —
   there is nothing to write; RX-only is a permanent design boundary, not a
   phase-1-only restriction that a later phase would lift.
