/**
 * @file profile_manager.c
 * @brief Profile management implementation for SD card JSON profiles
 */

#include "profile_manager.h"
#include "flipdeck_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <inttypes.h>
#include <furi_core/string.h>
#include <furi_core/fs.h>
#include <furi_core/storage.h>

#define PROFILE_BASE_PATH "/stor0800/flipdeck/profiles"
#define SETTINGS_PATH "/stor0800/flipdeck/settings.json"

// Simple string helpers for JSON parsing (no external library)
static char* find_json_string(char* json, const char* key, char* output, size_t max_len) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    char* pos = strstr(json, search_key);
    if(!pos) return NULL;
    
    pos = strstr(pos, ":");
    if(!pos) return NULL;
    
    // Find opening quote
    pos = strchr(pos, '"');
    if(!pos) return NULL;
    pos++;
    
    // Find closing quote
    char* end = strchr(pos, '"');
    if(!end) return NULL;
    
    size_t len = end - pos;
    if(len >= max_len) len = max_len - 1;
    strncpy(output, pos, len);
    output[len] = '\0';
    return output;
}

static bool find_json_bool(char* json, const char* key, bool default_val) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    char* pos = strstr(json, search_key);
    if(!pos) return default_val;
    
    pos = strstr(pos, ":");
    if(!pos) return default_val;
    
    if(strstr(pos, "true")) return true;
    if(strstr(pos, "false")) return false;
    return default_val;
}

static FlipDeckActionType parse_action_type(char* json) {
    char type_str[16];
    if(find_json_string(json, "type", type_str, sizeof(type_str))) {
        if(strcmp(type_str, "key_combo") == 0) return FlipDeckActionType_KeyCombo;
        if(strcmp(type_str, "key") == 0) return FlipDeckActionType_Key;
    }
    return FlipDeckActionType_Text;
}

bool profile_manager_load_all_categories(FlipDeckProfileCategory* categories, uint32_t* count) {
    FURI_LOG_I("FlipDeck", "Loading all profile categories");
    
    *count = 0;
    
    // Ensure directory exists
    furi_mkdir(PROFILE_BASE_PATH);
    
    FuriFs* dir = furi_open(PROFILE_BASE_PATH, FuriFlagRead | FuriFlagDirectory, false);
    if(!dir) {
        FURI_LOG_W("FlipDeck", "Profiles directory not found");
        return false;
    }
    
    char filename[64];
    while(furi_fs_read_directory(dir, filename, sizeof(filename)) && *count < FLIPDECK_MAX_CATEGORIES) {
        if(strstr(filename, ".json")) {
            char category_id[32];
            strncpy(category_id, filename, sizeof(category_id) - 1);
            category_id[sizeof(category_id) - 1] = '\0';
            size_t len = strlen(filename);
            if(len > 5 && strcmp(filename + len - 5, ".json") == 0) {
                category_id[len - 5] = '\0';
            }
            
            strncpy(categories[*count].id, category_id, sizeof(categories[*count].id) - 1);
            categories[*count].id[sizeof(categories[*count].id) - 1] = '\0';
            categories[*count].action_count = 0;
            (*count)++;
        }
    }
    
    furi_close(dir);
    FURI_LOG_I("FlipDeck", "Loaded %" PRIu32 " categories", *count);
    return *count > 0;
}

bool profile_manager_load_category(const char* category_id, FlipDeckProfileCategory* category) {
    FURI_LOG_I("FlipDeck", "Loading category: %s", category_id);
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%s.json", PROFILE_BASE_PATH, category_id);
    
    FuriFs* file = furi_open(path, FuriFlagRead, false);
    if(!file) {
        FURI_LOG_E("FlipDeck", "Category file not found: %s", path);
        return false;
    }
    
    // Read file content
    char buffer[2048];
    uint32_t bytes_read = furi_stream_read(file, buffer, sizeof(buffer) - 1);
    buffer[bytes_read] = '\0';
    
    // Parse JSON
    find_json_string(buffer, "name", category->name, sizeof(category->name));
    find_json_string(buffer, "id", category->id, sizeof(category->id));
    find_json_string(buffer, "description", category->description, sizeof(category->description));
    
    // Find actions array
    category->action_count = 0;
    char* actions_start = strstr(buffer, "\"actions\"");
    if(actions_start) {
        char* arr_start = strchr(actions_start, '[');
        char* arr_end = strrchr(buffer, ']');
        
        if(arr_start && arr_end && arr_end > arr_start) {
            // Parse each action
            char* pos = arr_start + 1;
            while(pos < arr_end && category->action_count < FLIPDECK_MAX_ACTIONS_PER_CATEGORY) {
                char* action_end = strchr(pos, '}');
                if(!action_end) break;
                
                // Find the opening brace
                char* action_start = pos;
                while(pos < action_end && *pos != '{') pos++;
                if(pos >= action_end) break;
                action_start = pos;
                
                // Create a mini JSON for this action
                char action_json[512];
                uint32_t action_len = action_end - action_start + 1;
                if(action_len >= sizeof(action_json)) action_len = sizeof(action_json) - 1;
                strncpy(action_json, action_start, action_len);
                action_json[action_len] = '\0';
                
                // Parse action fields
                find_json_string(action_json, "label", category->actions[category->action_count].label, 
                    sizeof(category->actions[category->action_count].label));
                find_json_string(action_json, "value", category->actions[category->action_count].value,
                    sizeof(category->actions[category->action_count].value));
                category->actions[category->action_count].type = parse_action_type(action_json);
                category->actions[category->action_count].confirm = find_json_bool(action_json, "confirm", true);
                
                category->action_count++;
                pos = action_end + 1;
            }
        }
    }
    
    furi_close(file);
    FURI_LOG_I("FlipDeck", "Loaded category '%s' with %" PRIu32 " actions", category->name, category->action_count);
    return true;
}

