# FlipDeck

**A USB Command Deck for Flipper Zero** — Turn your Flipper into a safe, configurable USB keyboard and touchpad for developers and power users.

[![License](https://img.shields.io/github/license/mohabbis/moswagger)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-blue)](https://flipperzero.one)
[![Project History](https://img.shields.io/badge/history-moswagger%20→%20FlipDeck-blue)](ARCHITECTURE.md)

## Why FlipDeck?

FlipDeck transforms your Flipper Zero into a programmable command deck. Store frequently-used commands, snippets, and shortcuts on your SD card, then send them to any connected computer over USB — no custom firmware or development boards required.

**Perfect for:**
- Git workflow automation
- Dev environment shortcuts
- Terminal snippet library
- Presentation remote control
- VSCode/Vim command palette

## Features

- ✅ **Profile Storage** — Save unlimited command profiles on SD card
- ✅ **USB HID Keyboard** — Send text, key presses, and shortcuts
- ✅ **Profile Types** — Commands, snippets, shortcuts, and scripts
- ✅ **No WiFi Required** — Works on stock Flipper Zero
- ✅ **Safe Mode** — Confirmation before sending commands
- ✅ **Settings** — Configurable delays and preferences

## Installation

1. **Download the latest release** from the [releases page](https://github.com/mohabbis/flipdeck/releases)
2. **Copy to SD card**: Extract and copy the `flipdeck` folder to:
   ```
   /apps_data/flipdeck/
   ```
   (or browse to Apps → Install in the Flipper menu)
3. **Launch**: From your Flipper, go to `Apps → FlipDeck`

## Getting Started

### Creating Profiles

Profiles are stored as JSON files in `/stor0800/flipdeck/profiles/` on the SD card. Default profiles include:

| Category | Location |
|----------|----------|
| Git | `profiles/git.json` |
| Node.js | `profiles/node.json` |
| Python | `profiles/python.json` |
| Docker | `profiles/docker.json` (NEW!) |
| System | `profiles/system.json` (NEW!) |
| Snippets | `profiles/snippets.json` (NEW!) |
| AWS | `profiles/aws.json` (NEW!) |
| VSCode | `profiles/vscode.json` |
| Presentation | `profiles/presentation.json` |

### Profile JSON Format

Profiles now use an **actions** array with explicit action types:

```json
{
  "name": "Node",
  "id": "node",
  "description": "Node.js development commands",
  "actions": [
    {
      "label": "Run dev server",
      "type": "text",
      "value": "npm run dev\n",
      "confirm": true
    },
    {
      "label": "Run tests",
      "type": "text",
      "value": "npm test\n",
      "confirm": true
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
  "actions": [
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
  "actions": [
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
/sd/card/apps_data/flipdeck/
├── profiles/
│   ├── git.json         # Git commands
│   ├── node.json        # Node.js commands
│   ├── python.json      # Python commands
│   ├── docker.json      # Docker commands (NEW!)
│   ├── system.json      # System utilities (NEW!)
│   ├── snippets.json    # Code templates (NEW!)
│   ├── aws.json         # AWS CLI commands (NEW!)
│   ├── vscode.json      # VSCode shortcuts
│   └── presentation.json # Presentation remote
├── snippets/            # Text snippet templates
│   ├── typescript.txt   # TS code templates
│   └── go.txt           # Go code templates
├── logs/                # Session logs
└── settings.json        # User preferences
```

### Action Format

Each action supports three types:

| Field | Type | Description |
|-------|------|-------------|
| `label` | string | Display name on Flipper |
| `type` | enum | `text`, `key`, or `key_combo` |
| `value` | string | The command/data to send |
| `confirm` | bool | Require confirmation before sending |

## Safety

FlipDeck is designed with safety as a priority:

- ⚠️ **No stealth payloads** — All commands are visible before sending
- ⚠️ **No automatic execution** — Requires explicit confirmation by default
- ⚠️ **No credential storage** — The Flipper never stores GitHub tokens or passwords
- ⚠️ **No destructive defaults** — Example commands are safe
- ⚠️ **Blocked dangerous commands** — System rejects `rm -rf`, `sudo`, `curl | sh`, and credential patterns

**Blocked Command Patterns:**
- `rm -rf`, `sudo`, `curl | sh`, `mkfs`, `dd if=`, fork bombs
- Any command containing: `PASSWORD`, `TOKEN`, `API_KEY`, `SECRET`, `PRIVATE_KEY`

**Always review your profiles in a text editor before storing them in FlipDeck.**

## Development

### Prerequisites

- Flipper Zero device
- SD card (8GB or larger)
- Computer with USB keyboard support

### Building

flipDeck uses the uFBT build system. To compile:

```bash
# Clone the repository
git clone https://github.com/mohabbis/moswagger.git
cd moswagger

# Build using fbt (Flipper Build Tool)
fbt
```

### Project Structure

```
moswagger/  (FlipDeck)
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
│       │   ├── docker.json   # NEW!
│       │   ├── system.json   # NEW!
│       │   ├── snippets.json # NEW!
│       │   ├── aws.json      # NEW!
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

The `desktop_helper/` directory contains a companion Node.js/TypeScript application for:
- GitHub authentication
- Profile sync from GitHub/Gist
- Profile import/export
- GitHub Actions triggers
- Issue creation from profiles

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

## Project History

This repository was originally **moswagger** - a Swagger/OpenAPI tooling project with Vercel deployment. It has been completely pivoted to **FlipDeck**, a Flipper Zero USB command deck application.

See [ARCHITECTURE.md](ARCHITECTURE.md) for details on the original moswagger-vercel.app and the migration to FlipDeck.

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) for details.

## Disclaimer

FlipDeck is an open-source project for educational and productivity purposes. The developers are not responsible for any misuse of this software. Always use caution when sending commands to connected computers.

---

Made with ❤️ for the Flipper Zero community.
**Safely hacking, one keypress at a time.**