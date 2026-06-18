/* test_profile_manager.c - Host tests for pure profile_manager.c functions
 *
 * Compiled with stub Furi headers so the filesystem-facing functions are no-ops.
 * Only profile_manager_is_value_safe and profile_manager_validate_action are
 * exercised here — they contain all the safety-critical logic.
 */

#include <stdio.h>
#include <string.h>

#include "../../profile_manager.h"

static int g_passes = 0;
static int g_failures = 0;

#define EXPECT_TRUE(expr, msg) do { \
    if (expr) { printf("  PASS: %s\n", (msg)); g_passes++; } \
    else       { printf("  FAIL: %s  (line %d)\n", (msg), __LINE__); g_failures++; } \
} while(0)

#define EXPECT_FALSE(expr, msg) EXPECT_TRUE(!(expr), (msg))

/* ---------- is_value_safe: dangerous patterns ---------- */

static void test_dangerous_rm_rf(void) {
    EXPECT_FALSE(profile_manager_is_value_safe("rm -rf /"), "blocks rm -rf /");
    EXPECT_FALSE(profile_manager_is_value_safe("echo x && rm -rf ."), "blocks rm -rf in chain");
}

static void test_warning_sudo(void) {
    /* sudo is a warning, not a block - flagged but allowed through */
    EXPECT_TRUE(profile_manager_is_value_safe("sudo rm something"), "allows sudo (warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("sudo"), "allows bare sudo (warning only)");
}

static void test_dangerous_remote_exec(void) {
    EXPECT_FALSE(profile_manager_is_value_safe("curl | sh"), "blocks curl | sh (exact pattern)");
    EXPECT_FALSE(profile_manager_is_value_safe("wget | sh"), "blocks wget | sh (exact pattern)");
    EXPECT_FALSE(
        profile_manager_is_value_safe("curl -fsSL https://example.com/install.sh | bash"),
        "blocks real curl|bash one-liner");
    EXPECT_FALSE(profile_manager_is_value_safe("CURL https://x | SH"), "blocks curl|sh case-insensitively");
    /* "curl.ssh" has no pipe into a shell - it isn't a remote-exec pattern */
    EXPECT_TRUE(profile_manager_is_value_safe("curl.ssh user@host"), "allows curl.ssh (no pipe to shell)");
}

static void test_dangerous_disk_ops(void) {
    EXPECT_FALSE(profile_manager_is_value_safe("mkfs.ext4 /dev/sda"), "blocks mkfs");
    EXPECT_FALSE(profile_manager_is_value_safe("dd if=/dev/zero of=/dev/sda"), "blocks dd if=");
    EXPECT_FALSE(profile_manager_is_value_safe("echo data > /dev/sda"), "blocks > /dev/sd");
}

static void test_dangerous_fork_bomb(void) {
    EXPECT_FALSE(profile_manager_is_value_safe(":(){ :|:& };:"), "blocks fork bomb");
}

static void test_dangerous_case_insensitive(void) {
    EXPECT_FALSE(profile_manager_is_value_safe("RM -RF /"), "blocks RM -RF (uppercase)");
    EXPECT_FALSE(profile_manager_is_value_safe("Rm -Rf /tmp"), "blocks Rm -Rf (mixed case)");
}

static void test_warning_permissions(void) {
    /* chmod 777 / chown root are warnings, not blocks */
    EXPECT_TRUE(profile_manager_is_value_safe("chmod 777 /etc/passwd"), "allows chmod 777 (warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("chown root /etc"), "allows chown root (warning only)");
}

/* ---------- is_value_safe: credential patterns (warning, not block) ---------- */

static void test_credential_uppercase(void) {
    EXPECT_TRUE(profile_manager_is_value_safe("export PASSWORD=secret"), "allows PASSWORD= (warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("echo TOKEN=abc"), "allows TOKEN= (warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("API_KEY=xyz"), "allows API_KEY= (warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("export SECRET=foo"), "allows SECRET= (warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("PRIVATE_KEY=bar"), "allows PRIVATE_KEY= (warning only)");
}

static void test_credential_lowercase(void) {
    EXPECT_TRUE(profile_manager_is_value_safe("export password=secret"), "allows password= (lower, warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("my_token=abc"), "allows token= (lower, warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("api_key=xyz"), "allows api_key= (lower, warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("export secret=foo"), "allows secret= (lower, warning only)");
    EXPECT_TRUE(profile_manager_is_value_safe("private_key=bar"), "allows private_key= (lower, warning only)");
}

/* ---------- is_value_safe: safe values ---------- */

static void test_safe_dev_commands(void) {
    EXPECT_TRUE(profile_manager_is_value_safe("git status"), "allows git status");
    EXPECT_TRUE(profile_manager_is_value_safe("git add ."), "allows git add");
    EXPECT_TRUE(profile_manager_is_value_safe("npm run dev"), "allows npm run dev");
    EXPECT_TRUE(profile_manager_is_value_safe("npm install"), "allows npm install");
    EXPECT_TRUE(profile_manager_is_value_safe("python"), "allows python");
    EXPECT_TRUE(profile_manager_is_value_safe("pip install flask"), "allows pip install");
    EXPECT_TRUE(profile_manager_is_value_safe("docker ps"), "allows docker ps");
    EXPECT_TRUE(profile_manager_is_value_safe("ls -la"), "allows ls -la");
    EXPECT_TRUE(profile_manager_is_value_safe("clear"), "allows clear");
}

static void test_safe_empty_string(void) {
    EXPECT_TRUE(profile_manager_is_value_safe(""), "allows empty string");
}

static void test_safe_null_rejected(void) {
    EXPECT_FALSE(profile_manager_is_value_safe(NULL), "rejects NULL");
}

/* ---------- validate_action ---------- */

static void test_validate_key_actions_always_safe(void) {
    FlipDeckAction key;
    memset(&key, 0, sizeof(key));
    strncpy(key.label, "Next Slide", sizeof(key.label) - 1);
    key.type = FlipDeckActionType_Key;
    strncpy(key.value, "RIGHT", sizeof(key.value) - 1);
    key.confirm = false;
    EXPECT_TRUE(profile_manager_validate_action(&key), "Key action: RIGHT");

    /* Key actions are safe regardless of value content */
    FlipDeckAction tricky;
    memset(&tricky, 0, sizeof(tricky));
    tricky.type = FlipDeckActionType_Key;
    strncpy(tricky.value, "rm -rf", sizeof(tricky.value) - 1);
    EXPECT_TRUE(profile_manager_validate_action(&tricky), "Key type ignores value content");
}

static void test_validate_key_combo_always_safe(void) {
    FlipDeckAction combo;
    memset(&combo, 0, sizeof(combo));
    strncpy(combo.label, "Command Palette", sizeof(combo.label) - 1);
    combo.type = FlipDeckActionType_KeyCombo;
    strncpy(combo.value, "CTRL+SHIFT+P", sizeof(combo.value) - 1);
    combo.confirm = false;
    EXPECT_TRUE(profile_manager_validate_action(&combo), "KeyCombo: CTRL+SHIFT+P");
}

static void test_validate_text_safe(void) {
    FlipDeckAction act;
    memset(&act, 0, sizeof(act));
    strncpy(act.label, "Git Status", sizeof(act.label) - 1);
    act.type = FlipDeckActionType_Text;
    strncpy(act.value, "git status\n", sizeof(act.value) - 1);
    act.confirm = true;
    EXPECT_TRUE(profile_manager_validate_action(&act), "Text action: git status");
}

static void test_validate_text_dangerous(void) {
    FlipDeckAction act;
    memset(&act, 0, sizeof(act));
    act.type = FlipDeckActionType_Text;
    strncpy(act.value, "rm -rf /", sizeof(act.value) - 1);
    EXPECT_FALSE(profile_manager_validate_action(&act), "Text action: rm -rf blocked");

    FlipDeckAction cred;
    memset(&cred, 0, sizeof(cred));
    cred.type = FlipDeckActionType_Text;
    strncpy(cred.value, "export PASSWORD=hunter2", sizeof(cred.value) - 1);
    EXPECT_TRUE(profile_manager_validate_action(&cred), "Text action: PASSWORD allowed (warning only)");
}

static void test_validate_null(void) {
    EXPECT_FALSE(profile_manager_validate_action(NULL), "NULL action rejected");
}

/* ---------- runner ---------- */

int main(void) {
    printf("=== FlipDeck profile_manager host tests ===\n\n");

    printf("[dangerous: rm -rf]\n");
    test_dangerous_rm_rf();

    printf("[warning: sudo]\n");
    test_warning_sudo();

    printf("[dangerous: remote exec]\n");
    test_dangerous_remote_exec();

    printf("[dangerous: disk ops]\n");
    test_dangerous_disk_ops();

    printf("[dangerous: fork bomb]\n");
    test_dangerous_fork_bomb();

    printf("[dangerous: case insensitivity]\n");
    test_dangerous_case_insensitive();

    printf("[warning: permissions]\n");
    test_warning_permissions();

    printf("[credentials: uppercase]\n");
    test_credential_uppercase();

    printf("[credentials: lowercase]\n");
    test_credential_lowercase();

    printf("[safe: dev commands]\n");
    test_safe_dev_commands();

    printf("[safe: edge cases]\n");
    test_safe_empty_string();
    test_safe_null_rejected();

    printf("[validate_action: key types]\n");
    test_validate_key_actions_always_safe();
    test_validate_key_combo_always_safe();

    printf("[validate_action: text type]\n");
    test_validate_text_safe();
    test_validate_text_dangerous();

    printf("[validate_action: null]\n");
    test_validate_null();

    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", g_passes, g_failures);
    return g_failures > 0 ? 1 : 0;
}