bool profile_manager_save_category(FlipDeckProfileCategory* category) {
    FURI_LOG_I("FlipDeck", "Saving category: %s", category->name);
    
    if(!category || !category->id[0]) {
        FURI_LOG_E("FlipDeck", "Invalid category for saving");
        return false;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%s.json", PROFILE_BASE_PATH, category->id);
    
    // Build JSON content
    char json[4096];
    uint32_t offset = 0;
    offset += snprintf(json + offset, sizeof(json) - offset,
        "{\n  \"name\": \"%s\",\n  \"id\": \"%s\",\n  \"description\": \"%s\",\n  \"actions\": [\n",
        category->name, category->id, category->description);
    
    for(uint32_t i = 0; i < category->action_count && offset < sizeof(json) - 64; i++) {
        FlipDeckAction* action = &category->actions[i];
        const char* type_str = "text";
        if(action->type == FlipDeckActionType_Key) type_str = "key";
        else if(action->type == FlipDeckActionType_KeyCombo) type_str = "key_combo";
        
        // Escape quotes in value
        char escaped_value[FLIPDECK_MAX_COMMAND_LENGTH];
        uint32_t e = 0;
        for(uint32_t j = 0; action->value[j] && e < sizeof(escaped_value) - 1; j++) {
            if(action->value[j] == '"') escaped_value[e++] = '\\';
            escaped_value[e++] = action->value[j];
        }
        escaped_value[e] = '\0';
        
        // Escape quotes in label
        char escaped_label[64];
        uint32_t el = 0;
        for(uint32_t j = 0; action->label[j] && el < sizeof(escaped_label) - 1; j++) {
            if(action->label[j] == '"') escaped_label[el++] = '\\';
            escaped_label[el++] = action->label[j];
        }
        escaped_label[el] = '\0';
        
        offset += snprintf(json + offset, sizeof(json) - offset,
            "    {\"label\": \"%s\", \"type\": \"%s\", \"value\": \"%s\", \"confirm\": %s}%s\n",
            escaped_label, type_str, escaped_value,
            action->confirm ? "true" : "false",
            (i < category->action_count - 1) ? "," : "");
    }
    
    offset += snprintf(json + offset, sizeof(json) - offset, "  ]\n}\n");
    
    // Write to file
    FuriFs* file = furi_open(path, FuriFlagWrite | FuriFlagCreate, true);
    if(!file) {
        FURI_LOG_E("FlipDeck", "Failed to open file for writing: %s", path);
        return false;
    }
    
    furi_stream_write(file, json, strlen(json));
    furi_close(file);
    
    FURI_LOG_I("FlipDeck", "Saved category '%s' to %s", category->name, path);
    return true;
}

bool profile_manager_list_categories(char category_ids[][32], uint32_t* count) {
    FURI_LOG_I("FlipDeck", "Listing categories");
    
    *count = 0;
    
    // Ensure directory exists
    if(furi_mkdir(PROFILE_BASE_PATH) != FuriStatusOK) {
        // Directory might already exist
    }
    
    FuriFs* dir = furi_open(PROFILE_BASE_PATH, FuriFlagRead | FuriFlagDirectory, false);
    if(!dir) {
        // Create default categories including new profiles
        const char* defaults[] = {"git", "node", "python", "docker", "system", "snippets", "aws", "vscode", "presentation"};
        for(int i = 0; i < 9; i++) {
            strncpy(category_ids[*count], defaults[i], 31);
            (*count)++;
        }
        return true;
    }
    
    char filename[64];
    while(furi_fs_read_directory(dir, filename, sizeof(filename)) && *count < FLIPDECK_MAX_CATEGORIES) {
        if(strstr(filename, ".json")) {
            strncpy(category_ids[*count], filename, 31);
            size_t len = strlen(filename);
            if(len > 5 && strcmp(filename + len - 5, ".json") == 0) {
                category_ids[*count][len - 5] = '\0';
            }
            (*count)++;
        }
    }
    
    furi_close(dir);
    return *count > 0;
}

bool profile_manager_load_settings(FlipDeckSettings* settings) {
    FURI_LOG_I("FlipDeck", "Loading settings");
    
    FuriFs* file = furi_open(SETTINGS_PATH, FuriFlagRead, false);
    if(!file) {
        FURI_LOG_W("FlipDeck", "No settings file found");
        goto default_settings;
    }
    
    char buffer[512];
    uint32_t bytes_read = furi_stream_read(file, buffer, sizeof(buffer) - 1);
    buffer[bytes_read] = '\0';
    furi_close(file);
    
    // Parse settings with defaults
    settings->send_delay_ms = 100;
    settings->confirm_before_send = find_json_bool(buffer, "confirm_before_send", true);
    settings->auto_detect_usb = find_json_bool(buffer, "auto_detect_usb", true);
    settings->show_icons = find_json_bool(buffer, "show_icons", true);
    settings->show_descriptions = find_json_bool(buffer, "show_descriptions", true);
    find_json_string(buffer, "startup_category", settings->startup_category, 
        sizeof(settings->startup_category));
    
    return true;

default_settings:
    settings->send_delay_ms = 100;
    settings->confirm_before_send = true;
    settings->auto_detect_usb = true;
    settings->show_icons = true;
    settings->show_descriptions = true;
    strcpy(settings->startup_category, "");
    settings->long_snippet_warn_state = 0;  // Initialize new field
    return false;
}

bool profile_manager_save_settings(FlipDeckSettings* settings) {
    FURI_LOG_I("FlipDeck", "Saving settings");
    
    if(!settings) {
        FURI_LOG_E("FlipDeck", "Invalid settings for saving");
        return false;
    }
    
    // Build JSON content
    char json[512];
    snprintf(json, sizeof(json),
        "{\n"
        "  \"confirm_before_send\": %s,\n"
        "  \"auto_detect_usb\": %s,\n"
        "  \"show_icons\": %s,\n"
        "  \"show_descriptions\": %s,\n"
        "  \"send_delay_ms\": %" PRIu32 ",\n"
        "  \"startup_category\": \"%s\"\n"
        "}\n",
        settings->confirm_before_send ? "true" : "false",
        settings->auto_detect_usb ? "true" : "false",
        settings->show_icons ? "true" : "false",
        settings->show_descriptions ? "true" : "false",
        settings->send_delay_ms,
        settings->startup_category);
    
    // Ensure directory exists
    furi_mkdir("/ext/flipdeck");
    
    // Write to file
    FuriFs* file = furi_open(SETTINGS_PATH, FuriFlagWrite | FuriFlagCreate, true);
    if(!file) {
        FURI_LOG_E("FlipDeck", "Failed to open settings file for writing");
        return false;
    }
    
    furi_stream_write(file, json, strlen(json));
    furi_close(file);
    
    FURI_LOG_I("FlipDeck", "Settings saved successfully");
    return true;
}

/**
 * @brief Check if value contains potentially dangerous commands
 * @param value Command string to check
 * @return true if value is safe
 */
bool profile_manager_is_value_safe(const char* value) {
    if(!value) return false;
    
    // List of dangerous patterns to block
    const char* dangerous_patterns[] = {
        "rm -rf",           // Recursive delete
        "sudo",             // Superuser
        "curl | sh",        // Remote script execution
        "wget | sh",        // Remote script execution
        "curl.ssh",         // SSH through curl
        "mkfs",             // Format filesystem
        "dd if=",           // Disk operations
        ":(){ :|:& };:",    // Fork bomb
        "> /dev/sd",        // Disk write
        "chmod 777",        // Insecure permissions
        "chown root",       // Root ownership
        NULL
    };
    
    for(int i = 0; dangerous_patterns[i]; i++) {
        if(strstr(value, dangerous_patterns[i])) {
            FURI_LOG_W("FlipDeck", "Blocked dangerous command: %s", dangerous_patterns[i]);
            return false;
        }
    }
    
    // Check for credential-related keys
    const char* credential_patterns[] = {
        "PASSWORD",
        "TOKEN",
        "API_KEY",
        "SECRET",
        "PRIVATE_KEY",
        NULL
    };
    
    char upper_value[256];
    for(uint32_t i = 0; value[i] && i < sizeof(upper_value) - 1; i++) {
        upper_value[i] = toupper(value[i]);
    }
    upper_value[sizeof(upper_value) - 1] = '\0';
    
    for(int i = 0; credential_patterns[i]; i++) {
        if(strstr(upper_value, credential_patterns[i])) {
            FURI_LOG_W("FlipDeck", "Blocked credential-related command");
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Validate action for safety before sending
 * @param action Action to validate
 * @return true if action is safe to send
 */
bool profile_manager_validate_action(FlipDeckAction* action) {
    if(!action) return false;
    
    // Key actions are generally safe (navigation keys, etc.)
    if(action->type == FlipDeckActionType_Key || 
       action->type == FlipDeckActionType_KeyCombo) {
        return true;
    }
    
    // Text actions need validation
    if(action->type == FlipDeckActionType_Text) {
        return profile_manager_is_value_safe(action->value);
    }
    
    return false;
}