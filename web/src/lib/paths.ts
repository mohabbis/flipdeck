/**
 * Canonical device and web paths for FlipDeck.
 * This is the single source of truth for all path references across:
 * - Web installer (web/)
 * - Desktop helper (desktop_helper/)
 * - Device paths displayed in C app
 *
 * Update this file when path structure changes; all three layers reference it.
 */

export const PATHS = {
  // On-device paths (Flipper Zero SD card layout)
  DEVICE: {
    // Base data directory where all flipdeck data lives
    DATA_ROOT: "/ext/apps_data/flipdeck",
    // Profiles directory: where JSON profile files are stored
    PROFILES_DIR: "/ext/apps_data/flipdeck/profiles",
    // Snippets directory: where text snippet templates are stored
    SNIPPETS_DIR: "/ext/apps_data/flipdeck/snippets",
    // Settings file
    SETTINGS_FILE: "/ext/apps_data/flipdeck/settings.json",
    // NFC tag → action mappings
    NFC_TAGS_FILE: "/ext/apps_data/flipdeck/nfc_tags.json",
    // Sub-GHz remote → action mappings
    SUBGHZ_REMOTES_FILE: "/ext/apps_data/flipdeck/subghz_remotes.json",
    // Logs directory
    LOGS_DIR: "/ext/apps_data/flipdeck/logs",
    // The installed flipdeck.fap app itself
    APP_PATH: "/ext/apps/Tools/flipdeck.fap",
  },

  // Local filesystem paths (where user prepares files before copying to device)
  LOCAL: {
    // Path to profiles on local SD card / temp directory before copying
    PROFILES_SOURCE: "apps_data/flipdeck/profiles",
    // Path to snippets on local SD card / temp directory before copying
    SNIPPETS_SOURCE: "apps_data/flipdeck/snippets",
  },

  // Web installer paths
  WEB: {
    // API endpoint for fetching all profiles as JSON array
    API_PROFILES: "/api/profiles",
    // API endpoint for downloading a single profile
    API_PROFILE_DOWNLOAD: "/api/profiles/download",
    // API endpoint for downloading a profile pack (ZIP of selected profiles)
    API_PACK: "/api/pack",
    // API endpoint for downloading the full install bundle
    API_INSTALL_BUNDLE: "/api/install-bundle/download",
    // API liveness check
    API_HEALTH: "/api/health",
    // Public profiles directory (populated at build time)
    PUBLIC_PROFILES: "/profiles",
  },

  // Desktop CLI paths
  DESKTOP: {
    // Relative path from project root where profiles are synced
    PROFILES_SYNC: "sd_card/apps_data/flipdeck/profiles",
    // Relative path from project root where snippets are synced
    SNIPPETS_SYNC: "sd_card/apps_data/flipdeck/snippets",
  },
} as const;

/**
 * Get the device-side path for a profile by ID.
 * e.g., "git" → "/ext/apps_data/flipdeck/profiles/git.json"
 */
export function getDeviceProfilePath(profileId: string): string {
  return `${PATHS.DEVICE.PROFILES_DIR}/${profileId}.json`;
}

/**
 * Get the device-side path for a snippet by filename.
 * e.g., "docker.txt" → "/ext/apps_data/flipdeck/snippets/docker.txt"
 */
export function getDeviceSnippetPath(filename: string): string {
  return `${PATHS.DEVICE.SNIPPETS_DIR}/${filename}`;
}
