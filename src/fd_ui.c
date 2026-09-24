/**
 * @file fd_ui.c
 * @brief Screens (see fd_ui.h). 128x64: 11 px header, five 10 px rows.
 */
#include "fd_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

#define FD_UI_LIST_TOP 13
#define FD_UI_ROW_H 10
#define FD_UI_VISIBLE 5
#define FD_UI_WRAP 23
#define FD_UI_TOAST_MS 3000

// ---------------------------------------------------------------------------
// Row building

static FdNav* fd_ui_nav(FdUi* ui) {
    return &ui->stack[ui->depth - 1];
}

static FdRow* fd_ui_add(FdUi* ui, FdRowKind kind, const char* left, const char* right) {
    if(ui->row_count >= FD_MAX_ROWS) return NULL;
    FdRow* row = &ui->rows[ui->row_count++];
    memset(row, 0, sizeof(*row));
    row->kind = kind;
    fd_copy(row->left, sizeof(row->left), left);
    fd_copy(row->right, sizeof(row->right), right);
    return row;
}

static void fd_ui_info(FdUi* ui, const char* left, const char* right, FdMark mark) {
    FdRow* row = fd_ui_add(ui, FdRowInfo, left, right);
    if(row) row->mark = mark;
}

static void fd_ui_link(FdUi* ui, const char* left, const char* right, FdScreen target, const char* id, FdMark mark) {
    FdRow* row = fd_ui_add(ui, FdRowNav, left, right);
    if(!row) return;
    row->target = target;
    row->mark = mark;
    fd_copy(row->target_id, sizeof(row->target_id), id);
}

static void fd_ui_actions(FdUi* ui, const FdState* state, const char* owner) {
    const FdAction* actions[10];
    size_t count = fd_state_actions_for(state, owner, actions, 10);
    for(size_t i = 0; i < count; i++) {
        FdRow* row = fd_ui_add(ui, FdRowAction, fd_action_label(actions[i]), actions[i]->confirm ? "!" : ">");
        if(row) row->action = actions[i];
    }
}

/** Splits text into info rows of about FD_UI_WRAP characters, at spaces. */
static void fd_ui_wrapped(FdUi* ui, const char* text, uint8_t max_rows) {
    char line[FD_UI_WRAP + 1];
    size_t length = strlen(text);
    size_t start = 0;
    for(uint8_t rows = 0; rows < max_rows && start < length; rows++) {
        while(start < length && text[start] == ' ') start++;
        size_t take = length - start;
        if(take > FD_UI_WRAP) {
            take = FD_UI_WRAP;
            size_t space = take;
            while(space > 0 && text[start + space] != ' ') space--;
            if(space > FD_UI_WRAP / 2) take = space;
        }
        memcpy(line, text + start, take);
        line[take] = '\0';
        fd_ui_info(ui, line, "", FdMarkNone);
        start += take;
    }
}

static FdMark fd_ui_severity_mark(char severity) {
    switch(severity) {
    case 's':
        return FdMarkOk;
    case 'e':
        return FdMarkFail;
    case 'w':
    case 'a':
        return FdMarkBang;
    default:
        return FdMarkNone;
    }
}

static const char* fd_ui_deploy_word(char deploy) {
    switch(deploy) {
    case 'r':
        return "Live";
    case 'e':
        return "Failed";
    case 'b':
        return "Building";
    case 'q':
        return "Queued";
    case 'x':
        return "Canceled";
    default:
        return "-";
    }
}

static FdMark fd_ui_deploy_mark(char deploy) {
    switch(deploy) {
    case 'r':
        return FdMarkOk;
    case 'e':
        return FdMarkFail;
    case 'b':
    case 'q':
        return FdMarkRing;
    default:
        return FdMarkNone;
    }
}

static void fd_ui_since(char* out, size_t cap, uint32_t started, const FdState* state, uint32_t now) {
    uint32_t mac_now = fd_state_mac_now(state, now);
    if(started == FD_NO_VALUE || mac_now == 0 || mac_now < started) {
        fd_copy(out, cap, "-");
        return;
    }
    fd_format_duration(out, cap, mac_now - started);
}

static void fd_ui_title(FdUi* ui, const char* title) {
    fd_copy(ui->title, sizeof(ui->title), title);
}

static void fd_ui_gone(FdUi* ui) {
    fd_ui_info(ui, "No longer available.", "", FdMarkNone);
    fd_ui_info(ui, "Press Back.", "", FdMarkNone);
}

