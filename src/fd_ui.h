/**
 * @file fd_ui.h
 * @brief Screens. Every screen is a list of rows built from FdState, so
 * drawing and input handling share one model.
 */
#pragma once

#include "fd_state.h"

#include <gui/canvas.h>
#include <input/input.h>

typedef enum {
    FdScreenHome,
    FdScreenMac,
    FdScreenProjects,
    FdScreenProject,
    FdScreenDeploys,
    FdScreenServices,
    FdScreenService,
    FdScreenAgents,
    FdScreenAgent,
    FdScreenActivity,
    FdScreenEvent,
    FdScreenAttention,
    FdScreenAttentionItem,
} FdScreen;

typedef enum {
    FdRowInfo,    // text only, not selectable
    FdRowNav,     // opens `target` for `target_id`
    FdRowAction,  // requests `action` from the Mac
    FdRowDismiss, // dismisses the alert / attention item `target_id`
    FdRowResync,  // asks the Mac for a fresh snapshot
} FdRowKind;

typedef enum {
    FdMarkNone,
    FdMarkDot,   // live / running
    FdMarkRing,  // idle / unknown
    FdMarkBang,  // needs attention
    FdMarkOk,
    FdMarkFail,
} FdMark;

#define FD_ROW_LEFT 34
#define FD_ROW_RIGHT 16
#define FD_MAX_ROWS 28
#define FD_NAV_DEPTH 6

typedef struct {
    FdRowKind kind;
    FdMark mark;
    bool emphasis;
    char left[FD_ROW_LEFT];
    char right[FD_ROW_RIGHT];
    FdScreen target;
    char target_id[FD_ID_LEN];
    const FdAction* action;
} FdRow;

typedef struct {
    FdScreen screen;
    char id[FD_ID_LEN];
    uint8_t selected; // index into rows (a selectable row, if any)
    uint8_t scroll;   // first visible row
} FdNav;

typedef struct {
    FdNav stack[FD_NAV_DEPTH];
    uint8_t depth;

    // Alert overlay navigation (separate so an alert never loses your place).
    uint8_t alert_selected;
    uint8_t alert_scroll;

    bool confirming;
    FdAction confirm_action;

    bool awaiting_result;
    char toast[FD_RESULT_LEN];
    bool toast_ok;
    uint32_t toast_until;

    // Scratch row buffer (heap-allocated with the app; guarded by its mutex).
    FdRow rows[FD_MAX_ROWS];
    uint8_t row_count;
    char title[FD_ROW_LEFT];
} FdUi;

typedef enum {
    FdUiNone = 0,
    FdUiExit = 1 << 0,
} FdUiResult;

void fd_ui_init(FdUi* ui);

void fd_ui_draw(FdUi* ui, const FdState* state, Canvas* canvas, uint32_t now);

/** Handles a key press. May queue frames in `out`. */
uint32_t fd_ui_input(FdUi* ui, FdState* state, const InputEvent* event, uint32_t now, FdOutbox* out);

/** Call when fd_state reports FdChangeResult. */
void fd_ui_on_result(FdUi* ui, const FdState* state, uint32_t now);

/** Call when fd_state reports FdChangeAlert. */
void fd_ui_on_alert(FdUi* ui);
