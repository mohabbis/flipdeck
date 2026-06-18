/* test_usb_hid.c - Host tests for usb_hid.c key mapping and combo parsing.
 *
 * Uses the #define static trick to expose static functions for direct testing.
 * The Furi HAL stubs make all hardware calls no-ops so the pure mapping
 * logic can be exercised without a device.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* Pull in stubs before any Furi headers are needed */
#include "stubs/furi.h"
#include "stubs/furi_hal.h"
#include "stubs/furi_hal_usb_hid.h"

/* Include the public header to satisfy include guards */
#include "../../usb_hid.h"

/* Expose static functions by stripping the storage class */
#define static
#include "../../usb_hid.c"
#undef static

/* Re-introduce static for the test code below */
static int g_passes = 0;
static int g_failures = 0;

#define EXPECT_EQ(a, b, msg) do { \
    int _a = (int)(a); int _b = (int)(b); \
    if (_a == _b) { printf("  PASS: %s\n", (msg)); g_passes++; } \
    else           { printf("  FAIL: %s — got %d, want %d (line %d)\n", (msg), _a, _b, __LINE__); g_failures++; } \
} while(0)

#define EXPECT_TRUE(expr, msg)  EXPECT_EQ(!!(expr), 1, (msg))
#define EXPECT_FALSE(expr, msg) EXPECT_EQ(!!(expr), 0, (msg))

/* ---------- key_name_to_code ---------- */

static void test_letters(void) {
    EXPECT_EQ(key_name_to_code("A"), 4,  "A -> 4");
    EXPECT_EQ(key_name_to_code("B"), 5,  "B -> 5");
    EXPECT_EQ(key_name_to_code("Z"), 29, "Z -> 29");
    EXPECT_EQ(key_name_to_code("a"), 4,  "a (lowercase) -> 4");
    EXPECT_EQ(key_name_to_code("z"), 29, "z (lowercase) -> 29");
}

static void test_special_keys(void) {
    EXPECT_EQ(key_name_to_code("ENTER"),     40, "ENTER");
    EXPECT_EQ(key_name_to_code("ESCAPE"),    41, "ESCAPE");
    EXPECT_EQ(key_name_to_code("BACKSPACE"), 42, "BACKSPACE");
    EXPECT_EQ(key_name_to_code("TAB"),       43, "TAB");
    EXPECT_EQ(key_name_to_code("SPACE"),     44, "SPACE");
}

static void test_arrow_keys(void) {
    EXPECT_EQ(key_name_to_code("RIGHT"), 79, "RIGHT");
    EXPECT_EQ(key_name_to_code("LEFT"),  80, "LEFT");
    EXPECT_EQ(key_name_to_code("DOWN"),  81, "DOWN");
    EXPECT_EQ(key_name_to_code("UP"),    82, "UP");
}

static void test_function_keys(void) {
    EXPECT_EQ(key_name_to_code("F4"),  61, "F4");
    EXPECT_EQ(key_name_to_code("F5"),  62, "F5");
    EXPECT_EQ(key_name_to_code("F12"), 69, "F12");
}

static void test_unknown_keys(void) {
    EXPECT_EQ(key_name_to_code("UNKNOWN"), 0, "unknown name -> 0");
    EXPECT_EQ(key_name_to_code("F99"),     0, "F99 -> 0");
    EXPECT_EQ(key_name_to_code(""),        0, "empty string -> 0");
}


static void test_text_character_chords(void) {
    HidKeyChord chord;
    EXPECT_TRUE(char_to_chord('A', &chord), "uppercase A maps");
    EXPECT_EQ(chord.key, HID_KEYBOARD_A, "uppercase A key");
    EXPECT_EQ(chord.modifiers, KEY_MOD_LEFT_SHIFT, "uppercase A shift modifier");

    EXPECT_TRUE(char_to_chord('/', &chord), "slash maps");
    EXPECT_EQ(chord.key, HID_KEYBOARD_SLASH, "slash key");
    EXPECT_EQ(chord.modifiers, 0, "slash no modifier");

    EXPECT_TRUE(char_to_chord('|', &chord), "pipe maps");
    EXPECT_EQ(chord.key, HID_KEYBOARD_BACKSLASH, "pipe key");
    EXPECT_EQ(chord.modifiers, KEY_MOD_LEFT_SHIFT, "pipe shift modifier");

    EXPECT_TRUE(char_to_chord('\n', &chord), "newline maps");
    EXPECT_EQ(chord.key, HID_KEYBOARD_RETURN, "newline return key");
}

static void test_shell_command_text_returns_true(void) {
    s_usb_connected = true;
    bool result = usb_hid_send_string("git commit -m \"fix: a_b/c | test\"\n");
    EXPECT_TRUE(result, "shell command punctuation is typeable");
    s_usb_connected = false;
}

/* ---------- modifier_name_to_code ---------- */

