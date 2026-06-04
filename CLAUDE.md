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

State lives entirely in `page.tsx` as `useState`/`useMemo` hooks. There is no global state manager — `selectedId` is passed as props/callbacks. The `SmartInstallButton` detects WebUSB availability to switch between direct install and ZIP download.

API routes:
- `GET /api/profiles` — JSON array of all profiles
- `GET /api/profiles/download?profile=<id>` — single profile download
- `GET /api/pack?profile=git&profile=node` — ZIP with selected profiles
- `GET /api/install-bundle/download` — full install ZIP (all profiles + snippets + settings.json + README-FIRST.txt)

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
      "confirmation_required": true
    }
  ]
}
```

v1 used `"actions"` (with `"confirm"` instead of `"confirmation_required"`). `normalizeProfile()` in `lib/profiles.ts` handles the migration transparently. The `extends` field enables profile inheritance (base profile commands are prepended).

Profile categories are derived from the profile `id`: `["aws", "docker"]` → "cloud"; `["system", "presentation"]` → "system"; everything else → "dev".

### Flipper Zero app state machine

The C app in `/src/` is a state machine with these states: `Idle`, `CategoryBrowser`, `ActionBrowser`, `ActionDetail`, `SendConfirm`, `LongSnippetWarning`, `Settings`.

Key modules:
- `flipdeck_app.c` — main loop, USB polling (50ms), state transitions
- `flipdeck_ui.c` — 128×64 LCD rendering, button input handling
- `profile_manager.c` — JSON parsing from SD card, safety validation before USB send
- `usb_hid.c` — USB HID keyboard report generation
- `settings.c` — JSON settings serialization

Memory constraint: 4096-byte stack limit. Use fixed-size buffers; avoid deep call stacks.

### Desktop helper CLI

Commander.js app with subcommands under `flipdeck profile <subcommand>`:
`validate`, `new`, `edit`, `preview`, `audit`, `migrate`, `share`, `sync`

Validation pipeline: JSON parse → Zod schema (`lib/schema.ts`) → normalize v1→v2 → resolve `extends` → audit against `DANGEROUS_PATTERNS`.

## Safety Rules

Both the web and C layers enforce the same blocked patterns. Any command matching these must be rejected:

**Critical (blocked):** `rm -rf`, `curl|sh`, `wget|sh`, `dd if=`, `mkfs`, `:(){ :|:& };:`  
**Credential patterns (blocked):** `PASSWORD`, `TOKEN`, `API_KEY`, `SECRET`, `PRIVATE_KEY`  
**Warning only:** `sudo`, `chmod 777`

Safety logic lives in:
- Web: `web/src/lib/safety-check.ts`
- Desktop helper: `desktop_helper/src/validation.ts`
- Flipper app: `src/profile_manager.c` (`profile_manager_validate_action`)

All commands require explicit OK confirmation on the Flipper before USB send.

## Key Conventions

- **Next.js version warning:** `web/AGENTS.md` notes this uses Next.js 16 with breaking API changes from training data — read `node_modules/next/dist/docs/` before writing Next.js code.
- **TypeScript paths:** `@/*` maps to `web/src/*` (configured in `web/tsconfig.json`).
- **Prettier config:** 100-char line width, no trailing commas (`desktop_helper/.prettierrc.json`).
- **C naming:** PascalCase for types with `_t` suffix, `snake_case` for functions and variables (Flipper Zero SDK convention).
- **Profile files live in two places:** `sd_card/apps_data/flipdeck/profiles/` is the source of truth for device profiles; `web/public/profiles/` is a copy populated at build time by `web/scripts/copy-profiles.mjs`.

## Deployment

Railway reads `nixpacks.toml` at the repo root: installs Node.js, runs `cd web && npm ci && npm run build`, then starts with `cd web && npm run start`. The `start` script binds to `0.0.0.0` for Railway's port routing.