static void fd_ui_build_home(FdUi* ui, const FdState* state) {
    const FdModel* model = &state->committed;
    char left[FD_ROW_LEFT];
    char right[FD_ROW_RIGHT];
    fd_ui_title(ui, "FLIPDECK");

    if(state->link == FdLinkIncompatible) {
        fd_ui_info(ui, "Version mismatch", "", FdMarkBang);
        snprintf(left, sizeof(left), "Mac FDP/%lu, this FDP/%d", (unsigned long)state->peer_proto, FD_PROTO_VERSION);
        fd_ui_info(ui, left, "", FdMarkNone);
        fd_ui_info(ui, "Update both apps.", "", FdMarkNone);
        return;
    }
    if(!state->has_data) {
        if(state->link == FdLinkOffline) {
            fd_ui_info(ui, "Waiting for your Mac", "", FdMarkRing);
            fd_ui_info(ui, "Open FlipDeck on the Mac.", "", FdMarkNone);
            fd_ui_info(ui, "First time: enter the PIN", "", FdMarkNone);
            fd_ui_info(ui, "shown here on the Mac.", "", FdMarkNone);
        } else {
            fd_ui_info(ui, "Connected. Syncing...", "", FdMarkDot);
        }
        return;
    }

    if(model->summary.attention > 0) {
        snprintf(left, sizeof(left), "! %u Attention", model->summary.attention);
        fd_ui_link(ui, left, "", FdScreenAttention, "", FdMarkNone);
        if(ui->row_count) ui->rows[ui->row_count - 1].emphasis = true;
    }
    bool live = state->link == FdLinkReady && !state->mac_quit;
    fd_ui_link(ui, "Mac", "", FdScreenMac, "", live ? FdMarkDot : FdMarkRing);
    snprintf(right, sizeof(right), "%u", model->summary.projects);
    fd_ui_link(ui, "Projects", right, FdScreenProjects, "", FdMarkNone);

    uint8_t deploys = 0, failed = 0, building = 0;
    for(uint8_t i = 0; i < model->project_count; i++) {
        char deploy = model->projects[i].deploy;
        if(deploy == '-') continue;
        deploys++;
        if(deploy == 'e') failed++;
        if(deploy == 'b' || deploy == 'q') building++;
    }
    if(deploys > 0) {
        if(failed) snprintf(right, sizeof(right), "%u failed", failed);
        else if(building) snprintf(right, sizeof(right), "%u building", building);
        else fd_copy(right, sizeof(right), "OK");
        fd_ui_link(ui, "Deploys", right, FdScreenDeploys, "", failed ? FdMarkFail : FdMarkNone);
    }
    snprintf(right, sizeof(right), "%u", model->summary.agents);
    fd_ui_link(ui, "Agents", right, FdScreenAgents, "", model->summary.agents ? FdMarkDot : FdMarkNone);
    snprintf(right, sizeof(right), "%u", model->summary.services);
    fd_ui_link(ui, "Services", right, FdScreenServices, "", model->summary.services ? FdMarkDot : FdMarkNone);
    fd_ui_link(ui, "Activity", "", FdScreenActivity, "", FdMarkNone);
}

