/**
 * @file fd_state.c
 * @brief FDP/1 session rules on the Flipper side (see fd_state.h).
 */
#include "fd_state.h"

#include <stdio.h>
#include <string.h>

#ifndef FD_APP_VERSION
#define FD_APP_VERSION "0.1.0"
#endif

static void fd_outbox_frame(FdOutbox* out, const char* type, const char* const* fields, size_t count) {
    if(!out) return;
    size_t written = fd_proto_encode(out->data + out->len, sizeof(out->data) - out->len, type, fields, count);
    out->len += written;
}

void fd_state_init(FdState* state, uint32_t nonce) {
    memset(state, 0, sizeof(*state));
    snprintf(state->nonce, sizeof(state->nonce), "%08lx", (unsigned long)nonce);
    state->next_req = 1;
    state->mtu = 20;
    state->link = FdLinkOffline;
}

void fd_state_link_up(FdState* state, uint32_t now) {
    state->link = FdLinkConnecting;
    state->last_rx_tick = now;
    state->mac_quit = false;
    state->staging_active = false;
}

void fd_state_link_down(FdState* state) {
    state->link = FdLinkOffline;
    state->staging_active = false;
    state->mtu = 20;
    if(state->request == FdRequestPending) {
        state->request = FdRequestDone;
        state->result_ok = false;
        fd_copy(state->result, sizeof(state->result), "Disconnected");
    }
}

static char fd_first_char(const char* text, char fallback) {
    return (text && text[0]) ? text[0] : fallback;
}

static bool fd_stage_record(FdModel* model, const FdFrame* frame) {
    const char* type = frame->type;
#define F(i) fd_frame_field(frame, (i))
    if(strcmp(type, "SUM") == 0) {
        model->summary.attention = (uint16_t)fd_parse_u32(F(0), 0);
        model->summary.projects = (uint16_t)fd_parse_u32(F(1), 0);
        model->summary.services = (uint16_t)fd_parse_u32(F(2), 0);
        model->summary.agents = (uint16_t)fd_parse_u32(F(3), 0);
    } else if(strcmp(type, "PRJ") == 0) {
        if(frame->count < 9 || model->project_count >= FD_MAX_PROJECTS) return true;
        FdProject* p = &model->projects[model->project_count++];
        fd_copy(p->id, sizeof(p->id), F(0));
        fd_copy(p->name, sizeof(p->name), F(1));
        fd_copy(p->branch, sizeof(p->branch), F(2));
        p->git = fd_first_char(F(3), '-');
        p->ahead = fd_parse_u32(F(4), FD_NO_VALUE);
        p->behind = fd_parse_u32(F(5), FD_NO_VALUE);
        p->port = (uint16_t)fd_parse_u32(F(6), 0);
        p->deploy = fd_first_char(F(7), '-');
        p->attention = F(8)[0] == '1';
    } else if(strcmp(type, "SVC") == 0) {
        if(frame->count < 5 || model->service_count >= FD_MAX_SERVICES) return true;
        FdService* s = &model->services[model->service_count++];
        fd_copy(s->id, sizeof(s->id), F(0));
        fd_copy(s->name, sizeof(s->name), F(1));
        s->port = (uint16_t)fd_parse_u32(F(2), 0);
        fd_copy(s->project, sizeof(s->project), F(3));
        s->started = fd_parse_u32(F(4), FD_NO_VALUE);
    } else if(strcmp(type, "AGT") == 0) {
        if(frame->count < 5 || model->agent_count >= FD_MAX_AGENTS) return true;
        FdAgent* a = &model->agents[model->agent_count++];
        fd_copy(a->id, sizeof(a->id), F(0));
        fd_copy(a->provider, sizeof(a->provider), F(1));
        fd_copy(a->project, sizeof(a->project), F(2));
        a->state = fd_first_char(F(3), 'r');
        a->started = fd_parse_u32(F(4), FD_NO_VALUE);
    } else if(strcmp(type, "ATN") == 0) {
        if(frame->count < 4 || model->attention_count >= FD_MAX_ATTENTION) return true;
        FdAttention* t = &model->attention[model->attention_count++];
        fd_copy(t->id, sizeof(t->id), F(0));
        t->severity = fd_first_char(F(1), 'i');
        fd_copy(t->project, sizeof(t->project), F(2));
        fd_copy(t->title, sizeof(t->title), F(3));
    } else if(strcmp(type, "EVT") == 0) {
        if(frame->count < 5 || model->event_count >= FD_MAX_EVENTS) return true;
        FdEvent* e = &model->events[model->event_count++];
        fd_copy(e->id, sizeof(e->id), F(0));
        e->severity = fd_first_char(F(1), 'i');
        fd_copy(e->time, sizeof(e->time), F(2));
        fd_copy(e->project, sizeof(e->project), F(3));
        fd_copy(e->title, sizeof(e->title), F(4));
    } else if(strcmp(type, "ACT") == 0) {
        if(frame->count < 4 || model->action_count >= FD_MAX_ACTIONS) return true;
        FdAction* a = &model->actions[model->action_count++];
        fd_copy(a->owner, sizeof(a->owner), F(0));
        fd_copy(a->id, sizeof(a->id), F(1));
        fd_copy(a->kind, sizeof(a->kind), F(2));
        a->destructive = strchr(F(3), 'd') != NULL;
        a->confirm = strchr(F(3), 'c') != NULL;
        fd_copy(a->label, sizeof(a->label), F(4));
    } else {
        return false;
    }
#undef F
    return true;
}

