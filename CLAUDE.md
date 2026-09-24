# CLAUDE.md

Guidance for Claude Code (claude.ai/code) in this repository.

## Project

FlipDeck Mission Control: a **Mac app** (Swift/SwiftUI) that monitors a developer's
environment, plus a **Flipper Zero app** (C, uFBT) that displays it and requests a few
vetted actions over Bluetooth LE. The Mac is the brains; the Flipper is a thin terminal.
Read `ARCHITECTURE.md` first, then `docs/protocol.md` (the contract between the two).

`web/`, `desktop_helper/`, `sd_card/` and `safety-rules.json` belong to the **legacy**
keystroke-deck product (see `docs/AUDIT.md`). Don't extend them. Vercel still deploys
`web/`, so don't delete them without the owner's go-ahead.

## Commands

```bash
# Mac (from mac/)
swift build                     # all targets; SwiftUI/CoreBluetooth code compiles only on macOS
swift test                      # FlipDeckCoreTests (runs on Linux and macOS)
swift test --filter InteropTests
swift run flipdeck-headless --root ~/Developer --once --frames
scripts/build-app.sh            # → build/FlipDeck.app (macOS only)

# Flipper (repo root)
make -C src/tests/host run      # protocol/state host tests (ASan/UBSan) + golden vectors
ufbt                            # build dist/flipdeck.fap
scripts/check_flipper_sdk.sh    # type-check vs real firmware headers + exported-symbol check
python3 scripts/gen_protocol_vectors.py   # regenerate docs/protocol-vectors.txt
```

In the cloud container there's no local Swift toolchain. Use the official image:
`docker run --rm -v "$PWD":/w -w /w/mac mirror.gcr.io/library/swift:6.1-noble swift test`
(`apt-get install lsof perl` inside it for the process tests). The uFBT SDK host is
blocked there too. Use `scripts/check_flipper_sdk.sh` (clones firmware from GitHub), or
`ufbt update --local <sdk.zip> --hw-target f7` with a Momentum SDK zip from GitHub releases.

## Architecture rules

- **FlipDeckCore stays platform-independent** (Foundation only). Every OS touchpoint
  goes behind a protocol: `CommandRunner`, `SystemEffects`, `SecretStore`,
  `MachineMetricsProvider`, `FlipperTransport`, `HTTPClient`, `MacNotifier`. macOS
  implementations live in `FlipDeckMacPlatform`, with every file wrapped in `#if os(macOS)`.
- **State in, events out.** Providers produce `EngineState`. `EventDiffer` derives events
  from old→new state, and the first observation of any section is a silent baseline.
  Every mutation goes through `FlipDeckEngine.apply`.
- **Never fabricate state.** If something can't be detected reliably (agent success or
  failure, test results), mark it unsupported (`AgentCapabilities`) instead of inferring it.
- **Actions are an allowlist.** `ActionKind` is the complete set, and there is no "run
  command". The Flipper sends only action ids from the Mac's current snapshot table.
  `ActionExecutor` re-validates the target against live state (PID + start time for
  processes, known URLs only) before acting. Destructive kinds require confirmation.
- **Notification routing** is data (`NotificationRules.defaultTable`): Activity always,
  plus Flipper and/or Mac per event type.
- **Secrets** go only in the Keychain via `SecretStore`. Never put them in settings,
  logs, errors (`Redact`), or anything sent to the Flipper. Git remote URLs are stripped
  of credentials.

## Protocol (FDP/1) rules

- Change `docs/protocol.md`, Swift (`mac/Sources/FlipDeckCore/Protocol/`) and C
  (`src/fd_proto.c`, `src/fd_state.c`) together. Add golden vectors via
  `scripts/gen_protocol_vectors.py`. Both test suites must pass them.
- Record limits and field lengths are mirrored in `FlipperLimits` (Swift) and
  `src/fd_state.h` (C). Changing them is a protocol change.
- Frames are ≤ 240 bytes, printable ASCII, and carry a CRC-16. Snapshots are staged
  and committed atomically. Everything must be safe to deliver twice.

## Flipper app rules

- The stack is 4 KB, and the GUI draw callback runs on the GUI thread, so large
  buffers live on the heap (`FdApp`, `FdUi.rows`). Shared state is guarded by `FdApp.mutex`.
- BLE: FlipDeck uses its **own** profile template (`fd_ble.c`). The stock
  `ble_profile_serial` gets hijacked for RPC by the BT service on every connect. Keep
  the startup/teardown order (disconnect → keys path → profile start; disconnect →
  default keys → restore default).
- `fd_proto.c` / `fd_state.c` must not include SDK headers, so they stay host-testable.
- Firmware builds use `-Werror` with format-truncation checks. `check_flipper_sdk.sh`
  compiles with `-Os`, as uFBT does, so those warnings surface locally.

## Conventions

- Swift: Swift 5 language mode (tools 5.10), macOS 14 deployment target, 4-space
  indent. Match the surrounding comment density: comments explain *why*.
- C: Flipper SDK style: `snake_case` functions, `PascalCase` types, `Fd` prefix.
- UI copy: plain, specific, no fake data. Empty states explain what would appear and why.

## CI

The session tokens used so far lack GitHub's `workflow` scope, so the intended
workflows live in `ci/` (see `ci/README.md`). The existing `.github/workflows/test.yml`
already runs `make -C src/tests/host run`, and `build-fap.yml` builds the new `.fap`
on master.