static void fd_ui_build_mac(FdUi* ui, const FdState* state, uint32_t now) {
    const FdMachine* machine = &state->machine;
    char value[FD_ROW_RIGHT];
    fd_ui_title(ui, state->has_machine ? machine->host : (state->mac_host[0] ? state->mac_host : "MAC"));

    switch(state->link) {
    case FdLinkReady:
        if(state->mac_quit) fd_ui_info(ui, "Link", "App closed", FdMarkRing);
        else if(fd_state_is_stale(state, now)) {
            snprintf(value, sizeof(value), "Stale %lus", (unsigned long)fd_state_silence_s(state, now));
            fd_ui_info(ui, "Link", value, FdMarkBang);
        } else fd_ui_info(ui, "Link", "Connected", FdMarkDot);
        break;
    case FdLinkConnecting:
        fd_ui_info(ui, "Link", "Connecting", FdMarkRing);
        break;
    case FdLinkIncompatible:
        fd_ui_info(ui, "Link", "Incompatible", FdMarkBang);
        break;
    case FdLinkOffline:
        fd_ui_info(ui, "Link", "Offline", FdMarkRing);
        break;
    }
    if(!state->has_machine) {
        fd_ui_info(ui, "No machine data yet.", "", FdMarkNone);
    } else {
        if(machine->cpu != FD_NO_VALUE) snprintf(value, sizeof(value), "%lu%%", (unsigned long)machine->cpu);
        else fd_copy(value, sizeof(value), "-");
        fd_ui_info(ui, "CPU", value, FdMarkNone);
        if(machine->mem != FD_NO_VALUE) snprintf(value, sizeof(value), "%lu%%", (unsigned long)machine->mem);
        else fd_copy(value, sizeof(value), "-");
        fd_ui_info(ui, "Memory", value, FdMarkNone);
        if(machine->battery != FD_NO_VALUE) {
            snprintf(value, sizeof(value), "%lu%%%s", (unsigned long)machine->battery, machine->charging == '1' ? " chg" : "");
            fd_ui_info(ui, "Battery", value, FdMarkNone);
        }
        fd_ui_info(ui, "Network", machine->network == '1' ? "Online" : (machine->network == '0' ? "Offline" : "-"), FdMarkNone);
        fd_ui_since(value, sizeof(value), machine->boot, state, now);
        fd_ui_info(ui, "Uptime", value, FdMarkNone);
    }
    fd_ui_add(ui, FdRowResync, "Resync now", ">");
}

static const FdProject* fd_ui_find_project(const FdState* state, const char* id) {
    for(uint8_t i = 0; i < state->committed.project_count; i++) {
        if(strcmp(state->committed.projects[i].id, id) == 0) return &state->committed.projects[i];
    }
    return NULL;
}

static void fd_ui_build_projects(FdUi* ui, const FdState* state) {
    fd_ui_title(ui, "PROJECTS");
    if(state->committed.project_count == 0) {
        fd_ui_info(ui, "No projects yet.", "", FdMarkNone);
        fd_ui_info(ui, "Add folders on the Mac.", "", FdMarkNone);
        return;
    }
    for(uint8_t i = 0; i < state->committed.project_count; i++) {
        const FdProject* p = &state->committed.projects[i];
        const char* flag = p->attention ? "!" : (p->git == 'd' ? "*" : "");
        fd_ui_link(ui, p->name, flag, FdScreenProject, p->id, p->port ? FdMarkDot : FdMarkRing);
    }
}

static void fd_ui_build_project(FdUi* ui, const FdState* state, const char* id) {
    const FdProject* p = fd_ui_find_project(state, id);
    if(!p) {
        fd_ui_title(ui, "PROJECT");
        fd_ui_gone(ui);
        return;
    }
    char value[FD_ROW_RIGHT];
    fd_ui_title(ui, p->name);

    value[0] = '\0';
    if(p->ahead != FD_NO_VALUE && p->behind != FD_NO_VALUE && (p->ahead || p->behind)) {
        unsigned ahead = p->ahead > 999 ? 999u : (unsigned)p->ahead;
        unsigned behind = p->behind > 999 ? 999u : (unsigned)p->behind;
        snprintf(value, sizeof(value), "+%u -%u", ahead, behind);
    }
    if(p->git != 'n') fd_ui_info(ui, p->branch[0] ? p->branch : "(detached)", value, FdMarkNone);

    switch(p->git) {
    case 'c':
        fd_ui_info(ui, "Git", "Clean", FdMarkOk);
        break;
    case 'd':
        fd_ui_info(ui, "Git", "Changes", FdMarkBang);
        break;
    case 'n':
        fd_ui_info(ui, "Git", "No repo", FdMarkNone);
        break;
    default:
        fd_ui_info(ui, "Git", "?", FdMarkNone);
        break;
    }
    if(p->port) {
        snprintf(value, sizeof(value), ":%u", p->port);
        fd_ui_info(ui, "Dev", value, FdMarkDot);
    } else {
        fd_ui_info(ui, "Dev", "-", FdMarkNone);
    }
    if(p->deploy != '-') fd_ui_info(ui, "Deploy", fd_ui_deploy_word(p->deploy), fd_ui_deploy_mark(p->deploy));
    fd_ui_actions(ui, state, p->id);
}

static void fd_ui_build_deploys(FdUi* ui, const FdState* state) {
    fd_ui_title(ui, "DEPLOYS");
    for(uint8_t i = 0; i < state->committed.project_count; i++) {
        const FdProject* p = &state->committed.projects[i];
        if(p->deploy == '-') continue;
        fd_ui_link(ui, p->name, fd_ui_deploy_word(p->deploy), FdScreenProject, p->id, fd_ui_deploy_mark(p->deploy));
    }
    if(ui->row_count == 0) fd_ui_info(ui, "No linked deployments.", "", FdMarkNone);
}

