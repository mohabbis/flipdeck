# FlipDeck Web Installer

A Next.js web installer for FlipDeck. It gives Flipper Zero users a simple qFlipper-style flow: plug in the Flipper, download one ZIP, and drag the included `apps_data` folder onto the SD card.

## User install flow

1. Plug in and unlock the Flipper Zero.
2. Open qFlipper and browse the SD card.
3. Open the deployed FlipDeck web app.
4. Click **Download Flipper install pack**.
5. Extract the ZIP and drag `apps_data` onto the SD card root.
6. Launch FlipDeck from the Flipper apps menu.

The install pack endpoint is:

```txt
/api/install-bundle/download
```

It includes:

```txt
README-FIRST.txt
apps_data/flipdeck/settings.json
apps_data/flipdeck/profiles/*.json
apps_data/flipdeck/snippets/*.txt
```

## Local development

```bash
npm install
npm run dev
```

Open [http://localhost:3000](http://localhost:3000).

## Build and run

```bash
npm run build
npm run start
```

`npm run start` runs the standalone Next.js server and binds to `0.0.0.0`, which is suitable for Railway.

## Railway deployment

Deploy from the repository root. The root `nixpacks.toml` runs:

```bash
cd web && npm ci
cd web && npm run build
cd web && npm run start
```

## Tests

```bash
npm test
npm run lint
npm run build
```
