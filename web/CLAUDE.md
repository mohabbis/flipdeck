# CLAUDE.md

@AGENTS.md

This file provides guidance to Claude Code when working in `flipdeck/web`.

## Project

This is the Next.js web installer for FlipDeck. It should feel like a firmware installer: connect a Flipper Zero, download one ZIP, and drag the included `apps_data` folder onto the SD card.

## Commands

Run commands from `flipdeck/web`.

```sh
npm install
npm run dev
npm run build
npm run start
npm run lint
npm test
```

`npm run build` runs `scripts/copy-profiles.mjs` first. Keep that copy step working when moving profile, snippet, or bundle files.

## Key Behavior

- Main install-pack endpoint: `/api/install-bundle/download`
- The generated ZIP must include `README-FIRST.txt`, `apps_data/flipdeck/settings.json`, `apps_data/flipdeck/profiles/*.json`, and `apps_data/flipdeck/snippets/*.txt`.
- `npm run start` serves the standalone Next.js build on `0.0.0.0` (used for local/production Node hosting).

## Guidelines

- Read `node_modules/next/dist/docs/` before relying on Next.js API details in this project, per `AGENTS.md`.
- Preserve the public install flow described in `README.md`.
- Use small-screen-friendly copy and controls because users may follow instructions while handling qFlipper.
- Do not add credential requirements for the installer.
