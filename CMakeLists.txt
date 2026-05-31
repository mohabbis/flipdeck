# FlipDeck - Flipper Zero USB Command Deck
# A uFBT-based Flipper Zero application

FlipperApp("FlipDeck")

# Source files
set(SOURCES
    src/flipdeck_app.c
    src/flipdeck_ui.c
    src/profile_manager.c
    src/usb_hid.c
    src/settings.c
)

target_sources(flipdeck PRIVATE ${SOURCES})