/**
 * @file fd_state.h
 * @brief The Flipper's copy of Mac state, and the FDP/1 session rules that
 * maintain it (staged snapshots, atomic commit, heartbeats, alerts, action
 * requests). No SDK dependencies: time is passed in as a millisecond tick.
 */
#pragma once

#include "fd_proto.h"

// Limits shared with the Mac (FlipperLimits in Swift, docs/protocol.md).
#define FD_MAX_PROJECTS 12
#define FD_MAX_SERVICES 8
#define FD_MAX_AGENTS 6
#define FD_MAX_ATTENTION 6
#define FD_MAX_EVENTS 10
#define FD_MAX_ACTIONS 64
#define FD_SEEN_ALERTS 16

#define FD_ID_LEN (7 + 1)
#define FD_NAME_LEN (20 + 1)
#define FD_BRANCH_LEN (24 + 1)
#define FD_PROVIDER_LEN (12 + 1)
#define FD_TITLE_LEN (40 + 1)
#define FD_EVENT_TITLE_LEN (48 + 1)
#define FD_MESSAGE_LEN (80 + 1)
#define FD_LABEL_LEN (20 + 1)
#define FD_RESULT_LEN (60 + 1)
#define FD_KIND_LEN (7 + 1)

#define FD_STALE_AFTER_MS 15000u
#define FD_ACTION_TIMEOUT_MS 10000u
#define FD_NO_VALUE 0xFFFFFFFFu

typedef struct {
    uint16_t attention;
    uint16_t projects;
    uint16_t services;
    uint16_t agents;
} FdSummary;

typedef struct {
    char id[FD_ID_LEN];
    char name[FD_NAME_LEN];
    char branch[FD_BRANCH_LEN];
    char git;    // c clean, d dirty, n not a repo, - unknown
    char deploy; // - none, q queued, b building, r ready, e error, x canceled
    bool attention;
    uint16_t port;
    uint32_t ahead;  // FD_NO_VALUE if unknown
    uint32_t behind; // FD_NO_VALUE if unknown
} FdProject;

typedef struct {
    char id[FD_ID_LEN];
    char name[FD_NAME_LEN];
    char project[FD_NAME_LEN];
    uint16_t port;
    uint32_t started; // Mac unix time, FD_NO_VALUE if unknown
} FdService;

typedef struct {
    char id[FD_ID_LEN];
    char provider[FD_PROVIDER_LEN];
    char project[FD_NAME_LEN];
    char state; // r running, w waiting, x exited, c completed, f failed
    uint32_t started;
} FdAgent;

typedef struct {
    char id[FD_ID_LEN];
    char severity; // i s w e a
    char project[FD_NAME_LEN];
    char title[FD_TITLE_LEN];
} FdAttention;

typedef struct {
    char id[FD_ID_LEN];
    char severity;
    char time[6];
    char project[FD_NAME_LEN];
    char title[FD_EVENT_TITLE_LEN];
} FdEvent;

typedef struct {
    char owner[FD_ID_LEN];
    char id[FD_ID_LEN];
    char kind[FD_KIND_LEN];
    char label[FD_LABEL_LEN];
    bool destructive;
    bool confirm;
} FdAction;

typedef struct {
    FdSummary summary;
    FdProject projects[FD_MAX_PROJECTS];
    FdService services[FD_MAX_SERVICES];
    FdAgent agents[FD_MAX_AGENTS];
    FdAttention attention[FD_MAX_ATTENTION];
    FdEvent events[FD_MAX_EVENTS];
    FdAction actions[FD_MAX_ACTIONS];
    uint8_t project_count;
    uint8_t service_count;
    uint8_t agent_count;
    uint8_t attention_count;
    uint8_t event_count;
    uint8_t action_count;
} FdModel;

typedef struct {
    char host[FD_NAME_LEN];
    uint32_t cpu;     // percent or FD_NO_VALUE
    uint32_t mem;     // percent or FD_NO_VALUE
    uint32_t battery; // percent or FD_NO_VALUE
    char charging;    // '1', '0' or '-'
    char network;     // '1', '0' or '-'
    uint32_t boot;    // Mac unix time or FD_NO_VALUE
} FdMachine;

