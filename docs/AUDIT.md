# FlipDeck Repository Audit (pre-pivot)

Audit date: 2026-09-24 · Base commit: `26b96bb`

This audit was done before the pivot to **FlipDeck Mission Control**. It covers
what existed, how well it worked, and what happens to each part.

## 1. What existed

| Area | Path | Stack | Size | Purpose |
|---|---|---|---|---|
| Flipper app | `src/`, `application.fam` | C, uFBT | ~3.3k LOC | USB HID "command deck": browse JSON profiles from SD card, type commands as keystrokes. Also had NFC and Sub-GHz triggers and a WiFi-devboard UART target. |
| Flipper host tests | `src/tests/host/` | C + stubs | 220 assertions | Tested profile JSON parsing, the safety regexes, and the HID keymap. |
| Web installer | `web/` | Next.js 16, Vitest | ~1.5k LOC | Browsed keystroke profiles; installed the `.fap` and profiles through Web Serial or a ZIP download. Deployed to Vercel. |
| Desktop helper | `desktop_helper/` | Node/TS CLI, Jest | ~1.2k LOC | `flipdeck profile validate/new/edit/...` for keystroke profiles. |
| Profiles | `sd_card/`, `safety-rules.json` | JSON | — | Keystroke command profiles and dangerous-command regexes. |
| CI | `.github/workflows/` | GH Actions | — | Vitest, Jest, C host tests. A uFBT build that commits `flipdeck.fap` into `web/public` on `master`. |
| Misc | `CMakeLists.txt`, `.agents/skills/` | — | — | `CMakeLists.txt` called a CMake function that doesn't exist (`FlipperApp`). It was never a working build file. `.agents/skills` is vendored Vercel agent skills. |

**Not present:** there was no Mac application of any kind, and no Bluetooth code
on either side. The only Mac↔Flipper channel was USB HID keystroke injection
(one-way, host-blind) plus Web Serial for installing files.

## 2. Test status at audit time

All existing suites passed:

- C host tests: 108 + 55 + 5 + 52 assertions, 0 failures
- desktop_helper (Jest): 54/54
- web (Vitest): 76/76

The `.fap` itself could not be built in this environment (the uFBT SDK host is
blocked by network policy). It had never been verified on hardware, and the
repo's own docs said so.

## 3. Quality findings (old Flipper app)

These findings matter for deciding what to reuse:

1. **The app could never exit.** `flipdeck_app()` runs `while(true) { loop; delay }`,
   so `flipdeck_app_free()` is unreachable. No input path breaks the loop:
   Back at the top level does nothing. It also polled and redrew every 50 ms,
   with no event queue.
2. **USB HID was probably never working.** `usb_hid.c` calls `furi_hal_hid_kb_press()`
   but never switches the USB stack into HID mode (`furi_hal_usb_set_config(&usb_hid, …)`
   is never called). While the Flipper sits in its default CDC mode,
   `furi_hal_hid_is_connected()` is false, so the product's core feature would
   not have worked on a real device.
3. The NFC and Sub-GHz bridges were type-checked but never built or run
   (as their own comments note).
4. The JSON "parser" is `strstr`-based key lookup. It works for fixtures but is
   fragile for real JSON.

## 4. Relevance to the new product

The pivot turns the architecture around. **Before:** the Flipper held the
logic (profiles) and pushed keystrokes into a computer it could not see.
**After:** the Mac holds the logic and state, and the Flipper is a thin, read-mostly
terminal that asks the Mac to run a small set of vetted actions.

Keystroke injection is the opposite of the new safety model ("never allow
arbitrary remote shell execution"). Typing `git push\n` into whatever window has
focus *is* arbitrary remote shell execution, and nothing confirms which window
that is.

## 5. Keep / refactor / remove / replace

| Component | Decision | Reason |
|---|---|---|
| `src/` Flipper keystroke app (UI, profile manager, HID, NFC, Sub-GHz, UART) | **Removed** (still in git history at `26b96bb`) | Obsolete product model; does not exit; HID path most likely non-functional. Nothing in it is the right shape for a state-mirroring BLE client. |
| `src/tests/host/` harness pattern (stub headers + `make run`) | **Kept (pattern)** | Reused for `flipper/tests/`, which tests the new protocol and state store on the host. |
| Root `application.fam` | **Replaced** by `flipper/application.fam` | New app, same `appid` (`flipdeck`). |
| `CMakeLists.txt` | **Removed** | Never a working build file. |
| `.github/workflows/build-fap.yml` | **Replaced** | It committed binaries into `web/public` on master. It now builds `flipper/` on PRs and pushes, uploads the `.fap` as an artifact, and does not commit. |
| `.github/workflows/test.yml` | **Refactored** | Adds Swift (macOS) and Flipper host-test jobs. The web and desktop_helper jobs stay while those packages exist. |
| `web/` installer | **Kept for now, legacy, removal recommended** | Vercel deploys it (root dir `web`). Deleting it would break a live deployment, which is the owner's call. It installs the *old* app and profiles, so it should be retired or rewritten as a download page for the Mac app and new `.fap`. |
| `desktop_helper/` CLI | **Kept for now, legacy, removal recommended** | Only manages keystroke profiles. Its role is superseded by the Mac app. |
| `sd_card/`, `safety-rules.json` | **Kept for now, legacy** | Only consumed by `web/` and `desktop_helper/`. Remove together with them. |
| Web Serial installer knowledge (`web/src/lib/flipper-serial.ts`) | **Reference only** | Could later power "Install Flipper app" from the Mac app over USB CDC. Not needed for the MVP. |
| `.agents/skills/` | Untouched | Unrelated vendored tooling. |

## 6. Environment notes for whoever builds next

- A Swift toolchain is not installed in the cloud container, but the official
  `swift:6.1-noble` image pulls from `mirror.gcr.io`. The platform-independent core
  (`mac/Sources/FlipDeckCore`) is built and tested on Linux that way. The macOS-only
  targets (SwiftUI, CoreBluetooth, Keychain, IOKit) are compiled only by the
  `macos` CI job.
- The Flipper SDK host (`update.flipperzero.one`) is blocked, but
  `github.com/flipperdevices/flipperzero-firmware` clones fine. `flipper/tools/check.sh`
  type-checks every Flipper source against the real firmware headers with
  `arm-none-eabi-gcc` and checks each SDK symbol used against `api_symbols.csv`.
  The real uFBT build runs in CI.