static void test_modifiers(void) {
    EXPECT_EQ(modifier_name_to_code("CTRL"),      KEY_MOD_LEFT_CTRL,  "CTRL -> KEY_MOD_LEFT_CTRL");
    EXPECT_EQ(modifier_name_to_code("LEFTCTRL"),  KEY_MOD_LEFT_CTRL,  "LEFTCTRL -> KEY_MOD_LEFT_CTRL");
    EXPECT_EQ(modifier_name_to_code("SHIFT"),     KEY_MOD_LEFT_SHIFT, "SHIFT -> KEY_MOD_LEFT_SHIFT");
    EXPECT_EQ(modifier_name_to_code("LEFTSHIFT"), KEY_MOD_LEFT_SHIFT, "LEFTSHIFT -> KEY_MOD_LEFT_SHIFT");
    EXPECT_EQ(modifier_name_to_code("ALT"),       KEY_MOD_LEFT_ALT,   "ALT -> KEY_MOD_LEFT_ALT");
    EXPECT_EQ(modifier_name_to_code("LEFTALT"),   KEY_MOD_LEFT_ALT,   "LEFTALT -> KEY_MOD_LEFT_ALT");
    EXPECT_EQ(modifier_name_to_code("GUI"),       KEY_MOD_LEFT_GUI,   "GUI -> KEY_MOD_LEFT_GUI");
    EXPECT_EQ(modifier_name_to_code("CMD"),       KEY_MOD_LEFT_GUI,   "CMD -> KEY_MOD_LEFT_GUI");
    EXPECT_EQ(modifier_name_to_code("WIN"),       KEY_MOD_LEFT_GUI,   "WIN -> KEY_MOD_LEFT_GUI");
    EXPECT_EQ(modifier_name_to_code("LEFTGUI"),   KEY_MOD_LEFT_GUI,   "LEFTGUI -> KEY_MOD_LEFT_GUI");
}

static void test_modifiers_case_insensitive(void) {
    EXPECT_EQ(modifier_name_to_code("ctrl"),  KEY_MOD_LEFT_CTRL,  "ctrl (lower) -> KEY_MOD_LEFT_CTRL");
    EXPECT_EQ(modifier_name_to_code("Shift"), KEY_MOD_LEFT_SHIFT, "Shift (mixed) -> KEY_MOD_LEFT_SHIFT");
    EXPECT_EQ(modifier_name_to_code("alt"),   KEY_MOD_LEFT_ALT,   "alt (lower) -> KEY_MOD_LEFT_ALT");
}

static void test_modifiers_dont_collide_with_keycodes(void) {
    /* Regression test: modifiers must live outside the 0-255 keycode range,
     * since usb_hid_send_key_combo ORs them into the same uint16_t button
     * value sent to furi_hal_hid_kb_press(). A narrower (uint8_t) modifier
     * accumulator would silently truncate these to 0. */
    EXPECT_TRUE(modifier_name_to_code("CTRL") > 0xFF, "CTRL modifier bit is above the keycode byte");
    EXPECT_TRUE(modifier_name_to_code("SHIFT") > 0xFF, "SHIFT modifier bit is above the keycode byte");
    EXPECT_TRUE(modifier_name_to_code("ALT") > 0xFF, "ALT modifier bit is above the keycode byte");
    EXPECT_TRUE(modifier_name_to_code("GUI") > 0xFF, "GUI modifier bit is above the keycode byte");
}

static void test_modifiers_unknown(void) {
    EXPECT_EQ(modifier_name_to_code("HYPER"), 0, "HYPER -> 0");
    EXPECT_EQ(modifier_name_to_code(""),      0, "empty -> 0");
}

/* ---------- send_key_combo parsing (requires USB connected) ---------- */

static void test_combo_unknown_key_returns_false(void) {
    s_usb_connected = true;
    bool result = usb_hid_send_key_combo("CTRL+SHIFT+UNKNOWNKEY");
    EXPECT_FALSE(result, "combo with unknown key returns false");
    s_usb_connected = false;
}

static void test_combo_known_key_returns_true(void) {
    s_usb_connected = true;
    bool result = usb_hid_send_key_combo("CTRL+SHIFT+P");
    EXPECT_TRUE(result, "CTRL+SHIFT+P returns true");
    s_usb_connected = false;
}

static void test_combo_single_key_no_modifier(void) {
    s_usb_connected = true;
    bool result = usb_hid_send_key_combo("F5");
    EXPECT_TRUE(result, "single key F5 returns true");
    s_usb_connected = false;
}

static void test_combo_disconnected_returns_false(void) {
    s_usb_connected = false;
    bool result = usb_hid_send_key_combo("CTRL+C");
    EXPECT_FALSE(result, "combo returns false when USB disconnected");
}

/* ---------- runner ---------- */

int main(void) {
    printf("=== FlipDeck usb_hid host tests ===\n\n");

    printf("[key_name_to_code: letters]\n");
    test_letters();

    printf("[key_name_to_code: special keys]\n");
    test_special_keys();

    printf("[key_name_to_code: arrow keys]\n");
    test_arrow_keys();

    printf("[key_name_to_code: function keys]\n");
    test_function_keys();

    printf("[key_name_to_code: unknown]\n");
    test_unknown_keys();

    printf("[text character mapping]\n");
    test_text_character_chords();
    test_shell_command_text_returns_true();

    printf("[modifier_name_to_code]\n");
    test_modifiers();
    test_modifiers_case_insensitive();
    test_modifiers_unknown();
    test_modifiers_dont_collide_with_keycodes();

    printf("[usb_hid_send_key_combo parsing]\n");
    test_combo_unknown_key_returns_false();
    test_combo_known_key_returns_true();
    test_combo_single_key_no_modifier();
    test_combo_disconnected_returns_false();

    printf("\n====================================\n");
    printf("Results: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures > 0 ? 1 : 0;
}
