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

// Maximum number of NFC tag -> action mappings, and the buffer size for a
// hex-encoded UID (ISO14443 UIDs are 4, 7, or 10 bytes -> up to 20 hex chars).
#define FLIPDECK_MAX_NFC_TAGS 16
#define FLIPDECK_NFC_UID_HEX_LEN 21

/**
 * @brief A single NFC tag -> action mapping. References the action by
 * (category_id, label) rather than duplicating it, same as FlipDeckFavorite,
 * so the action is always re-read fresh from its category file.
 */
typedef struct {
    char uid_hex[FLIPDECK_NFC_UID_HEX_LEN];
    char category_id[32];
    char label[64];
} FlipDeckNfcTag;

// Maximum number of Sub-GHz remote -> action mappings, and the buffer size
// for a hashed signature (64-bit FNV-1a -> 16 hex chars + NUL).
#define FLIPDECK_MAX_SUBGHZ_REMOTES 16
#define FLIPDECK_SUBGHZ_SIG_HEX_LEN 17

/**
 * @brief A single Sub-GHz remote button -> action mapping. References the
 * action by (category_id, label), same as FlipDeckNfcTag/FlipDeckFavorite.
 * The key is a hash of the decoded protocol string
 * (subghz_protocol_decoder_base_get_string()), not the raw string - see
 * profile_manager_hash_subghz_signature() for why.
 */
typedef struct {
    char signature_hex[FLIPDECK_SUBGHZ_SIG_HEX_LEN];
    char category_id[32];
    char label[64];
} FlipDeckSubghzRemote;

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
 * @brief Load NFC tag -> action mappings from nfc_tags.json
 * @param tags Array to store mappings (capacity FLIPDECK_MAX_NFC_TAGS)
 * @param count Pointer to store number of mappings loaded
 * @return true if the file existed and was read (even if it had zero tags)
 */
bool profile_manager_load_nfc_tags(FlipDeckNfcTag* tags, uint32_t* count);

/**
 * @brief Save NFC tag -> action mappings to nfc_tags.json
 * @param tags Array of mappings to save
 * @param count Number of mappings in the array
 * @return true if saved successfully
 */
bool profile_manager_save_nfc_tags(const FlipDeckNfcTag* tags, uint32_t count);

/**
 * @brief Find the mapping for a given UID, if any
 * @param tags Array of loaded mappings
 * @param count Number of mappings in the array
 * @param uid_hex Uppercase hex UID to look up
 * @return Pointer to the matching entry within `tags`, or NULL if not found
 */
const FlipDeckNfcTag* profile_manager_find_nfc_tag(
    const FlipDeckNfcTag* tags,
    uint32_t count,
    const char* uid_hex);

/**
 * @brief Hex-encode a raw UID into an uppercase string (e.g. {0x04,0xA1} -> "04A1")
 * @param uid Raw UID bytes
 * @param uid_len Number of bytes in uid
 * @param out Output buffer
 * @param out_size Size of out; must be at least uid_len*2 + 1
 * @return true on success, false if out_size is too small or args are NULL
 */
bool profile_manager_hex_encode_uid(
    const uint8_t* uid,
    size_t uid_len,
    char* out,
    size_t out_size);

/**
 * @brief Load Sub-GHz remote -> action mappings from subghz_remotes.json
 * @param remotes Array to store mappings (capacity FLIPDECK_MAX_SUBGHZ_REMOTES)
 * @param count Pointer to store number of mappings loaded
 * @return true if the file existed and was read (even if it had zero remotes)
 */
bool profile_manager_load_subghz_remotes(FlipDeckSubghzRemote* remotes, uint32_t* count);

/**
 * @brief Save Sub-GHz remote -> action mappings to subghz_remotes.json
 * @param remotes Array of mappings to save
 * @param count Number of mappings in the array
 * @return true if saved successfully
 */
bool profile_manager_save_subghz_remotes(const FlipDeckSubghzRemote* remotes, uint32_t count);

/**
 * @brief Find the mapping for a given signature, if any
 * @param remotes Array of loaded mappings
 * @param count Number of mappings in the array
 * @param signature_hex Hex signature to look up
 * @return Pointer to the matching entry within `remotes`, or NULL if not found
 */
const FlipDeckSubghzRemote* profile_manager_find_subghz_remote(
    const FlipDeckSubghzRemote* remotes,
    uint32_t count,
    const char* signature_hex);

/**
 * @brief Hash a decoded Sub-GHz protocol string into a fixed-width uppercase
 * hex fingerprint (64-bit FNV-1a). Fixed-code remotes produce byte-identical
 * decoded text on every press, so this hash is stable across presses;
 * rolling-code remotes produce different text each press, so the hash
 * naturally never repeats and such remotes simply never re-match (see
 * docs/subghz_trigger_spec.md).
 * @param text Decoded protocol string (from subghz_protocol_decoder_base_get_string)
 * @param out Output buffer
 * @param out_size Size of out; must be at least FLIPDECK_SUBGHZ_SIG_HEX_LEN
 * @return true on success, false if text or out is NULL, or out_size is too small
 */
bool profile_manager_hash_subghz_signature(const char* text, char* out, size_t out_size);

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