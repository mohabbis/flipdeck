/**
 * @file paths.h
 * @brief Canonical device paths for FlipDeck.
 *
 * This is the single source of truth for device paths across all layers
 * (C app, web installer, desktop helper). Update this file when the
 * device path structure changes.
 */

#ifndef FLIPDECK_PATHS_H
#define FLIPDECK_PATHS_H

/**
 * On-device base paths (Flipper Zero SD card layout)
 */
#define FLIPDECK_DATA_PATH "/ext/apps_data/flipdeck"
#define FLIPDECK_PROFILES_PATH "/ext/apps_data/flipdeck/profiles"
#define FLIPDECK_SNIPPETS_PATH "/ext/apps_data/flipdeck/snippets"
#define FLIPDECK_SETTINGS_PATH "/ext/apps_data/flipdeck/settings.json"
#define FLIPDECK_LOGS_PATH "/ext/apps_data/flipdeck/logs"
#define FLIPDECK_APP_PATH "/ext/apps/Tools/flipdeck.fap"

/**
 * Helper macro to build a profile path by ID.
 * Usage: char path[64]; snprintf(path, sizeof(path), FLIPDECK_PROFILE_PATH("git"));
 */
#define FLIPDECK_PROFILE_PATH(id) FLIPDECK_PROFILES_PATH "/" id ".json"

/**
 * Helper macro to build a snippet path by filename.
 * Usage: char path[64]; snprintf(path, sizeof(path), FLIPDECK_SNIPPET_PATH("docker.txt"));
 */
#define FLIPDECK_SNIPPET_PATH(filename) FLIPDECK_SNIPPETS_PATH "/" filename

#endif
