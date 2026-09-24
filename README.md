# FlipDeck Mission Control

**Your Mac's development environment, on your Flipper Zero.**

FlipDeck is a Mac app plus a Flipper Zero app. The Mac watches what matters:
projects and their Git state, dev servers, coding agents, deployments. The
Flipper shows it, so you can walk away from your desk, glance at it, and see
what's running, what failed, and what's waiting for you. A few vetted actions
(Open on Mac, Open localhost, Open logs, Stop server) run on the Mac at the
press of a button.

The Mac does all the work. The Flipper gets a small, normalized state snapshot
over Bluetooth LE. It can never run commands on the Mac, only ask for actions
the Mac put on its screen.

```
FLIPDECK          ●        PROJECTS          ●        FLIPDECK          ●
─────────────────          ─────────────────          ─────────────────
! 1 Attention              flipdeck      ! ●          main
Mac               ●        interlockd      ●          Git        Clean ✓
Projects          4        website       * ○          Dev        :3000 ●
Agents            1                                   Deploy      Live ✓
Services          3                                   Open on Mac      >
```

## What it does today (Phase 1)

| Area | Detected from | Reported |
|---|---|---|
| **Projects** | Folders you choose (e.g. `~/Developer`), scanned up to 3 levels deep, skipping `node_modules`, build output, etc. | Name, path, branch, clean/dirty counts, ahead/behind, last commit, remote (credentials stripped), framework, runtime, package manager |
| **Dev servers** | `lsof` listening TCP ports ≥ 1024 on processes you own that are a known dev runtime (Node, Bun, Deno, Python, Ruby, Docker, …) or run from inside a project | Port(s), process, PID, framework (Vite, Next.js, Django, …), project, uptime |
| **Builds & tests** | `ps` (vitest, jest, pytest, cargo/swift/go test, next/vite build, tsc --watch, docker compose, …) | Running, project, elapsed |
| **Agents** | `ps`: Claude Code and Codex | Running, elapsed, project. **Not** success/failure or "waiting for input": the process table can't tell, so FlipDeck says "finished" and doesn't guess. |
| **Vercel** | `.vercel/project.json` (from `vercel link`) plus a token in the Keychain | Deployments: started / live / failed / canceled, logs link |
| **Machine** | Mach, sysctl, IOKit, Network.framework | CPU, memory, battery, network, uptime |

Everything becomes a normalized **event** (`server.started`, `agent.exited`,
`git.changed`, `deployment.failed`, …) with a severity (`info`, `success`,
`warning`, `error`, `action_required`). A deterministic rules table decides
where each event goes: Activity only, or also a Flipper alert (vibrate), or
also a Mac notification. For example, a failed deployment goes everywhere and
a successful one goes to Activity.

## Install

**Mac app** (macOS 14+, Xcode 15+ command-line tools):

```sh
cd mac
scripts/build-app.sh          # → mac/build/FlipDeck.app
open build/FlipDeck.app
```

Then in **Settings**: add your project folders, pick your editor, and
optionally paste a Vercel token (stored in the Keychain).

**Flipper app:**

```sh
pip install ufbt
ufbt                           # → dist/flipdeck.fap
ufbt launch                    # with the Flipper connected over USB
```

Or copy `dist/flipdeck.fap` to `/ext/apps/Tools/` on the SD card.

**Pairing:** open FlipDeck on the Flipper, then on the Mac. The Mac finds the
Flipper (it advertises as `FlipDeck <name>`). On first connection macOS asks
for a PIN, so enter the one shown on the Flipper. FlipDeck uses its own BLE profile
and bonding keys, so it doesn't disturb the Flipper mobile app's pairing. The
phone app works normally again once you quit FlipDeck.

## Repository

```
mac/                 Swift package: FlipDeckCore (platform-independent engine),
                     FlipDeckMacPlatform (CoreBluetooth, Keychain, IOKit, AppKit),
                     FlipDeckApp (SwiftUI), flipdeck-headless (CLI)
src/, application.fam  Flipper Zero app (C, uFBT)
docs/protocol.md     FDP/1 wire protocol, the contract between the two
docs/AUDIT.md        audit of the pre-pivot codebase and what was kept or removed
ARCHITECTURE.md      architecture, key decisions, phased plan
ci/                  proposed GitHub workflows (need a maintainer to install)
web/, desktop_helper/, sd_card/   legacy keystroke-deck installer (see docs/AUDIT.md)
```

## Development

```sh
# Mac core: builds and tests anywhere Swift runs (Linux too)
cd mac && swift test
swift run flipdeck-headless --root ~/Developer --once --frames   # see what the engine sees

# Flipper protocol + state: host tests (ASan/UBSan, shared golden vectors)
make -C src/tests/host run

# Flipper app against real firmware headers, without the uFBT SDK download
scripts/check_flipper_sdk.sh
```

`flipdeck-headless --frames` prints the exact FDP/1 frames a Flipper would
receive, which is the fastest way to debug detection on a real machine.

## Status and known gaps

Be clear about what has and hasn't been proven:

- **Verified in CI-like conditions:** the Swift core (71 tests, including real
  `git` repos, real listening sockets and processes, an end-to-end engine run, and
  the Swift session talking to the Flipper's C state machine over pipes); the C
  protocol/state code (153 checks under sanitizers); the Flipper app type-checks
  clean against current `flipperzero-firmware` headers, and a full uFBT build and
  link passes against Momentum's SDK.
- **Not yet verified:** the SwiftUI/CoreBluetooth/Keychain code compiles only on
  macOS, and runs in the `mac-macos` job in `ci/test.yml` once that's installed.
  Nothing has run on a real Flipper or over real Bluetooth yet. Pairing,
  flow-control behavior under load, and reconnect timing need a hardware pass.
- **By design, not available:** agent success/failure/waiting states, and test
  pass/fail. These need a cooperating source (Phase 2: a local event-ingest socket
  that Claude Code hooks and test wrappers can report to) rather than guessing
  from process lifetimes.

See `ARCHITECTURE.md` → Phases for what comes next.

## License

MIT, see [LICENSE](LICENSE).
