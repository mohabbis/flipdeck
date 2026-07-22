# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FlipDeck is a USB command deck for Flipper Zero. It has three distinct sub-projects that must be understood as a whole:

1. **Flipper Zero native app** (`/src/`) — C app running on the device, built with uFBT
2. **Web installer** (`/web/`) — Next.js 16 app for browsing profiles and downloading install packs
3. **Desktop helper CLI** (`/desktop_helper/`) — Node.js/TypeScript CLI (`flipdeck` command) for profile management

## Commands

### Web installer (`/web/`)

```bash
npm install
npm run dev          # Dev server on localhost:3000
npm run build        # Production build (runs prebuild first to copy profiles)
npm run start        # Start standalone server bound to 0.0.0.0
npm run lint         # ESLint
npm test             # Vitest
```

Run a single test:
```bash
npx vitest run src/__tests__/specific.test.ts
```

### Desktop helper (`/desktop_helper/`)

```bash
npm install
npm start            # Run CLI with ts-node (development)
npm run build        # Compile TypeScript → dist/
npm test             # Jest
npm run format:profiles  # Prettier-format all SD card profile JSONs
```

Run a single test:
```bash
npx jest src/__tests__/specific.test.ts
```

### Flipper Zero app (`/src/`)

```bash
fbt                  # Build .fap using uFBT
fbt test             # Run unit tests
```

## Architecture

### Web installer data flow

Profiles are JSON files in `/web/public/profiles/` (copied from `/sd_card/apps_data/flipdeck/profiles/` during `prebuild`). The flow:

```
/web/public/profiles/*.json
  → lib/profiles.ts: loadAllProfiles() → normalizeProfile()  (v1 actions → v2 commands migration)
  → lib/safety-check.ts: auditCommands()                     (regex-based command audit)
  → API routes under app/api/                                 (serve JSON or ZIP)
  → React components in app/page.tsx                         (ProfileSelector → CommandPreview → CommandAudit → SmartInstallButton)
```

State lives entirely in `page.tsx` as `useState`/`useMemo` hooks. There is no global state manager — `selectedId` is passed as props/callbacks.

