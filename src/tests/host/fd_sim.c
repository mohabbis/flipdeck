/* fd_sim: the Flipper's real protocol/state code (fd_state + fd_proto) as a
 * stdin/stdout process, so the Mac's Swift session can be tested against it
 * end to end (mac/Tests/FlipDeckCoreTests/InteropTests.swift).
 *
 * Reads Mac->Flipper bytes on stdin, writes Flipper->Mac bytes to stdout.
 * Scripted behaviour: after the first committed snapshot it requests the
 * first action of kind $FD_SIM_ACTION (default OPEN); when an alert arrives
 * it dismisses it (SEEN). Progress is reported on stderr as KEY=value lines.
 */
#include "fd_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t g_now = 0;

static void flush(FdOutbox* out) {
    if(out->len == 0) return;
    fwrite(out->data, 1, out->len, stdout);
    fflush(stdout);
    out->len = 0;
}

int main(void) {
    const char* kind = getenv("FD_SIM_ACTION");
    if(!kind) kind = "OPEN";
    FdState* state = malloc(sizeof(FdState));
    fd_state_init(state, 0x51u);
    FdLineReader reader;
    fd_line_reset(&reader);
    FdOutbox out = {.len = 0};
    bool requested = false;

    fd_state_link_up(state, g_now);
    fprintf(stderr, "READY=1\n");

    uint8_t buffer[64];
    ssize_t n;
    while((n = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        g_now += 10;
        uint32_t changes = fd_state_receive(state, &reader, buffer, (size_t)n, g_now, &out);
        if(changes & FdChangeCommitted) {
            fprintf(stderr, "COMMITTED=%lu PROJECTS=%u ACTIONS=%u FIRST=%s\n",
                (unsigned long)state->committed_gen, state->committed.project_count,
                state->committed.action_count,
                state->committed.project_count ? state->committed.projects[0].name : "");
            if(!requested) {
                for(size_t i = 0; i < state->committed.action_count; i++) {
                    if(strcmp(state->committed.actions[i].kind, kind) == 0) {
                        requested = fd_state_request(state, &state->committed.actions[i], g_now, &out);
                        fprintf(stderr, "REQUESTED=%s\n", state->committed.actions[i].id);
                        break;
                    }
                }
            }
        }
        if(changes & FdChangeAlert) {
            fprintf(stderr, "ALERT=%s|%s\n", state->alert.id, state->alert.title);
            fd_state_seen(state, state->alert.id, &out);
        }
        if(changes & FdChangeResult) {
            fprintf(stderr, "RESULT=%d|%s\n", state->result_ok ? 1 : 0, state->result);
        }
        flush(&out);
        fflush(stderr);
    }
    fprintf(stderr, "LINK=%d FRAMES_OK=%lu FRAMES_BAD=%lu\n", (int)state->link,
        (unsigned long)state->frames_ok, (unsigned long)state->frames_bad);
    free(state);
    return 0;
}