static bool fd_is_record(const char* type) {
    return strcmp(type, "SUM") == 0 || strcmp(type, "PRJ") == 0 || strcmp(type, "SVC") == 0 ||
           strcmp(type, "AGT") == 0 || strcmp(type, "ATN") == 0 || strcmp(type, "EVT") == 0 ||
           strcmp(type, "ACT") == 0;
}

static void fd_send_sync(FdState* state, FdOutbox* out) {
    char gen[12];
    snprintf(gen, sizeof(gen), "%lu", (unsigned long)state->committed_gen);
    const char* fields[] = {gen};
    fd_outbox_frame(out, "SYNC", fields, 1);
}

static void fd_abort_staging(FdState* state, FdOutbox* out) {
    if(!state->staging_active) return;
    state->staging_active = false;
    fd_send_sync(state, out);
}

static bool fd_alert_seen_before(FdState* state, const char* id) {
    for(size_t i = 0; i < FD_SEEN_ALERTS; i++) {
        if(state->seen_alerts[i][0] && strcmp(state->seen_alerts[i], id) == 0) return true;
    }
    fd_copy(state->seen_alerts[state->seen_alert_next], FD_ID_LEN, id);
    state->seen_alert_next = (uint8_t)((state->seen_alert_next + 1) % FD_SEEN_ALERTS);
    return false;
}

