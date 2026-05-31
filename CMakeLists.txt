# FlipDeck - Flipper Zero USB Command Deck
# A uFBT-based Flipper Zero application

FlipperApp(
    ID "flipdeck"
    NAME "FlipDeck"
    TITLE "FlipDeck"
    DESCRIPTION "USB Command Deck for developers"
    AUTHOR "Muhammad Rafiq"
    VERSION "1.0.0"
    SDK "none"
)

# Include Flipper Zero SDK
Include(FZ_MAIN)

# Source files
Sources(
    src/flipdeck_app.c
    src/flipdeck_ui.c
    src/profile_manager.c
    src/usb_hid.c
    src/settings.c
)

# Assets
# Bitmap icons and GUI resources

# Optional: Custom fonts or additional resources