typedef struct {
    char id[FD_ID_LEN];
    char severity;
    char project[FD_NAME_LEN];
    char title[FD_TITLE_LEN];
    char message[FD_MESSAGE_LEN];
} FdAlert;

typedef enum {
    /** No transport link. */
    FdLinkOffline,
    /** Transport up, waiting for HELLO. */
    FdLinkConnecting,
    /** HELLO received and compatible. */
    FdLinkReady,
    /** Mac speaks another protocol version. */
    FdLinkIncompatible,
} FdLink;

typedef enum {
    FdRequestIdle,
    FdRequestPending,
    FdRequestDone,
} FdRequestState;

typedef struct {
    FdModel committed;
    FdModel staging;
    bool has_data;
    bool staging_active;
    uint32_t staging_gen;
    uint32_t staging_expected;
    uint32_t staging_received;
    uint32_t committed_gen;

    FdMachine machine;
    bool has_machine;

    FdLink link;
    uint32_t peer_proto;
    char mac_host[FD_NAME_LEN];
    uint16_t mtu;
    uint32_t mac_time;      // Mac unix time from the last HELLO/PING
    uint32_t mac_time_tick; // local tick when mac_time was received
    uint32_t last_rx_tick;
    bool ever_connected;
    bool mac_quit;

    FdAlert alert;
    bool alert_visible;
    char seen_alerts[FD_SEEN_ALERTS][FD_ID_LEN];
    uint8_t seen_alert_next;

    char nonce[9];
    uint32_t next_req;
    FdRequestState request;
    uint32_t request_id;
    uint32_t request_tick;
    bool result_ok;
    char result[FD_RESULT_LEN];

    uint32_t frames_ok;
    uint32_t frames_bad;
} FdState;

/** Output buffer for frames the Flipper must send in response. */
typedef struct {
    char data[FD_FRAME_MAX * 2];
    size_t len;
} FdOutbox;

/** What changed as a result of a frame (bitmask). */
typedef enum {
    FdChangeNone = 0,
    FdChangeRedraw = 1 << 0,
    FdChangeAlert = 1 << 1,   // a new alert arrived: vibrate
    FdChangeResult = 1 << 2,  // an action result arrived
    FdChangeCommitted = 1 << 3,
} FdChange;

void fd_state_init(FdState* state, uint32_t nonce);

/** Transport link came up / went down. */
void fd_state_link_up(FdState* state, uint32_t now);
void fd_state_link_down(FdState* state);

/** Feeds raw received bytes; replies are appended to `out`. Returns FdChange bits. */
uint32_t fd_state_receive(FdState* state, FdLineReader* reader, const uint8_t* data, size_t len, uint32_t now, FdOutbox* out);

/** Applies one decoded frame. Returns FdChange bits. */
uint32_t fd_state_apply(FdState* state, const FdFrame* frame, uint32_t now, FdOutbox* out);

/** Periodic housekeeping (request timeouts). Returns FdChange bits. */
uint32_t fd_state_tick(FdState* state, uint32_t now);

bool fd_state_is_stale(const FdState* state, uint32_t now);
/** Seconds since the last valid frame. */
uint32_t fd_state_silence_s(const FdState* state, uint32_t now);
/** Best estimate of the Mac's current unix time, or 0 if unknown. */
uint32_t fd_state_mac_now(const FdState* state, uint32_t now);

/** Collects the committed actions owned by `owner`. Returns the count. */
size_t fd_state_actions_for(const FdState* state, const char* owner, const FdAction** out, size_t max);

/** Queues a REQ for `action`. Fails if a request is already pending or the link isn't ready. */
bool fd_state_request(FdState* state, const FdAction* action, uint32_t now, FdOutbox* out);

/** Dismisses the current alert (or an attention item) and queues SEEN. */
void fd_state_seen(FdState* state, const char* id, FdOutbox* out);

/** Asks the Mac for a fresh snapshot. */
void fd_state_request_sync(FdState* state, FdOutbox* out);

/** Built-in label for an action kind (FDP/1 `ACT.kind`). */
const char* fd_action_label(const FdAction* action);

/** Formats a duration like "38m", "2h 5m", "3d". */
void fd_format_duration(char* out, size_t cap, uint32_t seconds);

/** Finds a project by name (for linking services/agents), or NULL. */
const FdProject* fd_state_project_named(const FdState* state, const char* name);