static void fd_ui_build_services(FdUi* ui, const FdState* state, uint32_t now) {
    fd_ui_title(ui, "SERVICES");
    if(state->committed.service_count == 0) {
        fd_ui_info(ui, "No dev servers running.", "", FdMarkNone);
        return;
    }
    char left[FD_ROW_LEFT];
    char right[FD_ROW_RIGHT];
    for(uint8_t i = 0; i < state->committed.service_count; i++) {
        const FdService* s = &state->committed.services[i];
        snprintf(left, sizeof(left), "%s :%u", s->name, s->port);
        fd_ui_since(right, sizeof(right), s->started, state, now);
        fd_ui_link(ui, left, right, FdScreenService, s->id, FdMarkDot);
    }
}

static void fd_ui_build_service(FdUi* ui, const FdState* state, const char* id, uint32_t now) {
    const FdService* s = NULL;
    for(uint8_t i = 0; i < state->committed.service_count; i++) {
        if(strcmp(state->committed.services[i].id, id) == 0) s = &state->committed.services[i];
    }
    if(!s) {
        fd_ui_title(ui, "SERVICE");
        fd_ui_gone(ui);
        return;
    }
    char value[FD_ROW_RIGHT];
    fd_ui_title(ui, s->name);
    snprintf(value, sizeof(value), "localhost:%u", s->port);
    fd_ui_info(ui, value, "", FdMarkDot);
    if(s->project[0]) fd_ui_info(ui, "Project", s->project, FdMarkNone);
    fd_ui_since(value, sizeof(value), s->started, state, now);
    fd_ui_info(ui, "Running", value, FdMarkNone);
    fd_ui_actions(ui, state, s->id);
}

static const char* fd_ui_agent_state(char state) {
    switch(state) {
    case 'r':
        return "Running";
    case 'w':
        return "Waiting";
    case 'c':
        return "Completed";
    case 'f':
        return "Failed";
    default:
        return "Finished";
    }
}

static void fd_ui_build_agents(FdUi* ui, const FdState* state, uint32_t now) {
    fd_ui_title(ui, "AGENTS");
    if(state->committed.agent_count == 0) {
        fd_ui_info(ui, "No agents running.", "", FdMarkNone);
        return;
    }
    char right[FD_ROW_RIGHT];
    for(uint8_t i = 0; i < state->committed.agent_count; i++) {
        const FdAgent* a = &state->committed.agents[i];
        fd_ui_since(right, sizeof(right), a->started, state, now);
        fd_ui_link(ui, a->project[0] ? a->project : a->provider, right, FdScreenAgent, a->id, a->state == 'r' ? FdMarkDot : FdMarkRing);
    }
}

static void fd_ui_build_agent(FdUi* ui, const FdState* state, const char* id, uint32_t now) {
    const FdAgent* a = NULL;
    for(uint8_t i = 0; i < state->committed.agent_count; i++) {
        if(strcmp(state->committed.agents[i].id, id) == 0) a = &state->committed.agents[i];
    }
    if(!a) {
        fd_ui_title(ui, "AGENT");
        fd_ui_gone(ui);
        return;
    }
    char value[FD_ROW_RIGHT];
    fd_ui_title(ui, a->provider);
    if(a->project[0]) fd_ui_info(ui, "Project", a->project, FdMarkNone);
    fd_ui_info(ui, "State", fd_ui_agent_state(a->state), a->state == 'r' ? FdMarkDot : FdMarkRing);
    fd_ui_since(value, sizeof(value), a->started, state, now);
    fd_ui_info(ui, "Elapsed", value, FdMarkNone);
    fd_ui_actions(ui, state, a->id);
}

static void fd_ui_build_activity(FdUi* ui, const FdState* state) {
    fd_ui_title(ui, "ACTIVITY");
    if(state->committed.event_count == 0) {
        fd_ui_info(ui, "Nothing yet.", "", FdMarkNone);
        return;
    }
    char left[FD_ROW_LEFT];
    for(uint8_t i = 0; i < state->committed.event_count; i++) {
        const FdEvent* e = &state->committed.events[i];
        // "HH:MM title", truncated to fit the row.
        size_t time_len = strlen(e->time);
        memcpy(left, e->time, time_len);
        left[time_len] = ' ';
        fd_copy(left + time_len + 1, sizeof(left) - time_len - 1, e->title);
        fd_ui_link(ui, left, "", FdScreenEvent, e->id, fd_ui_severity_mark(e->severity));
    }
}

