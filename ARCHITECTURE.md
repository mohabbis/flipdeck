# FlipDeck Mission Control — Architecture

The Mac is the brains; the Flipper Zero is a portable status-and-control
terminal. The Mac observes the developer environment, turns observations into
normalized **state** and **events**, and pushes a compact projection of that
state to the Flipper over Bluetooth LE. The Flipper renders it and can ask the
Mac to perform a small set of **explicitly supported actions**. It never sends
commands or shell text.

```
 ┌───────────────────────────── FlipDeck for Mac ─────────────────────────────┐
 │  Providers (observe)            Engine                 Consumers            │
 │  ─────────────────────          ──────────             ─────────            │
 │  ProjectScanner + GitClient ─┐                      ┌─ SwiftUI app          │
 │  ProcessScanner (ps)        ─┤  snapshot ─► diff ─► │  (Overview, Projects, │
 │  ListenerScanner (lsof)     ─┼─► EngineState  ─► Events ─► Activity log     │
 │  AgentProviders             ─┤        │          │     (persisted JSONL)    │
 │  VercelIntegration (HTTP)   ─┤        │          └─► NotificationRouter ─┐  │
 │  MachineMetrics (macOS)     ─┘        │              (deterministic rules)│  │
 │                                       ▼                                   │  │
 │  ActionExecutor ◄── ActionTable ◄── FlipperSnapshotBuilder   Mac notif ◄──┤  │
 │   (vetted kinds only)                  │                                  │  │
 │                                  FlipperSession (FDP/1 protocol) ◄────────┘  │
 │                                        │                                     │
 │                                  FlipperTransport (protocol)                 │
 │                                   ├─ BLESerialTransport (CoreBluetooth)      │
 │                                   └─ LoopbackTransport (tests)               │
 └────────────────────────────────────────┬────────────────────────────────────┘
                                          │ BLE, paired + encrypted
 ┌────────────────────────────────────────┴────────── FlipDeck for Flipper ────┐
 │  fd_ble (custom BLE profile → serial service) → fd_proto (frames, CRC)      │
 │  → fd_state (staged snapshot, atomic commit, dedupe, staleness) → fd_ui     │
 └─────────────────────────────────────────────────────────────────────────────┘
```

## Repository layout

```
mac/                         Swift package (swift-tools 5.10, macOS 14+)
  Sources/FlipDeckCore/      Platform-independent core. Builds and tests on Linux.
    Models/                  Severity, FDEvent, FDAction, Project, DevServer, AgentSession, MachineStatus
    Support/                 CommandRunner, Clock, Log (secret redaction)
    Projects/                ProjectScanner, GitClient + porcelain-v2 parser, FrameworkDetector
    Monitoring/              ProcessScanner (ps), ListenerScanner (lsof), ProcessClassifier, DevServerDetector
    Agents/                  AgentProvider protocol, Claude Code + Codex providers
    Integrations/            Integration protocol, Vercel (link discovery, API client, deployment tracking)
    Events/                  EventDiffer (state → events), ActivityLog, NotificationRules
    Actions/                 ActionKind, ActionTable, ActionExecutor + SystemEffects protocol
    Protocol/                FDP/1 wire codec, CRC-16, FlipperSession, FlipperSnapshotBuilder
    Transport/               FlipperTransport protocol, LoopbackTransport
    Persistence/             Settings, JSON file store
    Secrets/                 SecretStore protocol (+ in-memory store for tests)
    Engine/                  FlipDeckEngine: schedules providers, owns state, fans out
  Sources/FlipDeckMacPlatform/  macOS-only: CoreBluetooth transport, Keychain, machine
                                metrics (Mach/IOKit), NSWorkspace effects, notifications
  Sources/FlipDeckApp/          SwiftUI app (sidebar: Overview … Settings)
  Sources/flipdeck-headless/    CLI that runs the engine and prints state/events (Linux + macOS)
  Tests/FlipDeckCoreTests/
flipper/                     Flipper Zero .fap (uFBT project, appid "flipdeck")
  fd_proto.[ch]              frame codec + CRC (no SDK deps, host-tested)
  fd_state.[ch]              model store (no SDK deps, host-tested)
  fd_ble.[ch]                custom BLE profile + serial service transport
  fd_ui.[ch]                 screens
  flipdeck.c                 app entry, event loop
  tests/                     host tests (make -C flipper/tests run)
  tools/check.sh             type-check against real firmware headers + API symbol check
docs/protocol.md             FDP/1 wire protocol spec
docs/AUDIT.md                pre-pivot audit
```

## Key decisions

- **Native Swift/SwiftUI**, packaged with SwiftPM rather than an `.xcodeproj`, so
  the core stays portable and CI-testable. `mac/scripts/build-app.sh` builds a
  signed `.app` bundle. A bundle is needed for the Bluetooth usage string and
  for `UNUserNotificationCenter`.
