/* Host tests for fd_state: the Flipper side of the FDP/1 session rules. */
#include "test_util.h"

#include "fd_state.h"

#include <stdlib.h>

static FdState* g_state;
static FdLineReader g_reader;
static FdOutbox g_out;

static void reset(void) {
    if(!g_state) g_state = malloc(sizeof(FdState));
    fd_state_init(g_state, 0xC0FFEEu);
    fd_line_reset(&g_reader);
    g_out.len = 0;
    g_out.data[0] = '\0';
}

/* Encodes a frame from a type + '|'-separated field list and feeds it in. */
static uint32_t feed(const char* type, const char* fields_text, uint32_t now) {
    char copy[256];
    const char* fields[FD_MAX_FIELDS];
    size_t count = 0;
    if(fields_text) {
        strcpy(copy, fields_text);
        char* cursor = copy;
        fields[count++] = cursor;
        for(char* p = copy; *p; p++) {
            if(*p == '|') {
                *p = '\0';
                fields[count++] = p + 1;
            }
        }
    }
    char frame[FD_FRAME_MAX + 1];
    size_t len = fd_proto_encode(frame, sizeof(frame), type, fields, count);
    return fd_state_receive(g_state, &g_reader, (const uint8_t*)frame, len, now, &g_out);
}

static bool sent(const char* prefix) {
    return strstr(g_out.data, prefix) != NULL;
}

static void clear_out(void) {
    g_out.len = 0;
    g_out.data[0] = '\0';
}

static void handshake(void) {
    fd_state_link_up(g_state, 0);
    feed("HELLO", "1|0a1b2c3d|Studio|182|1790000000", 0);
}

static void snapshot(uint32_t gen) {
    char text[32];
    snprintf(text, sizeof(text), "%u|5", gen);
    feed("SNAP", text, 10);
    feed("SUM", "1|4|1|0", 10);
    feed("PRJ", "p1|flipdeck|main|c|0|0|3000|r|1", 10);
    feed("SVC", "s1|Vite|5173|flipdeck|1789997720", 10);
    feed("ACT", "p1|a1|OPEN||", 10);
    feed("ACT", "s1|a2|STOP|dc|", 10);
    snprintf(text, sizeof(text), "%u", gen);
    feed("END", text, 10);
}

static void test_handshake(void) {
    reset();
    CHECK_INT(g_state->link, FdLinkOffline, "starts offline");
    fd_state_link_up(g_state, 0);
    CHECK_INT(g_state->link, FdLinkConnecting, "link up waits for HELLO");
    feed("PING", "1|2", 0);
    CHECK_INT(g_out.len, 0, "nothing before HELLO");

    feed("HELLO", "1|0a1b2c3d|Studio|182|1790000000", 100);
    CHECK_INT(g_state->link, FdLinkReady, "ready after HELLO");
    CHECK_STR(g_state->mac_host, "Studio", "host recorded");
    CHECK_INT(g_state->mtu, 182, "mtu from HELLO");
    CHECK(sent("HI|1|0.1.0|0|00c0ffee*"), "replies HI with proto, version, gen, nonce");
    CHECK_INT(fd_state_mac_now(g_state, 5100), 1790000005, "estimates Mac time from local ticks");
}

static void test_incompatible(void) {
    reset();
    fd_state_link_up(g_state, 0);
    feed("HELLO", "2|x|Studio|182|1", 0);
    CHECK_INT(g_state->link, FdLinkIncompatible, "different protocol is incompatible");
    CHECK(sent("HI|1|"), "still answers with its own version");
    clear_out();
    snapshot(1);
    CHECK(!g_state->has_data, "ignores snapshots while incompatible");
}

static void test_snapshot_commit(void) {
    reset();
    handshake();
    snapshot(7);
    CHECK(g_state->has_data, "snapshot committed");
    CHECK_INT(g_state->committed_gen, 7, "generation recorded");
    CHECK_INT(g_state->committed.project_count, 1, "one project");
    const FdProject* p = &g_state->committed.projects[0];
    CHECK_STR(p->name, "flipdeck", "project name");
    CHECK_INT(p->git, 'c', "git clean");
    CHECK_INT(p->port, 3000, "dev port");
    CHECK_INT(p->deploy, 'r', "deploy ready");
    CHECK(p->attention, "attention flag");
    CHECK_INT(g_state->committed.summary.attention, 1, "summary");
    const FdAction* actions[4];
    CHECK_INT(fd_state_actions_for(g_state, "s1", actions, 4), 1, "service action found by owner");
    CHECK(actions[0]->destructive && actions[0]->confirm, "flags parsed");
    CHECK_STR(fd_action_label(actions[0]), "Stop server", "built-in label for STOP");
}