static void fd_ui_build_event(FdUi* ui, const FdState* state, const char* id) {
    const FdEvent* e = NULL;
    for(uint8_t i = 0; i < state->committed.event_count; i++) {
        if(strcmp(state->committed.events[i].id, id) == 0) e = &state->committed.events[i];
    }
    if(!e) {
        fd_ui_title(ui, "EVENT");
        fd_ui_gone(ui);
        return;
    }
    fd_ui_title(ui, e->project[0] ? e->project : "EVENT");
    fd_ui_info(ui, e->time, "", fd_ui_severity_mark(e->severity));
    fd_ui_wrapped(ui, e->title, 3);
    fd_ui_actions(ui, state, e->id);
}

static void fd_ui_build_attention(FdUi* ui, const FdState* state) {
    fd_ui_title(ui, "ATTENTION");
    if(state->committed.attention_count == 0) {
        fd_ui_info(ui, "All clear.", "", FdMarkOk);
        return;
    }
    for(uint8_t i = 0; i < state->committed.attention_count; i++) {
        const FdAttention* t = &state->committed.attention[i];
        fd_ui_link(ui, t->title, "", FdScreenAttentionItem, t->id, fd_ui_severity_mark(t->severity));
    }
}

static void fd_ui_build_attention_item(FdUi* ui, const FdState* state, const char* id) {
    const FdAttention* t = NULL;
    for(uint8_t i = 0; i < state->committed.attention_count; i++) {
        if(strcmp(state->committed.attention[i].id, id) == 0) t = &state->committed.attention[i];
    }
    if(!t) {
        fd_ui_title(ui, "ATTENTION");
        fd_ui_gone(ui);
        return;
    }
    fd_ui_title(ui, t->project[0] ? t->project : "ATTENTION");
    fd_ui_wrapped(ui, t->title, 2);
    fd_ui_actions(ui, state, t->id);
    FdRow* dismiss = fd_ui_add(ui, FdRowDismiss, "Dismiss", "");
    if(dismiss) fd_copy(dismiss->target_id, sizeof(dismiss->target_id), t->id);
}