`SmartInstallButton` always renders `SerialInstaller`, which drives a direct install over the **Web Serial API** (`lib/flipper-serial.ts`, `FlipperSerial` class — opens the Flipper's CLI serial port at 230400 baud and issues storage RPC commands to write `flipdeck.fap`, selected profiles via `getDeviceProfile()`, snippets, and `settings.json` directly to the SD card). `isWebSerialSupported()` gates whether this UI is shown; `SmartInstallButton` separately probes WebUSB (`navigator.usb.requestDevice`) just to show a "Flipper detected" hint, not for installs. Users without Web Serial support fall back to the ZIP download (`/api/pack`).

API routes:
- `GET /api/profiles` — JSON array of all profiles
- `GET /api/profiles/download?profile=<id>` — single profile download
- `GET /api/pack?profile=git&profile=node` — ZIP with selected profiles
- `GET /api/install-bundle/download` — full install ZIP (all profiles + snippets + settings.json + README-FIRST.txt)
- `GET /api/health` — liveness check, returns `{ ok, service, timestamp }`

### Profile schema (v2 canonical, v1 still accepted)

```json
{
  "name": "Git",
  "id": "git",
  "commands": [
    {
      "label": "Git Status",
      "type": "text",
      "value": "git status\n",
      "delay_ms": 100,
      "confirmation_required": true,
      "target": "usb_hid"
    }
  ]
}
```

v1 used `"actions"` (with `"confirm"` instead of `"confirmation_required"`). `normalizeProfile()` in `lib/profiles.ts` handles the migration transparently. The `extends` field enables profile inheritance (base profile commands are prepended). `getDeviceProfile()` returns the raw, normalized-but-unflattened profile written to the Flipper over serial — keep this in sync with what `profile_manager.c` expects to parse on-device.

Each command has an optional `target` field (`"usb_hid"` | `"wifi_uart"`, default `"usb_hid"`) that selects whether the command is sent as a USB HID keystroke to the host computer or as a UART line to the Flipper WiFi Dev Board.

Profile categories are derived from the profile `id` via `primaryCategory()` in `lib/profiles.ts`: `"wifi-devboard"` → "wifi"; `["aws", "docker"]` → "cloud"; `["system", "presentation"]` → "system"; everything else → "dev".

### Flipper Zero app state machine

The C app in `/src/` is a state machine with these states: `Idle`, `CategoryBrowser`, `ActionBrowser`, `SendConfirm`, `LongSnippetWarning`, `Settings`. In `ActionBrowser`, long-pressing OK sends immediately (skipping `SendConfirm`) and the Right button toggles the selected action as a favorite. In `CategoryBrowser`, long-pressing OK on a category pins/unpins it as the startup category (opened automatically on the next launch, bypassing `CategoryBrowser`), and a synthetic "Favorites" row appears at the top whenever any action is favorited, flattening favorited actions from every category into one list.

Key modules:
- `flipdeck_app.c` — main loop, USB polling (50ms), state transitions
- `flipdeck_ui.c` — 128×64 LCD rendering, button input handling, dispatches each action to USB HID or the UART bridge based on `action->target`
- `profile_manager.c` — JSON parsing from SD card, safety validation before send
- `usb_hid.c` — USB HID keyboard report generation
- `uart_bridge.c` — UART bridge to the Flipper WiFi Dev Board (GPIO pins 13/14, 115200 baud) for `target: "wifi_uart"` commands
- `settings.c` — JSON settings serialization

Memory constraint: 4096-byte stack limit. Use fixed-size buffers; avoid deep call stacks.

### Desktop helper CLI

Commander.js app with subcommands under `flipdeck profile <subcommand>`:
`validate`, `new`, `edit`, `preview`, `audit`, `migrate`, `share`, `sync`

Subcommands are registered via `registerProfileCommands()` in `src/lib/profile-tools.ts`; shared profile logic (loading, normalization, schema) lives in `src/lib/profile-tools.ts` and `src/lib/schema.ts`, while `src/validation.ts` holds `DANGEROUS_PATTERNS` and `validateProfile()`.

Validation pipeline: JSON parse → Zod schema (`lib/schema.ts`) → normalize v1→v2 → resolve `extends` → audit against `DANGEROUS_PATTERNS`.

## Safety Rules

`safety-rules.json` at the repo root is the single canonical source of truth for command
pattern severities. All three layers (web, desktop helper, Flipper C app) are expected to
match it exactly — web and desktop each have a "safety-rules-parity" test that fails if
their rule set drifts from the JSON. The C app's `profile_manager_is_value_safe` is checked
against the same rules by hand (no JSON parser on-device); keep it in sync manually when the
JSON changes.

Matching is case-insensitive and regex-based (not literal substring matching), so real-world
one-liners like `curl -fsSL https://x | bash` are caught regardless of spacing or case.

**Critical (blocks install/send):** `rm -rf`, real `curl|wget … | sh|bash` pipes, `dd if=`,
`mkfs`, fork bomb `:(){ :|:& };:`, raw disk redirect `> /dev/sd*`  
**Warning only (flagged, not blocked):** `sudo`, `chmod 777`, `chown root`, credential
assignments (`PASSWORD=`, `TOKEN=`, `API_KEY=`, `SECRET=`, `PRIVATE_KEY=`)

Severity determines enforcement: critical risks block the web install/download (both in the
UI and server-side in `/api/pack` and `/api/install-bundle/download`, which audit profiles
*and* snippets before zipping) and fail desktop `validate`/`audit`/`new`/`edit`. Warnings are
surfaced but never block.

Safety logic lives in:
- Canonical rules: `safety-rules.json` (repo root)
- Web: `web/src/lib/safety-check.ts` (enforced both client-side and in the `/api/pack` and
  `/api/install-bundle/download` routes)
- Desktop helper: `desktop_helper/src/validation.ts`
- Flipper app: `src/profile_manager.c` (`profile_manager_validate_action` /
  `profile_manager_is_value_safe`)

All commands require explicit OK confirmation on the Flipper before USB send.

## Key Conventions

- **Next.js version warning:** `web/AGENTS.md` notes this uses Next.js 16 with breaking API changes from training data — read `node_modules/next/dist/docs/` before writing Next.js code.
- **TypeScript paths:** `@/*` maps to `web/src/*` (configured in `web/tsconfig.json`).
- **Prettier config:** 100-char line width, no trailing commas (`desktop_helper/.prettierrc.json`).
- **C naming:** PascalCase for types with `_t` suffix, `snake_case` for functions and variables (Flipper Zero SDK convention).
- **Profile files live in two places:** `sd_card/apps_data/flipdeck/profiles/` is the source of truth for device profiles; `web/public/profiles/` is a copy populated at build time by `web/scripts/copy-profiles.mjs`.
- **Repo-level docs:** `docs/flight_manual.md` (end-user guide) and `docs/ROADMAP.md` (planned work) live at the repo root; `scripts/prebuild-packs.js` pre-generates downloadable packs.

## Deployment

Vercel deploys the `web/` directory. The root directory in the Vercel project is set to `web`; Vercel runs `npm run build` and deploys the output as serverless functions. No `vercel.json` is required — configuration is managed in the Vercel dashboard.
