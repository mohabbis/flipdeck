/* test_profile_manager_target.c - Host tests for parse_action_target/parse_action_type */

#include <stdio.h>
#include <string.h>

#include "../../profile_manager.h"

static int g_passes = 0;
static int g_failures = 0;

#define EXPECT_EQ(a, b, msg) do { \
    int _a = (int)(a); int _b = (int)(b); \
    if (_a == _b) { printf("  PASS: %s\n", (msg)); g_passes++; } \
    else           { printf("  FAIL: %s — got %d, want %d (line %d)\n", (msg), _a, _b, __LINE__); g_failures++; } \
} while(0)

static void test_parse_action_target_default(void) {
    char json[] = "{\"label\": \"Git Status\", \"type\": \"text\", \"value\": \"git status\\n\"}";
    EXPECT_EQ(parse_action_target(json), FlipDeckActionTarget_Usb, "missing target defaults to usb_hid");
}

static void test_parse_action_target_usb_hid(void) {
    char json[] = "{\"label\": \"Git Status\", \"type\": \"text\", \"value\": \"git status\\n\", \"target\": \"usb_hid\"}";
    EXPECT_EQ(parse_action_target(json), FlipDeckActionTarget_Usb, "explicit usb_hid target");
}

static void test_parse_action_target_wifi_uart(void) {
    char json[] = "{\"label\": \"Scan WiFi APs\", \"type\": \"text\", \"value\": \"scanap\\n\", \"target\": \"wifi_uart\"}";
    EXPECT_EQ(parse_action_target(json), FlipDeckActionTarget_WifiUart, "wifi_uart target");
}

static void test_parse_action_target_unknown(void) {
    char json[] = "{\"label\": \"Mystery\", \"type\": \"text\", \"value\": \"x\\n\", \"target\": \"bluetooth\"}";
    EXPECT_EQ(parse_action_target(json), FlipDeckActionTarget_Usb, "unrecognized target falls back to usb_hid");
}

static void test_parse_action_type_still_works(void) {
    char json[] = "{\"label\": \"Scan WiFi APs\", \"type\": \"text\", \"value\": \"scanap\\n\", \"target\": \"wifi_uart\"}";
    EXPECT_EQ(parse_action_type(json), FlipDeckActionType_Text, "type parsing unaffected by target field");
}

int main(void) {
    printf("=== FlipDeck profile_manager target host tests ===\n\n");

    printf("[parse_action_target]\n");
    test_parse_action_target_default();
    test_parse_action_target_usb_hid();
    test_parse_action_target_wifi_uart();
    test_parse_action_target_unknown();

    printf("[parse_action_type]\n");
    test_parse_action_type_still_works();

    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures > 0 ? 1 : 0;
}