static void fd_ui_build_alert(FdUi* ui, const FdState* state) {
    const FdAlert* alert = &state->alert;
    char title[FD_ROW_LEFT];
    size_t i = 0;
    for(; alert->title[i] && i + 1 < sizeof(title); i++) {
        char c = alert->title[i];
        title[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    title[i] = '\0';
    fd_ui_title(ui, title);
    if(alert->project[0]) fd_ui_info(ui, alert->project, "", fd_ui_severity_mark(alert->severity));
    if(alert->message[0]) fd_ui_wrapped(ui, alert->message, 3);
    fd_ui_actions(ui, state, alert->id);
    FdRow* dismiss = fd_ui_add(ui, FdRowDismiss, "Dismiss", "");
    if(dismiss) fd_copy(dismiss->target_id, sizeof(dismiss->target_id), alert->id);
}

static void fd_ui_build(FdUi* ui, const FdState* state, uint32_t now) {
    ui->row_count = 0;
    ui->title[0] = '\0';
    if(state->alert_visible) {
        fd_ui_build_alert(ui, state);
        return;
    }
    FdNav* nav = fd_ui_nav(ui);
    switch(nav->screen) {
    case FdScreenHome:
        fd_ui_build_home(ui, state);
        break;
    case FdScreenMac:
        fd_ui_build_mac(ui, state, now);
        break;
    case FdScreenProjects:
        fd_ui_build_projects(ui, state);
        break;
    case FdScreenProject:
        fd_ui_build_project(ui, state, nav->id);
        break;
    case FdScreenDeploys:
        fd_ui_build_deploys(ui, state);
        break;
    case FdScreenServices:
        fd_ui_build_services(ui, state, now);
        break;
    case FdScreenService:
        fd_ui_build_service(ui, state, nav->id, now);
        break;
    case FdScreenAgents:
        fd_ui_build_agents(ui, state, now);
        break;
    case FdScreenAgent:
        fd_ui_build_agent(ui, state, nav->id, now);
        break;
    case FdScreenActivity:
        fd_ui_build_activity(ui, state);
        break;
    case FdScreenEvent:
        fd_ui_build_event(ui, state, nav->id);
        break;
    case FdScreenAttention:
        fd_ui_build_attention(ui, state);
        break;
    case FdScreenAttentionItem:
        fd_ui_build_attention_item(ui, state, nav->id);
        break;
    }
}

// ---------------------------------------------------------------------------
// Selection

static bool fd_ui_selectable(const FdRow* row) {
    return row->kind != FdRowInfo;
}

static int fd_ui_first_selectable(const FdUi* ui) {
    for(uint8_t i = 0; i < ui->row_count; i++) {
        if(fd_ui_selectable(&ui->rows[i])) return i;
    }
    return -1;
}

static void fd_ui_cursor(FdUi* ui, const FdState* state, uint8_t** selected, uint8_t** scroll) {
    if(state->alert_visible) {
        *selected = &ui->alert_selected;
        *scroll = &ui->alert_scroll;
    } else {
        *selected = &fd_ui_nav(ui)->selected;
        *scroll = &fd_ui_nav(ui)->scroll;
    }
}

/** Keeps the selection on a selectable row and visible after state changes. */
static void fd_ui_clamp(FdUi* ui, uint8_t* selected, uint8_t* scroll) {
    int first = fd_ui_first_selectable(ui);
    if(first < 0) {
        *selected = 0;
        uint8_t max_scroll = ui->row_count > FD_UI_VISIBLE ? (uint8_t)(ui->row_count - FD_UI_VISIBLE) : 0;
        if(*scroll > max_scroll) *scroll = max_scroll;
        return;
    }
    if(*selected >= ui->row_count || !fd_ui_selectable(&ui->rows[*selected])) *selected = (uint8_t)first;
    if(*selected == first && first < FD_UI_VISIBLE) *scroll = 0; // show the info above
    if(*selected < *scroll) *scroll = *selected;
    if(*selected >= *scroll + FD_UI_VISIBLE) *scroll = (uint8_t)(*selected - FD_UI_VISIBLE + 1);
}

static void fd_ui_move(FdUi* ui, uint8_t* selected, uint8_t* scroll, int direction) {
    if(fd_ui_first_selectable(ui) < 0) {
        // Nothing to select: scroll the text instead.
        if(direction < 0 && *scroll > 0) (*scroll)--;
        if(direction > 0 && *scroll + FD_UI_VISIBLE < ui->row_count) (*scroll)++;
        return;
    }
    int index = *selected;
    for(;;) {
        index += direction;
        if(index < 0) {
            // Wrap to the last selectable row.
            index = ui->row_count - 1;
            while(index > 0 && !fd_ui_selectable(&ui->rows[index])) index--;
            break;
        }
        if(index >= ui->row_count) {
            index = fd_ui_first_selectable(ui);
            break;
        }
        if(fd_ui_selectable(&ui->rows[index])) break;
    }
    *selected = (uint8_t)index;
    fd_ui_clamp(ui, selected, scroll);
}

// ---------------------------------------------------------------------------
// Input

void fd_ui_init(FdUi* ui) {
    memset(ui, 0, sizeof(*ui));
    ui->depth = 1;
    ui->stack[0].screen = FdScreenHome;
}

static void fd_ui_toast(FdUi* ui, const char* text, bool ok, uint32_t now) {
    fd_copy(ui->toast, sizeof(ui->toast), text);
    ui->toast_ok = ok;
    ui->toast_until = now + FD_UI_TOAST_MS;
}

static void fd_ui_send(FdUi* ui, FdState* state, const FdAction* action, uint32_t now, FdOutbox* out) {
    if(state->link != FdLinkReady || state->mac_quit) {
        fd_ui_toast(ui, "Mac not connected", false, now);
        return;
    }
    if(!fd_state_request(state, action, now, out)) {
        fd_ui_toast(ui, "Busy, try again", false, now);
        return;
    }
    ui->awaiting_result = true;
    ui->toast_until = 0;
}

static void fd_ui_activate(FdUi* ui, FdState* state, const FdRow* row, uint32_t now, FdOutbox* out) {
    switch(row->kind) {
    case FdRowNav:
        if(ui->depth < FD_NAV_DEPTH) {
            FdNav* next = &ui->stack[ui->depth++];
            memset(next, 0, sizeof(*next));
            next->screen = row->target;
            fd_copy(next->id, sizeof(next->id), row->target_id);
        }
        break;
    case FdRowAction:
        if(row->action->confirm) {
            ui->confirming = true;
            memcpy(&ui->confirm_action, row->action, sizeof(FdAction));
        } else {
            fd_ui_send(ui, state, row->action, now, out);
        }
        break;
    case FdRowDismiss: {
        bool was_alert = state->alert_visible;
        fd_state_seen(state, row->target_id, out);
        if(was_alert) {
            ui->alert_selected = 0;
            ui->alert_scroll = 0;
        } else if(ui->depth > 1) {
            ui->depth--;
        }
        break;
    }
    case FdRowResync:
        fd_state_request_sync(state, out);
        fd_ui_toast(ui, "Resync requested", true, now);
        break;
    case FdRowInfo:
        break;
    }
}

uint32_t fd_ui_input(FdUi* ui, FdState* state, const InputEvent* event, uint32_t now, FdOutbox* out) {
    if(event->type == InputTypeLong && event->key == InputKeyBack) return FdUiExit;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return FdUiNone;

    if(ui->confirming) {
        if(event->type != InputTypeShort) return FdUiNone;
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            ui->confirming = false;
            fd_ui_send(ui, state, &ui->confirm_action, now, out);
        } else if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            ui->confirming = false;
        }
        return FdUiNone;
    }

    fd_ui_build(ui, state, now);
    uint8_t* selected;
    uint8_t* scroll;
    fd_ui_cursor(ui, state, &selected, &scroll);
    fd_ui_clamp(ui, selected, scroll);

    switch(event->key) {
    case InputKeyUp:
        fd_ui_move(ui, selected, scroll, -1);
        break;
    case InputKeyDown:
        fd_ui_move(ui, selected, scroll, 1);
        break;
    case InputKeyOk:
    case InputKeyRight:
        if(event->type != InputTypeShort) break;
        if(*selected < ui->row_count && fd_ui_selectable(&ui->rows[*selected])) {
            const FdRow* row = &ui->rows[*selected];
            if(event->key == InputKeyRight && row->kind != FdRowNav) break;
            fd_ui_activate(ui, state, row, now, out);
        }
        break;
    case InputKeyBack:
    case InputKeyLeft:
        if(event->type != InputTypeShort) break;
        if(state->alert_visible) {
            fd_state_seen(state, state->alert.id, out);
            ui->alert_selected = 0;
            ui->alert_scroll = 0;
        } else if(ui->depth > 1) {
            ui->depth--;
        } else if(event->key == InputKeyBack) {
            return FdUiExit;
        }
        break;
    default:
        break;
    }
    return FdUiNone;
}

