# Contributing to FlipDeck

Thank you for your interest in FlipDeck! This guide will help you get started with development.

## Development Setup

### Prerequisites

- **Flipper Zero** device (stock firmware)
- **SD card** (8GB FAT32 formatted)
- **Computer** with USB-A to USB-C cable (data-capable)

### Build Environment

We recommend using the Flipper Zero simulator for initial development:

```bash
# Clone the repository
git clone https://github.com/mohabbis/moswagger.git
cd moswagger

# Build using fbt
curl -fsSL https://raw.githubusercontent.com/flipperzero/fbt/main/install.sh | sh
fbt
```

### Setting Up fbt

1. Install Python 3.8+
2. Install fbt: `pip install fbt`
3. Install fbt for Flipper: `fbt install-sdk`

## Project Structure

```
src/
├── flipdeck_app.c      # Main entry point and state machine
├── flipdeck_ui.c       # GUI views and user interaction
├── profile_manager.c   # SD card profile storage
├── usb_hid.c           # USB HID communication
└── settings.c          # User preferences
```

## Coding Standards

### C Style

- Follow the [Flipper Zero C style guide](https://docs.flipperzero.one-developer-docs/style)
- Use 4-space indentation
- Limit lines to 100 characters
- Use meaningful variable names

### Naming Conventions

```c
// Types: PascalCase, suffixed with _t
typedef struct { ... } FlipDeckProfile;

// Functions: snake_case
void profile_manager_load_profiles(void);

// Variables: snake_case
static FlipDeckApp* g_app_ctx;

// Constants: SCREAMING_SNAKE_CASE
#define FLIPDECK_MAX_PROFILES 50
```

### Safety Requirements

⚠️ **All code must follow safety principles:**

- ✅ No credential storage
- ✅ No stealth payloads
- ✅ Explicit user confirmation before USB send
- ✅ Graceful error handling
- ✅ Resource cleanup on free

## Testing

### Unit Tests

Run tests on the simulator:

```bash
fbt test
```

### Manual Testing

1. Build: `fbt`
2. Copy `build-flipdeck/flipdeck.fap` to SD card
3. Install via Apps → Install
4. Test each workflow thoroughly

### Automated Checks

- All code compiles without warnings
- Memory leak check (use simulator with valgrind)
- USB safety review for new features

## Pull Request Process

Every PR should include:

1. **Clear Summary** - What and why, not how
2. **Testing Notes** - How to test manually
3. **Safety Review** - Any USB/HID changes need explicit review
4. **Updated Docs** - README.md, flight_manual.md, or ROADMAP.md

### PR Labels

- `feature` - New functionality
- `bug` - Fixed issue
- `docs` - Documentation changes
- `ui` - Display/GUI changes
- `safety` - Security or safety review needed
- `enhancement` - Improving existing features

## Architecture Guidelines

### State Machine

The app uses a simple state machine. Add new states in `FlipDeckState`:

```c
typedef enum {
    FlipDeckState_Idle,
    FlipDeckState_Browser,
    FlipDeckState_ProfileView,
    FlipDeckState_SendConfirm,
    FlipDeckState_Settings,
    // Add new states here
} FlipDeckState;
```

### Profile Types

Add new profile types in `FlipDeckProfileType`:

```c
typedef enum {
    FlipDeckProfileType_Command,
    FlipDeckProfileType_Snippet,
    FlipDeckProfileType_Shortcut,
    FlipDeckProfileType_Script,
} FlipDeckProfileType;
```

### Memory Management

- Always check malloc success
- Free all resources in `flipdeck_app_free()`
- Avoid dynamic allocation in loops
- Use stack for small, fixed-size data

## Security Review Checklist

Before submitting PRs with sensitive changes:

- [ ] No passwords or credentials in code
- [ ] USB HID changes reviewed for safety
- [ ] All commands require explicit confirmation
- [ ] Error messages don't leak sensitive info
- [ ] SD card operations handle corruption gracefully

## Questions or Issues?

- Open an issue with `bug` label for problems
- Use `feature` label for enhancement ideas
- Check existing issues before creating new ones

Thank you for contributing to FlipDeck! 🎮