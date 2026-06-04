# FlipDeck Web Installer

A Next.js web installer for FlipDeck. The public flow assumes users keep the microSD card inside the Flipper Zero, connect the Flipper over USB, and copy the included `apps_data` folder through qFlipper.

## User install flow

1. Leave the microSD card inserted in the Flipper Zero.
2. Plug in and unlock the Flipper over USB.
3. Open qFlipper and browse the Flipper SD card.
4. Open the deployed FlipDeck web app.
5. Click **Download Flipper install pack**.
6. Extract the ZIP and drag `apps_data` onto the Flipper SD card root in qFlipper.
7. Launch FlipDeck from the Flipper apps menu.

The install pack endpoint is:

```txt
/api/install-bundle/download
```

Manual fallback: download the ZIP and copy `apps_data` to the SD card yourself. Developer-only fallback: mount the microSD card in an external reader, copy `apps_data` to that mounted volume, eject it, and insert it back into the Flipper.

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
