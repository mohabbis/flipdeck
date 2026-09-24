/**
 * @file fd_proto.h
 * @brief FDP/1 framing: `TYPE|f1|...|fN*CCCC\n` (see docs/protocol.md).
 *
 * No SDK dependencies, so it is unit-tested on the host (src/tests/host).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FD_PROTO_VERSION 1
/** Maximum frame length in bytes, including the trailing '\n'. */
#define FD_FRAME_MAX 240
#define FD_MAX_FIELDS 12
#define FD_TYPE_MAX 8

typedef struct {
    char type[FD_TYPE_MAX];
    /** Point into the parsed line buffer, which must outlive the frame. */
    const char* fields[FD_MAX_FIELDS];
    uint8_t count;
} FdFrame;

typedef enum {
    FdParseOk,
    FdParseTooLong,
    FdParseBadChecksum,
    FdParseNonPrintable,
    FdParseMalformed,
} FdParseResult;

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, xorout 0). */
uint16_t fd_crc16(const uint8_t* data, size_t len);

/**
 * Parses one line (without '\n') in place: '|' separators are replaced by
 * NULs and `out->fields` point into `line`. `line` must have room for a
 * terminating NUL at `line[len]`.
 */
FdParseResult fd_proto_parse(char* line, size_t len, FdFrame* out);

/** Field accessor that returns "" for missing fields. */
const char* fd_frame_field(const FdFrame* frame, uint8_t index);

/**
 * Encodes a frame into `out` (including '\n', NUL-terminated).
 * @return frame length in bytes excluding the NUL, or 0 if it doesn't fit
 *         in `cap` or FD_FRAME_MAX.
 */
size_t fd_proto_encode(char* out, size_t cap, const char* type, const char* const* fields, size_t count);

const char* fd_parse_result_name(FdParseResult result);

/** Reassembles lines from an arbitrarily chunked byte stream. */
typedef struct {
    char buf[FD_FRAME_MAX + 1];
    size_t len;
    /** After an overlong line, drop bytes until the next '\n'. */
    bool discarding;
} FdLineReader;

typedef enum {
    FdLineNone,
    /** A complete line is in `buf` (NUL-terminated, `len` bytes). */
    FdLineReady,
    /** The current line exceeded FD_FRAME_MAX and is being discarded. */
    FdLineOverflow,
} FdLineEvent;

void fd_line_reset(FdLineReader* reader);
FdLineEvent fd_line_feed(FdLineReader* reader, uint8_t byte);
/** Call after handling an FdLineReady line. */
void fd_line_consumed(FdLineReader* reader);

/** Parses an unsigned decimal field; returns `fallback` for "-", "" or junk. */
uint32_t fd_parse_u32(const char* text, uint32_t fallback);

/** Bounded copy that always NUL-terminates. */
void fd_copy(char* dst, size_t cap, const char* src);