- **The core is platform-independent.** Every OS touchpoint sits behind a
  protocol (`CommandRunner`, `SystemEffects`, `SecretStore`, `MachineMetricsProvider`,
  `FlipperTransport`). That lets the logic that decides what the user sees be
  tested on Linux with fixtures and real `git`/`ps`/`lsof`.
- **Monitoring shells out** to `git`, `ps` and `lsof`, at bounded intervals, with
  timeouts. These tools are stable, present on every Mac, and far cheaper to get
  right than private APIs. Scans are incremental: project discovery runs rarely,
  `git status` runs per project on a slower cadence than the process table, and
  descent stops at each repo root and skips dependency and build directories.
- **Events come from diffs.** Each provider produces state. The engine diffs
  consecutive states to emit normalized `FDEvent`s. The first observation after
  launch is a silent baseline, so already-running servers don't flood Activity
  with "started".
- **Honesty over coverage.** Agents report only what the process table proves:
  running, elapsed time, and directory. Exit status of a process FlipDeck didn't
  spawn is unknowable, so an agent that disappears is reported as `agent.exited`
  (outcome unknown), never as "completed" or "failed". Test pass/fail is not
  inferred from process lifetimes; no source for it exists in Phase 1.
- **The Flipper can only reference actions, never define them.** Each snapshot
  carries an action table (`id → kind + target`) generated by the Mac. A Flipper
  request names an action id. The Mac looks it up in its own table, re-validates
  the target (for example, that the PID is still the same dev server via its
  start time), then executes. Unknown or stale ids are rejected.
- **Transport-agnostic session.** `FlipperSession` implements handshake,
  heartbeats, snapshots, alerts, action requests and results over any
  byte-stream `FlipperTransport`.

## Bluetooth details (verified against firmware source)

- The stock BLE serial profile can't be reused. On connect, the BT service
  checks `current_profile == ble_profile_serial`, opens its own RPC session and
  overwrites the app's data callback (`applications/services/bt/bt_service/bt.c`).
- FlipDeck defines its **own profile template** (the pattern the firmware's HID
  profile uses). It starts the exported serial GATT service plus Device Info and
  Battery, uses its own advertising name (`FlipDeck <name>`), a distinct MAC, and
  its own bonding-key file. The phone-app pairing is untouched, and the BT
  service never opens RPC on it.
- Serial service: RX `19ed82ae-ed21-4c9d-4145-228e62fe0000` (Mac writes),
  TX `…61fe0000` (Flipper indicates), flow control `…63fe0000` (uint32 BE, bytes
  the Flipper can accept). RX and TX require an **authenticated (paired) link**.
  macOS shows the pairing prompt and the Flipper shows the PIN.
- The Flipper app can't observe MTU (GAP events go to the BT service), so the Mac
  sends its `maximumWriteValueLength` in `HELLO`. The Flipper chunks indications
  to that size, and to 20 bytes until then.

## Phases

**Phase 0: audit and plan** ✅ (`docs/AUDIT.md`, this file, `docs/protocol.md`)

**Phase 1: reliable core pipeline (this milestone)**
1. Core models, event model, severity, and action model.
2. Project discovery with bounded scanning and Git state (branch, dirty, ahead/behind, last commit, remote).
3. Process scan, dev-server detection (listening ports + cwd), project association.
4. Agent providers: Claude Code and Codex (running, elapsed, directory, exited).
5. Event differ, persistent activity log, deterministic notification rules.
6. Vercel integration end to end: `.vercel/project.json` linking, Keychain token, deployments → events, Open Logs.
7. FDP/1 protocol and session (handshake, snapshot/commit, heartbeat, staleness, dedupe, versioning, action request/result).
8. CoreBluetooth transport; SwiftUI app with all seven sections backed by real data or explicit empty states.
9. Flipper app: BLE transport, protocol, state store, and Home / Projects / Project / Services / Agents / Activity / Alert / Confirm screens. Actions: Open on Mac, Open localhost, Open logs, Stop server (confirmed).
10. CI: Swift build and tests on macOS, Swift core tests on Linux, Flipper host tests, and a real uFBT build.

**Phase 2: depth**
- Local event ingest (Unix socket plus a `flipdeck notify` CLI). This lets Claude Code
  hooks (`Stop`, `Notification`) report **completed / waiting** agent states and lets
  test runs report pass/fail. These are real sources, not inference.
- GitHub integration (Actions runs, PR checks) on the same `Integration` protocol.
- FSEvents-driven Git refresh instead of interval polling.
- Retry deployment (Vercel redeploy API), once it has been verified end to end.
- Mac-side Flipper app installer over USB CDC.

**Phase 3: polish**
- Smarter notification prioritization (quiet hours, focus-aware).
- Per-project pinning and ordering, synced to the Flipper.