void fd_ui_on_result(FdUi* ui, const FdState* state, uint32_t now) {
    ui->awaiting_result = false;
    fd_ui_toast(ui, state->result[0] ? state->result : (state->result_ok ? "Done" : "Failed"), state->result_ok, now);
}

void fd_ui_on_alert(FdUi* ui) {
    ui->alert_selected = 0;
    ui->alert_scroll = 0;
    ui->confirming = false;
}

// ---------------------------------------------------------------------------
// Drawing

/** Draws `text` shortened with '.' so it fits in `max_width` pixels. */
static void fd_ui_text(Canvas* canvas, int32_t x, int32_t y, const char* text, uint16_t max_width) {
    char buf[FD_ROW_LEFT + 2];
    fd_copy(buf, sizeof(buf), text);
    size_t len = strlen(buf);
    if(canvas_string_width(canvas, buf) > max_width) {
        while(len > 1) {
            buf[--len] = '\0';
            buf[len - 1] = '.';
            if(canvas_string_width(canvas, buf) <= max_width) break;
        }
    }
    canvas_draw_str(canvas, x, y, buf);
}

static void fd_ui_mark(Canvas* canvas, FdMark mark, int32_t cx, int32_t cy) {
    switch(mark) {
    case FdMarkDot:
        canvas_draw_disc(canvas, cx, cy, 2);
        break;
    case FdMarkRing:
        canvas_draw_circle(canvas, cx, cy, 2);
        break;
    case FdMarkBang:
        canvas_draw_line(canvas, cx, cy - 3, cx, cy + 1);
        canvas_draw_dot(canvas, cx, cy + 3);
        break;
    case FdMarkOk:
        canvas_draw_line(canvas, cx - 3, cy, cx - 1, cy + 2);
        canvas_draw_line(canvas, cx - 1, cy + 2, cx + 3, cy - 2);
        break;
    case FdMarkFail:
        canvas_draw_line(canvas, cx - 2, cy - 2, cx + 2, cy + 2);
        canvas_draw_line(canvas, cx - 2, cy + 2, cx + 2, cy - 2);
        break;
    case FdMarkNone:
        break;
    }
}

