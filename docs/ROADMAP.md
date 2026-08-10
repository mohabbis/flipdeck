# FlipDeck Roadmap

## Phase 1: Foundation

- [x] Core Flipper Zero app structure (uFBT)
- [x] Basic UI with profile browser
- [x] SD card profile storage
- [x] USB HID communication layer
- [x] Default profile templates
- [x] Settings management
- [x] Flight manual documentation

## Phase 2: Profile Management

- [ ] Create new profiles from Flipper UI
- [ ] Edit existing profiles
- [ ] Delete profiles
- [ ] Profile import/export via SD card
- [ ] Profile categories/tags
- [ ] Icon selection per profile

## Phase 3: USB Enhancements

- [x] Key combination support (Ctrl, Alt, Shift, Win)
- [x] Delay configuration per command (`delay_ms` on each command; profile-level default still open — see #42)
- [ ] Key press/release timing control
- [x] USB connection auto-detection (settings toggle gates the status poll)
- [ ] Multiple keyboard layouts

## Phase 4: Advanced Features

- [x] Favorites + quick-send (long-press OK skips confirm; Right toggles favorite)
- [x] NFC tag triggers (scan a tag to jump straight to a mapped action's confirm screen)
- [x] Sub-GHz RF remote triggers (RX-only 433MHz bind/fire; always confirms)
- [ ] Presentation remote mode (left/right/up/down keys)
- [ ] Mouse movement and clicks
- [ ] Desktop companion app for profile sync
- [ ] GitHub Gist import for shared profiles
- [ ] Quick snippet buffer (multiple profiles at once)

## Phase 5: Optional Extensions

- [x] WiFi dev board support for network commands
- [ ] Momentum firmware integration (developed/tested on Momentum so far; not yet
      verified against official Flipper firmware)
- [ ] Bluetooth HID support
- [ ] QR code profile import
- [ ] Voice command triggering

## Long-term Vision

FlipDeck should become the ultimate USB command center for Flipper Zero: a safe, extensible tool that bridges the gap between physical button presses and digital workflows.

---

## Safety Requirements

All future features must adhere to the safety principles in `docs/flight_manual.md`:
- No stealth payloads
- No automatic execution
- No credential storage
- Safe defaults