uint32_t fd_state_apply(FdState* state, const FdFrame* frame, uint32_t now, FdOutbox* out) {
    const char* type = frame->type;
    state->last_rx_tick = now;

    if(strcmp(type, "HELLO") == 0) {
        state->peer_proto = fd_parse_u32(fd_frame_field(frame, 0), 0);
        fd_copy(state->mac_host, sizeof(state->mac_host), fd_frame_field(frame, 2));
        uint32_t mtu = fd_parse_u32(fd_frame_field(frame, 3), 20);
        state->mtu = (uint16_t)(mtu < 20 ? 20 : (mtu > 243 ? 243 : mtu));
        state->mac_time = fd_parse_u32(fd_frame_field(frame, 4), 0);
        state->mac_time_tick = now;
        state->staging_active = false;
        state->mac_quit = false;
        state->link = state->peer_proto == FD_PROTO_VERSION ? FdLinkReady : FdLinkIncompatible;
        if(state->link == FdLinkReady) state->ever_connected = true;

        char proto[4];
        char gen[12];
        snprintf(proto, sizeof(proto), "%d", FD_PROTO_VERSION);
        snprintf(gen, sizeof(gen), "%lu", (unsigned long)state->committed_gen);
        const char* fields[] = {proto, FD_APP_VERSION, gen, state->nonce};
        fd_outbox_frame(out, "HI", fields, 4);
        return FdChangeRedraw;
    }

    // Until a compatible HELLO, nothing else is meaningful.
    if(state->link != FdLinkReady) return FdChangeNone;

    if(strcmp(type, "SNAP") == 0) {
        memset(&state->staging, 0, sizeof(state->staging));
        state->staging_active = true;
        state->staging_gen = fd_parse_u32(fd_frame_field(frame, 0), 0);
        state->staging_expected = fd_parse_u32(fd_frame_field(frame, 1), 0);
        state->staging_received = 0;
        return FdChangeNone;
    }
    if(fd_is_record(type)) {
        if(!state->staging_active) return FdChangeNone; // stray record: ignore
        state->staging_received++;
        fd_stage_record(&state->staging, frame);
        return FdChangeNone;
    }
    if(strcmp(type, "END") == 0) {
        uint32_t gen = fd_parse_u32(fd_frame_field(frame, 0), 0);
        if(state->staging_active && gen == state->staging_gen &&
           state->staging_received == state->staging_expected) {
            memcpy(&state->committed, &state->staging, sizeof(state->committed));
            state->committed_gen = gen;
            state->has_data = true;
            state->staging_active = false;
            return FdChangeRedraw | FdChangeCommitted;
        }
        state->staging_active = true; // so fd_abort_staging sends SYNC
        fd_abort_staging(state, out);
        return FdChangeNone;
    }
    if(strcmp(type, "PING") == 0) {
        state->mac_time = fd_parse_u32(fd_frame_field(frame, 1), state->mac_time);
        state->mac_time_tick = now;
        char gen[12];
        snprintf(gen, sizeof(gen), "%lu", (unsigned long)state->committed_gen);
        const char* fields[] = {gen};
        fd_outbox_frame(out, "PONG", fields, 1);
        return FdChangeRedraw;
    }
    if(strcmp(type, "MAC") == 0) {
        FdMachine* m = &state->machine;
        fd_copy(m->host, sizeof(m->host), fd_frame_field(frame, 0));
        m->cpu = fd_parse_u32(fd_frame_field(frame, 1), FD_NO_VALUE);
        m->mem = fd_parse_u32(fd_frame_field(frame, 2), FD_NO_VALUE);
        m->battery = fd_parse_u32(fd_frame_field(frame, 3), FD_NO_VALUE);
        m->charging = fd_first_char(fd_frame_field(frame, 4), '-');
        m->network = fd_first_char(fd_frame_field(frame, 5), '-');
        m->boot = fd_parse_u32(fd_frame_field(frame, 6), FD_NO_VALUE);
        state->has_machine = true;
        return FdChangeRedraw;
    }
    if(strcmp(type, "ALR") == 0) {
        const char* id = fd_frame_field(frame, 0);
        if(!id[0] || fd_alert_seen_before(state, id)) return FdChangeNone;
        FdAlert* alert = &state->alert;
        fd_copy(alert->id, sizeof(alert->id), id);
        alert->severity = fd_first_char(fd_frame_field(frame, 1), 'i');
        fd_copy(alert->project, sizeof(alert->project), fd_frame_field(frame, 2));
        fd_copy(alert->title, sizeof(alert->title), fd_frame_field(frame, 3));
        fd_copy(alert->message, sizeof(alert->message), fd_frame_field(frame, 4));
        state->alert_visible = true;
        return FdChangeRedraw | FdChangeAlert;
    }
    if(strcmp(type, "RES") == 0) {
        uint32_t req = fd_parse_u32(fd_frame_field(frame, 0), 0);
        if(state->request != FdRequestPending || req != state->request_id) return FdChangeNone;
        state->request = FdRequestDone;
        state->result_ok = fd_frame_field(frame, 1)[0] == '1';
        fd_copy(state->result, sizeof(state->result), fd_frame_field(frame, 2));
        return FdChangeRedraw | FdChangeResult;
    }
    if(strcmp(type, "BYE") == 0) {
        state->mac_quit = true;
        return FdChangeRedraw;
    }
    return FdChangeNone; // unknown types are ignored (forward compatibility)
}

uint32_t fd_state_receive(FdState* state, FdLineReader* reader, const uint8_t* data, size_t len, uint32_t now, FdOutbox* out) {
    uint32_t changes = FdChangeNone;
    for(size_t i = 0; i < len; i++) {
        FdLineEvent event = fd_line_feed(reader, data[i]);
        if(event == FdLineOverflow) {
            state->frames_bad++;
            fd_abort_staging(state, out);
        } else if(event == FdLineReady) {
            FdFrame frame;
            if(fd_proto_parse(reader->buf, reader->len, &frame) == FdParseOk) {
                state->frames_ok++;
                changes |= fd_state_apply(state, &frame, now, out);
            } else {
                state->frames_bad++;
                // A corrupt frame inside a snapshot means the snapshot is incomplete.
                fd_abort_staging(state, out);
            }
            fd_line_consumed(reader);
        }
    }
    return changes;
}

