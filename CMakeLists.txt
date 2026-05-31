# FlipDeck - Flipper Zero USB Command Deck
# A uFBT-based Flipper Zero application

FlixFile(APP_ICON assets/icon.bmp)

App(
    NAME "FlipDeck",
    TITLE "FlipDeck",
    EDITION "Standard",
    AUTHOR "Muhammad Rafiq",
    GROUP "Games",
    PERMISSIONS "None",
    SUBCATEGORY "Tools",
    SORT_ORDER 10
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