static void test_bad_snapshot_keeps_previous_and_requests_sync(void) {
    reset();
    handshake();
    snapshot(1);
    clear_out();

    // Count mismatch: SNAP says 3 records, only 1 arrives.
    feed("SNAP", "2|3", 20);
    feed("PRJ", "p9|other|main|d|0|0|0|-|0", 20);
    feed("END", "2", 20);
    CHECK_INT(g_state->committed_gen, 1, "incomplete snapshot not committed");
    CHECK_STR(g_state->committed.projects[0].name, "flipdeck", "previous data kept");
    CHECK(sent("SYNC|1*"), "asks for a resync");

    // Generation mismatch.
    clear_out();
    feed("SNAP", "3|0", 30);
    feed("END", "4", 30);
    CHECK_INT(g_state->committed_gen, 1, "mismatched END not committed");
    CHECK(sent("SYNC|1*"), "resync after generation mismatch");

    // A corrupt frame mid-snapshot aborts it.
    clear_out();
    feed("SNAP", "5|1", 40);
    const char* corrupt = "PRJ|p1|x|main|c|0|0|0|-|0*0000\n";
    fd_state_receive(g_state, &g_reader, (const uint8_t*)corrupt, strlen(corrupt), 40, &g_out);
    feed("END", "5", 40);
    CHECK_INT(g_state->committed_gen, 1, "snapshot with a corrupt record not committed");
    CHECK_INT(g_state->frames_bad, 1, "corrupt frame counted");
    CHECK(sent("SYNC|1*"), "resync after corruption");

    // Records outside a snapshot are ignored.
    feed("PRJ", "p8|stray|main|c|0|0|0|-|0", 50);
    CHECK_INT(g_state->committed.project_count, 1, "stray record ignored");
}

static void test_chunked_delivery(void) {
    reset();
    fd_state_link_up(g_state, 0);
    const char* hello_fields[] = {"1", "0a1b2c3d", "Studio", "20", "1"};
    const char* snap_fields[] = {"1", "1"};
    const char* sum_fields[] = {"0", "0", "0", "0"};
    const char* end_fields[] = {"1"};
    char stream[1024];
    size_t len = 0;
    len += fd_proto_encode(stream + len, sizeof(stream) - len, "HELLO", hello_fields, 5);
    len += fd_proto_encode(stream + len, sizeof(stream) - len, "SNAP", snap_fields, 2);
    len += fd_proto_encode(stream + len, sizeof(stream) - len, "SUM", sum_fields, 4);
    len += fd_proto_encode(stream + len, sizeof(stream) - len, "END", end_fields, 1);
    for(size_t i = 0; i < len; i += 3) {
        size_t n = len - i < 3 ? len - i : 3;
        fd_state_receive(g_state, &g_reader, (const uint8_t*)stream + i, n, 0, &g_out);
    }
    CHECK(g_state->has_data, "3-byte chunks reassemble into a committed snapshot");
    CHECK_INT(g_state->mtu, 20, "mtu clamped to at least 20");
}

static void test_ping_pong_and_staleness(void) {
    reset();
    handshake();
    snapshot(3);
    clear_out();
    feed("PING", "3|1790000100", 1000);
    CHECK(sent("PONG|3*"), "PONG reports committed generation");
    CHECK(!fd_state_is_stale(g_state, 1000 + FD_STALE_AFTER_MS), "not stale at the limit");
    CHECK(fd_state_is_stale(g_state, 1001 + FD_STALE_AFTER_MS), "stale after 15s of silence");
    CHECK_INT(fd_state_silence_s(g_state, 21000), 20, "silence in seconds");
    CHECK_INT(fd_state_mac_now(g_state, 3000), 1790000102, "Mac time updated by PING");
}

