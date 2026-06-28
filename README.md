# FlipDeck

**A USB Command Deck for Flipper Zero** — Turn your Flipper into a safe, configurable USB keyboard for developers and power users.

[![License](https://img.shields.io/github/license/mohabbis/flipdeck)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-blue)](https://flipperzero.one)

## Why FlipDeck?

FlipDeck transforms your Flipper Zero into a programmable command deck. Store frequently-used commands, snippets, and shortcuts on your SD card, then send them to any connected computer over USB — no custom firmware or development boards required.

**Perfect for:**
- Git workflow automation
- Dev environment shortcuts
- Terminal snippet library
- Presentation remote control
- VSCode/Vim command palette

## Features

- ✅ **Profile Storage** — Save command profiles on the SD card
- ✅ **USB HID Keyboard** — Send text, key presses, and shortcuts
- ✅ **Profile Types** — Commands, snippets, shortcuts, and scripts
- ✅ **No WiFi Required** — Works on stock Flipper Zero
- ✅ **Safe Mode** — Confirmation before sending commands
- ✅ **Settings** — Configurable delays and preferences

## Installation for Flipper Zero users

FlipDeck installs selected profiles to the Flipper Zero SD card in two ways. Both paths create the same on-device files; they only differ in how the files get onto the SD card.

### Path A: direct install with Chrome or Edge

Chrome and Edge support WebSerial, so the FlipDeck web installer can talk to the Flipper CLI directly.

1. Plug in your Flipper Zero with the SD card inserted.
2. Open the FlipDeck web app in Chrome or Edge.
3. Select the profiles you want to install.
4. Review the browser safety check. Critical matches are blocked before anything is written.
5. Click **Connect Flipper & Stage Pack**.
6. Pick your Flipper in the browser's OS-level device picker.
7. Click **Stage N profiles to Flipper**.
8. On the Flipper: **Apps → Tools → FlipDeck**, pick a profile, then press **OK**.

Notes:

- FlipDeck cannot select the Flipper automatically. The browser and OS control the device picker, because apparently letting websites silently grab USB devices would be frowned upon by civilization.
- Direct install writes to the mounted SD path: `/ext/apps_data/flipdeck/`.
- The installer creates `/ext/apps_data/flipdeck/profiles/` and `/ext/apps_data/flipdeck/snippets/` if needed.
- If the **Connect Flipper** button is missing, your browser probably does not support WebSerial. Use Path B.

### Path B: ZIP + qFlipper, any browser

This works in any browser by downloading an install ZIP that mirrors the SD card layout.

1. Select the profiles you want to install.
2. Review the browser safety check. Critical matches are blocked before ZIP creation.
3. Click **Download ZIP**.
4. Extract the ZIP.
5. Open qFlipper's SD card browser.
6. Drag the extracted `apps_data/` folder onto the SD card root.
7. Merge or replace if prompted.
8. On the Flipper: **Apps → Tools → FlipDeck**, pick a profile, then press **OK**.

The ZIP contains `apps_data/` at its root. Dragging that folder onto the SD card root produces `/ext/apps_data/flipdeck/` on the Flipper.

### What gets written

Final on-device layout:

```text
/ext/apps_data/flipdeck/
├── settings.json
├── profiles/
│   └── <selected-profile>.json
└── snippets/
```

ZIP layout before copying:

```text
apps_data/
└── flipdeck/
    ├── settings.json
    ├── profiles/
    │   └── <selected-profile>.json
    └── snippets/
```

Use `/ext/apps_data/...` when referring to the Flipper's mounted SD path. Use `apps_data/...` when referring to the folder inside the ZIP. This distinction exists because path naming was apparently not dramatic enough already.

### Safety gate

Before anything is staged to the Flipper or packaged into a ZIP, FlipDeck screens the selected profiles in the browser.

The browser safety check lives in:

```text
web/src/lib/safety-check.ts
```

It uses the shared rule set in:

```text
safety-rules.json
```

If a profile matches a critical safety pattern, installation is blocked for both direct install and ZIP download.

See [`docs/security-model.md`](docs/security-model.md) for the broader enforcement model.

### Deploying the web installer on Vercel

The installer is a Next.js app in `web/`.

Set these Vercel options:

```text
Root directory: web
Build command: npm run build
Output directory: .next
```

## Getting Started

### Creating Profiles

Profiles are stored as JSON files in `/ext/apps_data/flipdeck/profiles/` on the Flipper SD card. Default profiles include:

| Category | Location |
|----------|----------|
| Git | `profiles/git.json` |
| Node.js | `profiles/node.json` |
| Python | `profiles/python.json` |
| Docker | `profiles/docker.json` |
| System | `profiles/system.json` |
| Snippets | `profiles/snippets.json` |
| AWS | `profiles/aws.json` |
| VSCode | `profiles/vscode.json` |
| Presentation | `profiles/presentation.json` |

### Profile JSON Format

Profiles use a **commands** array with explicit action types. This is the canonical v2 format. Older profiles using `actions`/`confirm` are still read and migrated automatically.

```json
{
  "name": "Node",
  "id": "node",
  "description": "Node.js development commands",
  "commands": [
    {
      "label": "Run dev server",
      "type": "text",
      "value": "npm run dev\n",
      "confirmation_required": true
    },
    {
      "label": "Run tests",
      "type": "text",
      "value": "npm test\n",
      "confirmation_required": true
    }
  ]
}
```

#### Action Types

| Type | Description | Example |
|------|-------------|---------|
| `text` | Sends text string | `"npm run dev\n"` |
| `key` | Presses a single key | `"RIGHT"`, `"ENTER"`, `"ESCAPE"` |
| `key_combo` | Modifier + key combination | `"CTRL+C"`, `"SHIFT+F5"` |

#### Example Profiles

**Git Profile:**

```json
{
  "name": "Git",
  "id": "git",
  "commands": [
    {"label": "Git Status", "type": "text", "value": "git status\n"},
    {"label": "Git Push", "type": "text", "value": "git push origin\n"}
  ]
}
```

**VSCode Shortcuts:**

```json
{
  "name": "VSCode",
  "id": "vscode",
  "commands": [
    {"label": "Command Palette", "type": "key_combo", "value": "CTRL+SHIFT+P"},
    {"label": "Terminal", "type": "key_combo", "value": "CTRL+`"}
  ]
}
```

### Using FlipDeck

1. **Browse** categories with UP/DOWN buttons.
2. **Select** a category, such as Git, Node, or Python.
3. **Browse** actions within the category.
4. **Press OK** to send, or confirm first if required.
5. **Press MENU** for Settings.

### SD Card Layout

On the Flipper, the SD card is mounted under `/ext`, so the app reads from `/ext/apps_data/flipdeck/`.

```text
/ext/apps_data/flipdeck/
├── profiles/
│   ├── git.json          # Git commands
│   ├── node.json         # Node.js commands
│   ├── python.json       # Python commands
│   ├── docker.json       # Docker commands
│   ├── system.json       # System utilities
│   ├── snippets.json     # Code templates
│   ├── aws.json          # AWS CLI commands
│   ├── vscode.json       # VSCode shortcuts
│   └── presentation.json # Presentation remote
├── snippets/             # Text snippet templates
│   ├── typescript.txt    # TS code templates
│   └── go.txt            # Go code templates
├── logs/                 # Session logs
└── settings.json         # User preferences
```

Repository seed files live under `sd_card/apps_data/flipdeck/`, without the `/ext` prefix, because `/ext` only exists on the Flipper.

### Command Format

Each command supports three types:

| Field | Type | Description |
|-------|------|-------------|
| `label` | string | Display name on Flipper |
| `type` | enum | `text`, `key`, or `key_combo` |
| `value` | string | The command/data to send |
| `confirmation_required` | bool | Require confirmation before sending |
| `target` | enum | `usb_hid` by default, or `wifi_uart` |

## Safety

FlipDeck is designed with safety as a priority:

- ⚠️ **No stealth payloads** — All commands are visible before sending.
- ⚠️ **No automatic execution** — Requires explicit confirmation by default.
- ⚠️ **No credential storage** — The Flipper never stores GitHub tokens or passwords.
- ⚠️ **No destructive defaults** — Example commands are safe.
- ⚠️ **Blocked dangerous commands** — Destructive patterns are rejected outright; risky-but-common ones are flagged instead of blocked.

The full rule set lives in [`safety-rules.json`](safety-rules.json) at the repo root and is shared across the web installer, desktop helper, and Flipper app.

**Blocked (critical):** `rm -rf`, real `curl`/`wget … | sh`/`bash` pipes, `mkfs`, `dd if=`, fork bombs, raw disk redirects such as `> /dev/sd*`.

**Flagged but allowed (warning):** `sudo`, `chmod 777`, `chown root`, and credential-looking assignments such as `PASSWORD=`, `TOKEN=`, `API_KEY=`, `SECRET=`, and `PRIVATE_KEY=`.

**Always review your profiles in a text editor before storing them in FlipDeck.**

## Development

### Prerequisites

- Flipper Zero device
- SD card, 8GB or larger
- Computer with USB keyboard support

### Building

FlipDeck uses the uFBT build system. To compile:

```bash
git clone https://github.com/mohabbis/flipdeck.git
cd flipdeck
fbt
```

### Project Structure

```text
flipdeck/
├── CMakeLists.txt           # uFBT build configuration
├── assets/                  # Icons and resources
├── desktop_helper/          # Companion desktop app
│   ├── package.json
│   ├── tsconfig.json
│   └── src/
│       └── index.ts         # CLI tool for profile management
├── sd_card/                 # Seed SD card content
│   └── apps_data/flipdeck/
│       ├── profiles/        # JSON command profiles
│       ├── snippets/        # Text snippet templates
│       ├── logs/            # Session logs
│       └── settings.json    # User preferences
├── src/                     # Flipper app source
│   ├── flipdeck_app.c       # Main application logic
│   ├── flipdeck_app.h
│   ├── flipdeck_ui.c        # User interface
│   ├── flipdeck_ui.h
│   ├── profile_manager.c    # SD card profile system
│   ├── profile_manager.h
│   ├── usb_hid.c            # USB HID communication
│   ├── usb_hid.h
│   └── settings.c           # Settings management
├── web/                     # Next.js web installer
├── docs/
│   ├── flight_manual.md     # Safety and usage guide
│   └── ROADMAP.md           # Development roadmap
└── README.md
```

## Desktop Helper

The `desktop_helper/` directory contains a companion Node.js/TypeScript CLI for:

- Creating, validating, and auditing profiles
- Previewing how a profile renders on the Flipper
- Migrating legacy `actions` profiles to the v2 `commands` format
- Importing/exporting profiles and syncing them via GitHub Gist

```bash
cd desktop_helper
npm install
npm start
```

## Roadmap

See [docs/ROADMAP.md](docs/ROADMAP.md) for planned features.

### Planned Features

- [ ] Custom profile creation from Flipper UI
- [ ] Profile import/export via SD card
- [ ] Key combination support
- [ ] Presentation remote mode
- [ ] Desktop companion app for profile sync
- [ ] Momentum firmware integration, optional

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Disclaimer

FlipDeck is an open-source project for educational and productivity purposes. The developers are not responsible for any misuse of this software. Always use caution when sending commands to connected computers.

---

Made with ❤️ for the Flipper Zero community.
**Safely hacking, one keypress at a time.**
