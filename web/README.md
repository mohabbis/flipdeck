# FlipDeck Web Installer

A Next.js web installer for FlipDeck.

The installer supports two user paths:

1. **Direct install in Chrome or Edge** using WebSerial.
2. **ZIP + qFlipper install** in any browser.

Both paths create the same final layout on the Flipper SD card:

```txt
/ext/apps_data/flipdeck/
├── settings.json
├── profiles/
│   └── <selected-profile>.json
└── snippets/
```

## Path A: direct install with Chrome or Edge

Chromium browsers expose WebSerial, which lets the web installer talk to the Flipper CLI directly.

1. Leave the microSD card inserted in the Flipper Zero.
2. Plug in and unlock the Flipper over USB.
3. Open the deployed FlipDeck web app in Chrome or Edge.
4. Select the profiles to install.
5. Pass the browser safety check.
6. Click **Connect Flipper & Stage Pack**.
7. Select the Flipper in the browser's OS-level device picker.
8. Click **Stage N profiles to Flipper**.
9. Launch FlipDeck on the Flipper from **Apps → Tools → FlipDeck**.

Notes:

- The browser and operating system control the device picker. FlipDeck cannot choose the Flipper for the user.
- Direct install writes to `/ext/apps_data/flipdeck/` through the Flipper CLI.
- If the **Connect Flipper** button is missing, the browser likely does not support WebSerial. Use the ZIP path.

## Path B: ZIP + qFlipper, any browser

This path downloads an install ZIP that mirrors the SD card layout.

1. Open the deployed FlipDeck web app.
2. Select the profiles to install.
3. Pass the browser safety check.
4. Click **Download ZIP**.
5. Extract the ZIP.
6. Open qFlipper's SD card browser.
7. Drag the extracted `apps_data/` folder onto the SD card root.
8. Merge or replace if prompted.
9. Launch FlipDeck on the Flipper from **Apps → Tools → FlipDeck**.

The ZIP contains `apps_data/` at its root:

```txt
apps_data/
└── flipdeck/
    ├── settings.json
    ├── profiles/
    │   └── <selected-profile>.json
    └── snippets/
```

After copying to the Flipper SD root, that becomes:

```txt
/ext/apps_data/flipdeck/
```

Use `/ext/apps_data/...` for the Flipper's mounted SD path. Use `apps_data/...` for the ZIP contents before copying.

## Safety gate

Screening happens in the browser before anything is staged or zipped.

The browser safety check lives at:

```txt
web/src/lib/safety-check.ts
```

The shared rules live at:

```txt
../safety-rules.json
```

A profile containing a critical pattern cannot be installed through either direct install or ZIP download.

## Install pack endpoint

The ZIP install pack endpoint is:

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

`npm run start` runs the standalone Next.js server and binds to `0.0.0.0`.

## Vercel deployment

Deploy from the repository root. Set the root directory to `web` in your Vercel project settings.

```txt
Root directory: web
Build command: npm run build
Output directory: .next
```

## Tests

```bash
npm test
npm run lint
npm run build
```
