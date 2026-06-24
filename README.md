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

### Fastest: direct install (Chrome/Edge)

1. Plug in your Flipper Zero (SD card stays inserted).
2. Open the FlipDeck web app, check the profiles you want.
3. Click **Connect Flipper & Stage Pack**, pick your Flipper in the browser prompt.
4. Click **Stage N profiles to Flipper**.
5. On the Flipper: Apps → Tools → FlipDeck, pick a profile, press OK to send.

No button? You're not on a Chromium browser — use the ZIP method below.

### Any browser: ZIP + qFlipper

1. Check the profiles you want, click **Download ZIP**.
2. Open qFlipper's SD card view, drag the ZIP's `apps_data` folder onto the SD card root
   (merge/replace if prompted).
3. On the Flipper: Apps → Tools → FlipDeck, pick a profile, press OK to send.

Manual layout, if copying files by hand:
```
/apps_data/flipdeck/
├── settings.json
├── profiles/
└── snippets/
```

### Deploying the web installer on Vercel

Deploy from the repository root on Vercel. Set the root directory to `web`, build command to `npm run build`, and output directory to `.next`.

## Getting Started

### Creating Profiles

Profiles are stored as JSON files in `/apps_data/flipdeck/profiles/` on the SD card. Default profiles include:

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

Profiles use a **commands** array with explicit action types (this is the canonical v2 format —
older profiles using `actions`/`confirm` are still read and migrated automatically):

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

1. **Browse** categories with UP/DOWN buttons
2. **Select** a category (e.g., Git, Node, Python)
3. **Browse** actions within the category
4. **Press OK** to send (or confirm if required)
5. **Press MENU** for Settings

### SD Card Layout

```
/apps_data/flipdeck/
├── profiles/
│   ├── git.json         # Git commands
│   ├── node.json        # Node.js commands
│   ├── python.json      # Python commands
│   ├── docker.json      # Docker commands
│   ├── system.json      # System utilities
│   ├── snippets.json    # Code templates
│   ├── aws.json         # AWS CLI commands
│   ├── vscode.json      # VSCode shortcuts
│   └── presentation.json # Presentation remote
├── snippets/            # Text snippet templates
│   ├── typescript.txt   # TS code templates
│   └── go.txt           # Go code templates
├── logs/                # Session logs
└── settings.json        # User preferences
```

### Command Format

Each command supports three types:

| Field | Type | Description |
|-------|------|-------------|
| `label` | string | Display name on Flipper |
| `type` | enum | `text`, `key`, or `key_combo` |
| `value` | string | The command/data to send |
| `confirmation_required` | bool | Require confirmation before sending |
| `target` | enum | `usb_hid` (default) or `wifi_uart` — where the command is sent |

## Safety

FlipDeck is designed with safety as a priority:

- ⚠️ **No stealth payloads** — All commands are visible before sending
- ⚠️ **No automatic execution** — Requires explicit confirmation by default
- ⚠️ **No credential storage** — The Flipper never stores GitHub tokens or passwords
- ⚠️ **No destructive defaults** — Example commands are safe
- ⚠️ **Blocked dangerous commands** — Destructive patterns are rejected outright; risky-but-common
  ones are flagged instead of blocked

The full rule set lives in [`safety-rules.json`](safety-rules.json) at the repo root and is shared
(and tested for parity) across the web installer, desktop helper, and Flipper app:

**Blocked (critical):** `rm -rf`, real `curl`/`wget … | sh`/`bash` pipes, `mkfs`, `dd if=`,
fork bombs, raw disk redirects (`> /dev/sd*`)
**Flagged but allowed (warning):** `sudo`, `chmod 777`, `chown root`, and credential-looking
assignments (`PASSWORD=`, `TOKEN=`, `API_KEY=`, `SECRET=`, `PRIVATE_KEY=`)

**Always review your profiles in a text editor before storing them in FlipDeck.**

## Development

### Prerequisites

- Flipper Zero device
- SD card (8GB or larger)
- Computer with USB keyboard support

### Building

FlipDeck uses the uFBT build system. To compile:

```bash
# Clone the repository
git clone https://github.com/mohabbis/flipdeck.git
cd flipdeck

# Build using fbt (Flipper Build Tool)
fbt
```

### Project Structure

```
flipdeck/
├── CMakeLists.txt           # uFBT build configuration
├── assets/                  # Icons and resources
├── desktop_helper/          # Companion desktop app
│   ├── package.json
│   ├── tsconfig.json
│   └── src/
│       └── index.ts         # CLI tool for profile management
├── sd_card/                 # SD card content (mounted on device)
│   └── apps_data/flipdeck/
│       ├── profiles/        # JSON command profiles
│       │   ├── git.json
│       │   ├── node.json
│       │   ├── python.json
│       │   ├── docker.json
│       │   ├── system.json
│       │   ├── snippets.json
│       │   ├── aws.json
│       │   ├── vscode.json
│       │   └── presentation.json
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
- [ ] Key combination support (Ctrl+C, Alt+Tab, etc.)
- [ ] Presentation remote mode
- [ ] Desktop companion app for profile sync
- [ ] Momentum firmware integration (optional)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) for details.

## Disclaimer

FlipDeck is an open-source project for educational and productivity purposes. The developers are not responsible for any misuse of this software. Always use caution when sending commands to connected computers.

---

Made with ❤️ for the Flipper Zero community.
**Safely hacking, one keypress at a time.**