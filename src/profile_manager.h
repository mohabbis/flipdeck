/**
 * @file profile_manager.h
 * @brief Profile management for FlipDeck
 */

#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include "flipdeck_app.h"
#include <furi.h>

#ifdef UFBT_HOST_BUILD
/* Host test build - include stubs */
#include "tests/host/stubs/furi.h"
#endif

// Maximum number of profile categories
#define FLIPDECK_MAX_CATEGORIES 10
#define FLIPDECK_MAX_ACTIONS_PER_CATEGORY 20

/** Action types for profiles */
typedef enum {
    FlipDeckActionType_Text,
    FlipDeckActionType_Key,
    FlipDeckActionType_KeyCombo,
} FlipDeckActionType;

/** Destination for an action's output */
typedef enum {
    FlipDeckActionTarget_Usb,
    FlipDeckActionTarget_WifiUart,
} FlipDeckActionTarget;

/**
 * @brief Single action within a profile
 */
typedef struct {
    char label[64];
    FlipDeckActionType type;
    char value[FLIPDECK_MAX_COMMAND_LENGTH];
    bool confirm;
    FlipDeckActionTarget target;
    uint32_t delay_ms;
} FlipDeckAction;

/**
 * @brief Profile category (group of related actions)
 */
typedef struct {
    char id[32];
    char name[64];
    char description[128];
    char category[24];
    char icon[16];
    uint32_t action_count;
    FlipDeckAction actions[FLIPDECK_MAX_ACTIONS_PER_CATEGORY];
} FlipDeckProfileCategory;

/**
 * @brief Load all profile categories from SD card
 * @param categories Array to store categories
 * @param count Pointer to store number of categories loaded
 * @return true if profiles were found and loaded
 */
bool profile_manager_load_all_categories(FlipDeckProfileCategory* categories, uint32_t* count);

/**
 * @brief Load a single profile category from JSON
 * @param category_id The category ID (e.g., "git", "node")
 * @param category Pointer to store loaded category
 * @return true if loaded successfully
 */
bool profile_manager_load_category(const char* category_id, FlipDeckProfileCategory* category);

/**
 * @brief Save a profile category to SD card (as JSON)
 * @param category Pointer to category data
 * @return true if saved successfully
 */
bool profile_manager_save_category(FlipDeckProfileCategory* category);

/**
 * @brief Get list of available category files
 * @param category_ids Array to store category IDs
 * @param count Pointer to store number of categories
 * @return true if categories found
 */
bool profile_manager_list_categories(char category_ids[][32], uint32_t* count);

/**
 * @brief Load settings from SD card
 * @param settings Pointer to settings structure
 * @return true if loaded successfully
 */
bool profile_manager_load_settings(FlipDeckSettings* settings);

/**
 * @brief Save settings to SD card
 * @param settings Pointer to settings structure
 * @return true if saved successfully
 */
bool profile_manager_save_settings(FlipDeckSettings* settings);

/**
 * @brief Validate action for safety before sending
 * @param action Action to validate
 * @return true if action is safe to send
 */
bool profile_manager_validate_action(FlipDeckAction* action);

/**
 * @brief Check whether (category_id, label) is currently favorited
 * @return true if a matching favorite exists in settings->favorites
 */
bool profile_manager_is_favorite(
    const FlipDeckSettings* settings,
    const char* category_id,
    const char* label);

/**
 * @brief Add or remove (category_id, label) from settings->favorites in place
 * @return true if the action is favorited after the toggle, false if it was
 *         removed, or if it wasn't present and there was no room to add it
 *         (favorite_count already at FLIPDECK_MAX_FAVORITES)
 */
bool profile_manager_toggle_favorite(
    FlipDeckSettings* settings,
    const char* category_id,
    const char* label);

/**
 * @brief Check if value contains potentially dangerous commands
 * @param value Command string to check
 * @return true if value is safe
 */
bool profile_manager_is_value_safe(const char* value);

/**
 * @brief Parse the "type" field of an action's JSON object
 * @param json Mini JSON object for a single action
 * @return Parsed action type, defaulting to FlipDeckActionType_Text
 */
FlipDeckActionType parse_action_type(char* json);

/**
 * @brief Parse the "target" field of an action's JSON object
 * @param json Mini JSON object for a single action
 * @return Parsed action target, defaulting to FlipDeckActionTarget_Usb
 */
FlipDeckActionTarget parse_action_target(char* json);

#endif // PROFILE_MANAGER_H