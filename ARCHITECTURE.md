# ARCHITECTURE

## Overview

FlipDeck is a Flipper Zero native application that turns the device into a USB command deck — a safety-first tool for sending confirmed keystroke commands to a connected computer over USB HID.

## Tech Stack

```
FlipDeck/
├── CMakeLists.txt       # uFBT build configuration for Flipper Zero
├── src/               # C source code for Flipper app
│   ├── flipdeck_app.c/h # Main application entry point
│   ├── flipdeck_ui.c/h  # User interface views
│   ├── profile_manager.c/h # SD card profile system
│   ├── usb_hid.c/h      # USB HID keyboard implementation
│   └── settings.c/h     # User preferences
├── assets/
│   └── icon.bmp       # Flipper Zero app icon
├── desktop_helper/      # Optional TypeScript companion app
│   ├── package.json
│   ├── tsconfig.json
│   └── src/index.ts
└── sd_card/           # SD card content for Flipper Zero
    └── apps_data/flipdeck/
        ├── profiles/    # JSON command profiles
        ├── snippets/    # Text snippet templates
        ├── logs/        # Session logs
        └── settings.json
```

## Architecture

- **Platform**: Flipper Zero (ARM Cortex-M4, no WiFi required on stock firmware)
- **Build System**: uFBT (Flipper Build Tool) for compiling to `.fap` packages
- **Storage**: SD card for profiles and settings (no credential storage)
- **USB Protocol**: USB HID keyboard for keystroke injection to connected computers
- **Desktop Helper** (optional): Node.js/TypeScript app for GitHub sync and advanced features

## Design Principles

1. **Flipper Community Focus**: Leverage the growing Flipper Zero maker/hacker community
2. **Hardware Integration**: Real-world USB HID functionality rather than web APIs
3. **Productivity Tool**: Practical daily-use tool for developers
4. **Safety-First Design**: Explicit confirmation before command execution (unlike stealth BadUSB tools)
5. **No Internet Required**: Works completely offline on stock Flipper firmware

## License

MIT License - See [LICENSE](LICENSE) for details.
