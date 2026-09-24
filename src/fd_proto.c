/**
 * @file fd_proto.c
 * @brief FDP/1 framing (see fd_proto.h, docs/protocol.md).
 */
#include "fd_proto.h"

#include <string.h>

uint16_t fd_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for(int bit = 0; bit < 8; bit++) {
            if(crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static int fd_hex_value(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

FdParseResult fd_proto_parse(char* line, size_t len, FdFrame* out) {
    memset(out, 0, sizeof(*out));

    // Drop '\r' in place (protocol says it is ignored).
    size_t write = 0;
    for(size_t read = 0; read < len; read++) {
        if(line[read] != '\r') line[write++] = line[read];
    }
    len = write;
    line[len] = '\0';

    if(len + 1 > FD_FRAME_MAX) return FdParseTooLong;
    for(size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)line[i];
        if(c < 0x20 || c > 0x7E) return FdParseNonPrintable;
    }

    // The CRC delimiter is the last '*', followed by exactly 4 hex digits.
    char* star = strrchr(line, '*');
    if(!star || (size_t)(line + len - star) != 5) return FdParseMalformed;
    uint16_t expected = 0;
    for(int i = 1; i <= 4; i++) {
        int value = fd_hex_value(star[i]);
        if(value < 0) return FdParseMalformed;
        expected = (uint16_t)((expected << 4) | (uint16_t)value);
    }
    size_t payload_len = (size_t)(star - line);
    if(fd_crc16((const uint8_t*)line, payload_len) != expected) return FdParseBadChecksum;
    *star = '\0';

    // Type: 1..7 uppercase ASCII letters.
    char* cursor = line;
    char* bar = strchr(cursor, '|');
    size_t type_len = bar ? (size_t)(bar - cursor) : strlen(cursor);
    if(type_len == 0 || type_len >= FD_TYPE_MAX) return FdParseMalformed;
    for(size_t i = 0; i < type_len; i++) {
        if(cursor[i] < 'A' || cursor[i] > 'Z') return FdParseMalformed;
    }
    memcpy(out->type, cursor, type_len);
    out->type[type_len] = '\0';

    // Fields beyond FD_MAX_FIELDS are ignored (forward compatibility).
    while(bar && out->count < FD_MAX_FIELDS) {
        *bar = '\0';
        cursor = bar + 1;
        out->fields[out->count++] = cursor;
        bar = strchr(cursor, '|');
    }
    if(bar) *bar = '\0';
    return FdParseOk;
}

const char* fd_frame_field(const FdFrame* frame, uint8_t index) {
    return index < frame->count ? frame->fields[index] : "";
}

size_t fd_proto_encode(char* out, size_t cap, const char* type, const char* const* fields, size_t count) {
    size_t limit = cap < FD_FRAME_MAX + 1 ? cap : FD_FRAME_MAX + 1;
    size_t len = 0;

#define FD_APPEND(ptr, n)                          \
    do {                                           \
        if(len + (n) + 1 > limit) return 0;        \
        memcpy(out + len, (ptr), (n));             \
        len += (n);                                \
    } while(0)

    FD_APPEND(type, strlen(type));
    for(size_t i = 0; i < count; i++) {
        FD_APPEND("|", 1);
        const char* field = fields[i] ? fields[i] : "";
        // Fields must not contain separators; replace them defensively.
        size_t field_len = strlen(field);
        if(len + field_len + 1 > limit) return 0;
        for(size_t j = 0; j < field_len; j++) {
            char c = field[j];
            if(c == '|') c = '/';
            else if(c == '*') c = '+';
            else if((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) c = '?';
            out[len++] = c;
        }
    }
    uint16_t crc = fd_crc16((const uint8_t*)out, len);
    static const char hex[] = "0123456789ABCDEF";
    char tail[6] = {'*', hex[(crc >> 12) & 0xF], hex[(crc >> 8) & 0xF], hex[(crc >> 4) & 0xF], hex[crc & 0xF], '\n'};
    FD_APPEND(tail, sizeof(tail));
#undef FD_APPEND
    out[len] = '\0';
    return len;
}

const char* fd_parse_result_name(FdParseResult result) {
    switch(result) {
    case FdParseOk:
        return "ok";
    case FdParseTooLong:
        return "tooLong";
    case FdParseBadChecksum:
        return "badChecksum";
    case FdParseNonPrintable:
        return "nonPrintable";
    case FdParseMalformed:
        return "malformed";
    }
    return "unknown";
}

void fd_line_reset(FdLineReader* reader) {
    reader->len = 0;
    reader->discarding = false;
    reader->buf[0] = '\0';
}

FdLineEvent fd_line_feed(FdLineReader* reader, uint8_t byte) {
    if(byte == '\n') {
        if(reader->discarding) {
            reader->discarding = false;
            reader->len = 0;
            return FdLineNone;
        }
        if(reader->len == 0) return FdLineNone;
        reader->buf[reader->len] = '\0';
        return FdLineReady;
    }
    if(reader->discarding) return FdLineNone;
    reader->buf[reader->len++] = (char)byte;
    if(reader->len >= FD_FRAME_MAX) {
        reader->len = 0;
        reader->discarding = true;
        return FdLineOverflow;
    }
    return FdLineNone;
}

void fd_line_consumed(FdLineReader* reader) {
    reader->len = 0;
    reader->buf[0] = '\0';
}

uint32_t fd_parse_u32(const char* text, uint32_t fallback) {
    if(!text || !*text) return fallback;
    uint32_t value = 0;
    for(const char* p = text; *p; p++) {
        if(*p < '0' || *p > '9') return fallback;
        uint32_t next = value * 10u + (uint32_t)(*p - '0');
        if(next < value) return fallback; // overflow
        value = next;
    }
    return value;
}

void fd_copy(char* dst, size_t cap, const char* src) {
    if(cap == 0) return;
    size_t i = 0;
    if(src) {
        for(; i + 1 < cap && src[i]; i++) dst[i] = src[i];
    }
    dst[i] = '\0';
}