uint32_t fd_state_tick(FdState* state, uint32_t now) {
    if(state->request == FdRequestPending && now - state->request_tick > FD_ACTION_TIMEOUT_MS) {
        state->request = FdRequestDone;
        state->result_ok = false;
        fd_copy(state->result, sizeof(state->result), "No response from Mac");
        return FdChangeRedraw | FdChangeResult;
    }
    return FdChangeNone;
}

bool fd_state_is_stale(const FdState* state, uint32_t now) {
    return state->link == FdLinkReady && now - state->last_rx_tick > FD_STALE_AFTER_MS;
}

uint32_t fd_state_silence_s(const FdState* state, uint32_t now) {
    return (now - state->last_rx_tick) / 1000u;
}

uint32_t fd_state_mac_now(const FdState* state, uint32_t now) {
    if(state->mac_time == 0) return 0;
    return state->mac_time + (now - state->mac_time_tick) / 1000u;
}

size_t fd_state_actions_for(const FdState* state, const char* owner, const FdAction** out, size_t max) {
    size_t count = 0;
    for(size_t i = 0; i < state->committed.action_count && count < max; i++) {
        if(strcmp(state->committed.actions[i].owner, owner) == 0) out[count++] = &state->committed.actions[i];
    }
    return count;
}

bool fd_state_request(FdState* state, const FdAction* action, uint32_t now, FdOutbox* out) {
    if(state->link != FdLinkReady || state->request == FdRequestPending) return false;
    state->request_id = state->next_req++;
    state->request = FdRequestPending;
    state->request_tick = now;
    state->result[0] = '\0';
    char req[12];
    char gen[12];
    snprintf(req, sizeof(req), "%lu", (unsigned long)state->request_id);
    snprintf(gen, sizeof(gen), "%lu", (unsigned long)state->committed_gen);
    const char* fields[] = {req, action->id, gen};
    fd_outbox_frame(out, "REQ", fields, 3);
    return true;
}

void fd_state_seen(FdState* state, const char* id, FdOutbox* out) {
    if(state->alert_visible && strcmp(state->alert.id, id) == 0) state->alert_visible = false;
    if(state->link != FdLinkReady) return;
    const char* fields[] = {id};
    fd_outbox_frame(out, "SEEN", fields, 1);
}

void fd_state_request_sync(FdState* state, FdOutbox* out) {
    if(state->link == FdLinkReady) fd_send_sync(state, out);
}

const char* fd_action_label(const FdAction* action) {
    if(action->label[0]) return action->label;
    if(strcmp(action->kind, "OPEN") == 0) return "Open on Mac";
    if(strcmp(action->kind, "LOCAL") == 0) return "Open localhost";
    if(strcmp(action->kind, "LOGS") == 0) return "Open logs";
    if(strcmp(action->kind, "DEPLOY") == 0) return "Open deployment";
    if(strcmp(action->kind, "TERM") == 0) return "Open Terminal";
    if(strcmp(action->kind, "STOP") == 0) return "Stop server";
    return action->kind;
}

void fd_format_duration(char* out, size_t cap, uint32_t seconds) {
    if(seconds < 60) snprintf(out, cap, "%lus", (unsigned long)seconds);
    else if(seconds < 3600) snprintf(out, cap, "%lum", (unsigned long)(seconds / 60));
    else if(seconds < 86400) snprintf(out, cap, "%luh %lum", (unsigned long)(seconds / 3600), (unsigned long)((seconds % 3600) / 60));
    else snprintf(out, cap, "%lud", (unsigned long)(seconds / 86400));
}

const FdProject* fd_state_project_named(const FdState* state, const char* name) {
    if(!name || !name[0]) return NULL;
    for(size_t i = 0; i < state->committed.project_count; i++) {
        if(strcmp(state->committed.projects[i].name, name) == 0) return &state->committed.projects[i];
    }
    return NULL;
}