static void fd_ui_header(Canvas* canvas, const FdUi* ui, const FdState* state, uint32_t now) {
    const char* status = NULL;
    bool live = false;
    switch(state->link) {
    case FdLinkOffline:
        status = "offline";
        break;
    case FdLinkConnecting:
        status = "...";
        break;
    case FdLinkIncompatible:
        status = "update";
        break;
    case FdLinkReady:
        if(state->mac_quit) status = "closed";
        else if(fd_state_is_stale(state, now)) status = "stale";
        else live = true;
        break;
    }
    canvas_set_font(canvas, FontSecondary);
    uint16_t status_width = 8;
    if(status) {
        status_width = (uint16_t)(canvas_string_width(canvas, status) + 4);
        canvas_draw_str_aligned(canvas, 127, 1, AlignRight, AlignTop, status);
    } else if(live) {
        canvas_draw_disc(canvas, 123, 5, 2);
    }
    canvas_set_font(canvas, FontPrimary);
    fd_ui_text(canvas, 1, 9, ui->title, (uint16_t)(126 - status_width));
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

static void fd_ui_draw_confirm(Canvas* canvas, const FdUi* ui) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 1, 9, "Confirm");
    canvas_draw_line(canvas, 0, 11, 127, 11);
    char question[FD_LABEL_LEN + 2];
    snprintf(question, sizeof(question), "%s?", fd_action_label(&ui->confirm_action));
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, question);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, ui->confirm_action.destructive ? "This can't be undone." : "");
    elements_button_left(canvas, "Cancel");
    elements_button_right(canvas, "Confirm");
}

void fd_ui_draw(FdUi* ui, const FdState* state, Canvas* canvas, uint32_t now) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(ui->confirming) {
        fd_ui_draw_confirm(canvas, ui);
        return;
    }

    fd_ui_build(ui, state, now);
    uint8_t* selected;
    uint8_t* scroll;
    fd_ui_cursor(ui, state, &selected, &scroll);
    fd_ui_clamp(ui, selected, scroll);
    bool has_selectable = fd_ui_first_selectable(ui) >= 0;

    fd_ui_header(canvas, ui, state, now);

    for(uint8_t i = *scroll; i < ui->row_count && i < *scroll + FD_UI_VISIBLE; i++) {
        const FdRow* row = &ui->rows[i];
        int32_t top = FD_UI_LIST_TOP + (int32_t)(i - *scroll) * FD_UI_ROW_H;
        int32_t baseline = top + 8;
        bool highlight = has_selectable && i == *selected;
        if(highlight) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, top, 124, FD_UI_ROW_H, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_set_font(canvas, row->emphasis ? FontPrimary : FontSecondary);
        uint16_t right_width = 0;
        canvas_set_font(canvas, FontSecondary);
        if(row->right[0]) {
            right_width = (uint16_t)(canvas_string_width(canvas, row->right) + 3);
            canvas_draw_str_aligned(canvas, row->mark ? 115 : 121, baseline, AlignRight, AlignBottom, row->right);
        }
        uint16_t mark_width = row->mark ? 9 : 0;
        canvas_set_font(canvas, row->emphasis ? FontPrimary : FontSecondary);
        fd_ui_text(canvas, 3, baseline, row->left, (uint16_t)(118 - right_width - mark_width));
        fd_ui_mark(canvas, row->mark, 119, top + 5);
        canvas_set_color(canvas, ColorBlack);
    }

    if(ui->row_count > FD_UI_VISIBLE) {
        elements_scrollbar_pos(canvas, 127, FD_UI_LIST_TOP, 51, has_selectable ? *selected : *scroll, ui->row_count);
    }

    const char* toast = NULL;
    if(ui->awaiting_result) toast = "Working...";
    else if(now < ui->toast_until) toast = ui->toast;
    if(toast) {
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 53, 128, 11);
        canvas_set_color(canvas, ColorWhite);
        fd_ui_text(canvas, 2, 62, toast, 124);
        canvas_set_color(canvas, ColorBlack);
    }
}
