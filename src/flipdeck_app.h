/**
 * @file flipdeck_app.h
 * @brief FlipDeck - Flipper Zero USB Command Deck
 */

#ifndef FLIPDECK_APP_H
#define FLIPDECK_APP_H

#include <furi.h>
#include <furi_hal.h>

#ifdef UFBT_HOST_BUILD
/* Host test build - skip GUI headers */
typedef void Gui;
#else
#include <gui/gui.h>
#endif

/** Current app state */
typedef enum {
    FlipDeckState_Idle,
    FlipDeckState_CategoryBrowser,
    FlipDeckState_ActionBrowser,
    FlipDeckState_NfcScan,
    FlipDeckState_SendConfirm,
    FlipDeckState_LongSnippetWarning,
    FlipDeckState_Settings,
} FlipDeckState;

/** Maximum limits */
#define FLIPDECK_MAX_COMMAND_LENGTH 256
#define FLIPDECK_MAX_PROFILE_NAME 64
#define FLIPDECK_MAX_SNIPPET_LENGTH_WARN 100  /* Warn for snippets longer than this */
#define FLIPDECK_MAX_FAVORITES 6

/** A pinned action, referenced by its source category + label so the actual
 *  FlipDeckAction (value/type/target/etc.) is always re-read fresh from the
 *  category file rather than duplicated and allowed to go stale. */
typedef struct {
    char category_id[32];
    char label[64];
} FlipDeckFavorite;

/** Settings structure */
typedef struct {
    uint32_t send_delay_ms;
    bool confirm_before_send;
    bool auto_detect_usb;
    bool show_icons;
    bool show_descriptions;
    char startup_category[32];
    uint32_t long_snippet_warn_state;  // Temporary state for long snippet warning
    FlipDeckFavorite favorites[FLIPDECK_MAX_FAVORITES];
    uint32_t favorite_count;
} FlipDeckSettings;

/** App context containing all state */
typedef struct FlipDeckApp {
    FlipDeckState state;
    uint32_t category_count;
    uint32_t current_category_index;
    uint32_t selected_action_index;
    char status_message[64];
    bool usb_connected;
    FlipDeckSettings settings;
    char current_category_id[32];
} FlipDeckApp;

/**
 * @brief Initialize the FlipDeck application
 * @param furi_void Pointer to Furi object
 * @return true if initialization successful
 */
bool flipdeck_app_init(void* furi_void);

/**
 * @brief Free resources and shutdown gracefully
 * @param furi_void Pointer to Furi object
 */
void flipdeck_app_free(void* furi_void);

/**
 * @brief Main application loop
 * @param furi_void Pointer to Furi object
 */
void flipdeck_app_loop(void* furi_void);

FlipDeckApp* flipdeck_app_get_context(void);
void flipdeck_app_set_state(FlipDeckState new_state);

#endif // FLIPDECK_APP_H
