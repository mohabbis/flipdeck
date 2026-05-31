# ARCHITECTURE

## Project History

This repository was originally **moswagger** - a Swagger/OpenAPI tooling project with Vercel deployment. It has since been completely pivoted to **FlipDeck**, a Flipper Zero USB command deck application.

### The Original moswagger-vercel.app

The original project provided:

- **Vercel Serverless API**: A Go-based backend deployed to Vercel for API endpoints
- **Swagger/OpenAPI Tooling**: Utilities for working with OpenAPI specifications
- **Flipper Zero Styling**: Custom CSS styling for web dashboards with Flipper Zero aesthetics
- **MuHome Dashboard**: A HomeKit dashboard with Flipper-styled UI components

#### Original Tech Stack
```
moswagger/
├── main.go              # Go HTTP server
├── go.mod               # Go module definition
├── api/index.go         # Vercel serverless function
├── vercel.json          # Vercel deployment configuration
└── examples/
    └── openapi.yaml     # Example OpenAPI specification
```

#### Original Architecture
- **Frontend**: Static HTML/CSS with Flipper Zero-inspired styling
- **Backend**: Go HTTP server running as Vercel serverless function
- **Deployment**: Vercel platform with automatic builds from GitHub
- **Routing**: `/api/*` endpoints handled by serverless functions

### The New: FlipDeck

The project has been entirely rearchitected as a **Flipper Zero native application**:

#### Current Tech Stack
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

#### FlipDeck Architecture
- **Platform**: Flipper Zero (ARM Cortex-M4, no WiFi required on stock firmware)
- **Build System**: uFBT (Flipper Build Tool) for compiling to `.fap` packages
- **Storage**: SD card for profiles and settings (no credential storage)
- **USB Protocol**: USB HID keyboard for keystroke injection to connected computers
- **Desktop Helper** (optional): Node.js/TypeScript app for GitHub sync and advanced features

## Why the Pivot?

The pivot from moswagger to FlipDeck was made because:

1. **Flipper Community Focus**: Leverage the growing Flipper Zero maker/hacker community
2. **Hardware Integration**: Real-world USB HID functionality rather than web APIs
3. **Productivity Tool**: Practical daily-use tool for developers
4. **Safety-First Design**: Explicit confirmation before command execution (unlike stealth BadUSB tools)
5. **No Internet Required**: Works completely offline on stock Flipper firmware

## Migration Notes

All OpenAPI and Vercel-related code has been removed:
- `main.go` - removed (was Go HTTP server)
- `go.mod` - removed (was Go module)
- `vercel.json` - removed (was Vercel config)
- `examples/openapi.yaml` - removed (was OpenAPI example)
- API endpoints - removed (were `/api/*` routes)

All moswagger references have been cleaned up and replaced with FlipDeck branding and functionality.

## License

MIT License - See [LICENSE](LICENSE) for details.