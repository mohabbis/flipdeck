/* test_settings.c - Host tests for settings defaults and JSON parsing.
 *
 * Includes profile_manager.c and settings.c directly so the static JSON
 * helpers (find_json_bool / find_json_string) can be exercised alongside the
 * public settings API. Filesystem-facing functions are stubbed no-ops, so the
 * load path here covers the "no settings file" defaults branch.
 */

#include <stdio.h>
#include <string.h>

#include "../../profile_manager.c"
#include "../../settings.c"

static int g_passes = 0;
static int g_failures = 0;

#define EXPECT_TRUE(expr, msg) do { \
    if (expr) { printf("  PASS: %s\n", (msg)); g_passes++; } \
    else       { printf("  FAIL: %s  (line %d)\n", (msg), __LINE__); g_failures++; } \
} while(0)

#define EXPECT_FALSE(expr, msg) EXPECT_TRUE(!(expr), (msg))
#define EXPECT_EQ_INT(a, b, msg) EXPECT_TRUE((a) == (b), (msg))
#define EXPECT_STR_EQ(a, b, msg) EXPECT_TRUE(strcmp((a), (b)) == 0, (msg))

/* ---------- find_json_bool ---------- */

static void test_find_json_bool(void) {
    char json[] = "{ \"confirm_before_send\": true, \"show_icons\": false }";
    EXPECT_TRUE(find_json_bool(json, "confirm_before_send", false), "reads true value");
    EXPECT_FALSE(find_json_bool(json, "show_icons", true), "reads false value");
    EXPECT_TRUE(find_json_bool(json, "missing_key", true), "missing key returns default true");
    EXPECT_FALSE(find_json_bool(json, "missing_key", false), "missing key returns default false");
}

/* ---------- find_json_string ---------- */

static void test_find_json_string(void) {
    char json[] = "{ \"startup_category\": \"dev\", \"name\": \"Git\" }";
    char out[32];

    memset(out, 0, sizeof(out));
    EXPECT_TRUE(find_json_string(json, "startup_category", out, sizeof(out)) != NULL,
        "returns non-NULL for present key");
    EXPECT_STR_EQ(out, "dev", "extracts startup_category value");

    memset(out, 0, sizeof(out));
    find_json_string(json, "name", out, sizeof(out));
    EXPECT_STR_EQ(out, "Git", "extracts name value");

    memset(out, 0, sizeof(out));
    EXPECT_TRUE(find_json_string(json, "absent", out, sizeof(out)) == NULL,
        "returns NULL for missing key");
}

/* ---------- find_json_uint ---------- */

static void test_find_json_uint(void) {
    char json[] = "{ \"delay_ms\": 250, \"zero\": 0, \"spaced\":   42 }";
    EXPECT_EQ_INT(find_json_uint(json, "delay_ms", 99u), 250u, "reads numeric value");
    EXPECT_EQ_INT(find_json_uint(json, "zero", 99u), 0u, "reads explicit zero");
    EXPECT_EQ_INT(find_json_uint(json, "spaced", 99u), 42u, "skips whitespace before digits");
    EXPECT_EQ_INT(find_json_uint(json, "missing", 7u), 7u, "missing key returns default");

    char nan[] = "{ \"delay_ms\": \"oops\" }";
    EXPECT_EQ_INT(find_json_uint(nan, "delay_ms", 5u), 5u, "non-numeric value returns default");
}

/* ---------- load_settings: no file → defaults ---------- */

static void test_load_settings_defaults(void) {
    FlipDeckSettings settings;
    memset(&settings, 0xAA, sizeof(settings)); /* poison to prove every field is set */

    /* Stub storage has no settings file, so load returns false and fills defaults. */
    bool loaded = profile_manager_load_settings(&settings);
    EXPECT_FALSE(loaded, "load returns false when no settings file present");
    EXPECT_EQ_INT(settings.send_delay_ms, 100u, "default send_delay_ms is 100");
    EXPECT_TRUE(settings.confirm_before_send, "default confirm_before_send is true");
    EXPECT_TRUE(settings.auto_detect_usb, "default auto_detect_usb is true");
    EXPECT_TRUE(settings.show_icons, "default show_icons is true");
    EXPECT_TRUE(settings.show_descriptions, "default show_descriptions is true");
    EXPECT_STR_EQ(settings.startup_category, "", "default startup_category is empty");
    EXPECT_EQ_INT(settings.long_snippet_warn_state, 0u, "default long_snippet_warn_state is 0");
}

/* ---------- settings_reset_to_defaults ---------- */

static void test_reset_to_defaults(void) {
    FlipDeckSettings settings;
    memset(&settings, 0xAA, sizeof(settings));

    settings_reset_to_defaults(&settings);
    EXPECT_EQ_INT(settings.send_delay_ms, 100u, "reset send_delay_ms to 100");
    EXPECT_TRUE(settings.confirm_before_send, "reset confirm_before_send to true");
    EXPECT_TRUE(settings.auto_detect_usb, "reset auto_detect_usb to true");
    EXPECT_TRUE(settings.show_icons, "reset show_icons to true");
    EXPECT_TRUE(settings.show_descriptions, "reset show_descriptions to true");
    EXPECT_STR_EQ(settings.startup_category, "", "reset startup_category to empty");
    EXPECT_EQ_INT(settings.long_snippet_warn_state, 0u, "reset long_snippet_warn_state to 0");
}

/* ---------- favorites parsing ---------- */

static void test_load_settings_parses_favorites(void) {
    /* parse_favorites() is exercised directly against a hand-built JSON
     * buffer, since the stub read_text_file() always reports "no file". */
    char json[] =
        "{\n"
        "  \"confirm_before_send\": true,\n"
        "  \"favorites\": [\n"
        "    {\"category_id\": \"git\", \"label\": \"Git Status\"},\n"
        "    {\"category_id\": \"node\", \"label\": \"Run Dev\"}\n"
        "  ]\n"
        "}\n";

    FlipDeckSettings settings;
    memset(&settings, 0xAA, sizeof(settings));
    parse_favorites(json, &settings);

    EXPECT_EQ_INT(settings.favorite_count, 2u, "parses two favorites");
    EXPECT_STR_EQ(settings.favorites[0].category_id, "git", "first favorite category_id");
    EXPECT_STR_EQ(settings.favorites[0].label, "Git Status", "first favorite label");
    EXPECT_STR_EQ(settings.favorites[1].category_id, "node", "second favorite category_id");
    EXPECT_STR_EQ(settings.favorites[1].label, "Run Dev", "second favorite label");
}

static void test_load_settings_no_favorites_key(void) {
    char json[] = "{ \"confirm_before_send\": true }";
    FlipDeckSettings settings;
    memset(&settings, 0xAA, sizeof(settings));
    parse_favorites(json, &settings);
    EXPECT_EQ_INT(settings.favorite_count, 0u, "no favorites key means zero favorites");
}

/* ---------- runner ---------- */

int main(void) {
    printf("=== FlipDeck settings host tests ===\n\n");

    printf("[find_json_bool]\n");
    test_find_json_bool();

    printf("[find_json_string]\n");
    test_find_json_string();

    printf("[find_json_uint]\n");
    test_find_json_uint();

    printf("[load_settings defaults]\n");
    test_load_settings_defaults();

    printf("[reset_to_defaults]\n");
    test_reset_to_defaults();

    printf("[favorites parsing]\n");
    test_load_settings_parses_favorites();
    test_load_settings_no_favorites_key();

    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures > 0 ? 1 : 0;
}