static void test_alerts_dedupe(void) {
    reset();
    handshake();
    uint32_t change = feed("ALR", "t1|e|flipdeck|Production deploy failed|Type error", 5);
    CHECK(change & FdChangeAlert, "alert triggers vibration");
    CHECK(g_state->alert_visible, "alert visible");
    CHECK_STR(g_state->alert.message, "Type error", "alert message");
    change = feed("ALR", "t1|e|flipdeck|Production deploy failed|Type error", 6);
    CHECK(!(change & FdChangeAlert), "replayed alert is not shown twice");

    clear_out();
    fd_state_seen(g_state, "t1", &g_out);
    CHECK(!g_state->alert_visible, "dismissed");
    CHECK(sent("SEEN|t1*"), "SEEN sent");
}

static void test_action_request_lifecycle(void) {
    reset();
    handshake();
    snapshot(2);
    const FdAction* actions[4];
    fd_state_actions_for(g_state, "p1", actions, 4);
    clear_out();
    CHECK(fd_state_request(g_state, actions[0], 100, &g_out), "request queued");
    CHECK(sent("REQ|1|a1|2*"), "REQ carries id, action and generation");
    CHECK(!fd_state_request(g_state, actions[0], 101, &g_out), "only one request at a time");

    feed("RES", "99|1|other", 200);
    CHECK_INT(g_state->request, FdRequestPending, "result for another request ignored");
    uint32_t change = feed("RES", "1|1|Opened in Cursor", 300);
    CHECK(change & FdChangeResult, "result reported");
    CHECK(g_state->result_ok, "result ok");
    CHECK_STR(g_state->result, "Opened in Cursor", "result message");

    // Timeout.
    clear_out();
    fd_state_request(g_state, actions[0], 1000, &g_out);
    CHECK(sent("REQ|2|"), "request ids increase");
    CHECK_INT(fd_state_tick(g_state, 1000 + FD_ACTION_TIMEOUT_MS), FdChangeNone, "not timed out yet");
    CHECK(fd_state_tick(g_state, 1001 + FD_ACTION_TIMEOUT_MS) & FdChangeResult, "times out");
    CHECK(!g_state->result_ok, "timeout is a failure");
    CHECK_STR(g_state->result, "No response from Mac", "timeout message");

    // Disconnect fails a pending request.
    fd_state_request(g_state, actions[0], 2000, &g_out);
    fd_state_link_down(g_state);
    CHECK_STR(g_state->result, "Disconnected", "disconnect fails the pending request");
    CHECK(!fd_state_request(g_state, actions[0], 2001, &g_out), "no requests while offline");
    CHECK(g_state->has_data, "data kept after disconnect (shown dimmed)");
}

static void test_limits_are_enforced(void) {
    reset();
    handshake();
    feed("SNAP", "9|80", 0);
    for(int i = 0; i < 80; i++) {
        char text[64];
        snprintf(text, sizeof(text), "p%d|a%d|OPEN||", i, i);
        feed("ACT", text, 0);
    }
    feed("END", "9", 0);
    CHECK(g_state->has_data, "oversized snapshot still commits");
    CHECK_INT(g_state->committed.action_count, FD_MAX_ACTIONS, "actions capped at the limit");

    // Overlong text fields are truncated, never overflow.
    feed("SNAP", "10|1", 0);
    feed("PRJ", "p1234567890|a-project-name-that-is-way-too-long-for-the-screen|b|c|0|0|0|-|0", 0);
    feed("END", "10", 0);
    CHECK_INT(strlen(g_state->committed.projects[0].id), FD_ID_LEN - 1, "id truncated");
    CHECK_INT(strlen(g_state->committed.projects[0].name), FD_NAME_LEN - 1, "name truncated");
}

static void test_duration_format(void) {
    char buf[16];
    fd_format_duration(buf, sizeof(buf), 42);
    CHECK_STR(buf, "42s", "seconds");
    fd_format_duration(buf, sizeof(buf), 38 * 60);
    CHECK_STR(buf, "38m", "minutes");
    fd_format_duration(buf, sizeof(buf), 2 * 3600 + 5 * 60);
    CHECK_STR(buf, "2h 5m", "hours");
    fd_format_duration(buf, sizeof(buf), 3 * 86400 + 5);
    CHECK_STR(buf, "3d", "days");
}

int main(void) {
    test_handshake();
    test_incompatible();
    test_snapshot_commit();
    test_bad_snapshot_keeps_previous_and_requests_sync();
    test_chunked_delivery();
    test_ping_pong_and_staleness();
    test_alerts_dedupe();
    test_action_request_lifecycle();
    test_limits_are_enforced();
    test_duration_format();
    free(g_state);
    return test_summary("fd_state");
}
