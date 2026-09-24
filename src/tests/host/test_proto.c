/* Host tests for fd_proto: CRC, parse/encode, line reassembly, golden vectors
 * shared with the Swift implementation (docs/protocol-vectors.txt). */
#include "test_util.h"

#include "fd_proto.h"

#include <stdlib.h>

static void test_crc(void) {
    CHECK_INT(fd_crc16((const uint8_t*)"123456789", 9), 0x29B1, "CRC-16/CCITT-FALSE check value");
    CHECK_INT(fd_crc16((const uint8_t*)"", 0), 0xFFFF, "CRC of empty input is the init value");
}

static void test_round_trip(void) {
    const char* fields[] = {"p1", "flipdeck", "main", "c", "0", "0", "3000", "r", "0"};
    char buf[FD_FRAME_MAX + 1];
    size_t len = fd_proto_encode(buf, sizeof(buf), "PRJ", fields, 9);
    CHECK(len > 0 && buf[len - 1] == '\n', "encodes with trailing newline");

    buf[len - 1] = '\0';
    FdFrame frame;
    CHECK_INT(fd_proto_parse(buf, len - 1, &frame), FdParseOk, "parses what it encoded");
    CHECK_STR(frame.type, "PRJ", "type");
    CHECK_INT(frame.count, 9, "field count");
    CHECK_STR(fd_frame_field(&frame, 1), "flipdeck", "field 1");
    CHECK_STR(fd_frame_field(&frame, 8), "0", "last field");
    CHECK_STR(fd_frame_field(&frame, 20), "", "missing field reads as empty");
}

static void test_empty_fields_and_sanitizing(void) {
    const char* fields[] = {"a|b*c", "", NULL};
    char buf[FD_FRAME_MAX + 1];
    size_t len = fd_proto_encode(buf, sizeof(buf), "ACT", fields, 3);
    buf[len - 1] = '\0';
    FdFrame frame;
    CHECK_INT(fd_proto_parse(buf, len - 1, &frame), FdParseOk, "parses frame with empty fields");
    CHECK_INT(frame.count, 3, "keeps empty fields");
    CHECK_STR(fd_frame_field(&frame, 0), "a/b+c", "separators sanitized on encode");
}

static void test_encode_limits(void) {
    char big[300];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    const char* fields[] = {big};
    char buf[FD_FRAME_MAX + 1];
    CHECK_INT(fd_proto_encode(buf, sizeof(buf), "EVT", fields, 1), 0, "refuses frames over the limit");
    char tiny[8];
    const char* small[] = {"1"};
    CHECK_INT(fd_proto_encode(tiny, sizeof(tiny), "PONG", small, 1), 0, "refuses when the buffer is too small");
}

static void test_line_reader(void) {
    FdLineReader reader;
    fd_line_reset(&reader);
    const char* stream = "AB\r\n\nCD\n";
    int ready = 0;
    char lines[2][8] = {{0}};
    for(const char* p = stream; *p; p++) {
        if(fd_line_feed(&reader, (uint8_t)*p) == FdLineReady) {
            strcpy(lines[ready++], reader.buf);
            fd_line_consumed(&reader);
        }
    }
    CHECK_INT(ready, 2, "two lines, blank line skipped");
    CHECK_STR(lines[0], "AB\r", "line keeps \\r for the parser to drop");
    CHECK_STR(lines[1], "CD", "second line");

    // Overlong line: overflow reported once, then resync at the next newline.
    fd_line_reset(&reader);
    int overflows = 0;
    for(int i = 0; i < 400; i++) {
        if(fd_line_feed(&reader, 'x') == FdLineOverflow) overflows++;
    }
    CHECK_INT(overflows, 1, "overflow reported once");
    CHECK_INT(fd_line_feed(&reader, '\n'), FdLineNone, "discarded line produces nothing");
    fd_line_feed(&reader, 'O');
    fd_line_feed(&reader, 'K');
    CHECK_INT(fd_line_feed(&reader, '\n'), FdLineReady, "resynchronized");
    CHECK_STR(reader.buf, "OK", "next line intact");
}

static void test_parse_u32(void) {
    CHECK_INT(fd_parse_u32("3000", 0), 3000, "decimal");
    CHECK_INT(fd_parse_u32("-", 7), 7, "dash is fallback");
    CHECK_INT(fd_parse_u32("", 7), 7, "empty is fallback");
    CHECK_INT(fd_parse_u32("12a", 7), 7, "junk is fallback");
    CHECK_INT(fd_parse_u32("99999999999", 7), 7, "overflow is fallback");
}

static void test_golden_vectors(const char* path) {
    FILE* file = fopen(path, "r");
    CHECK(file != NULL, "golden vector file opens");
    if(!file) return;
    char line[512];
    int checked = 0;
    while(fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);
        if(len && line[len - 1] == '\n') line[--len] = '\0';
        if(len == 0 || line[0] == '#') continue;

        if(strncmp(line, "OK ", 3) == 0) {
            char original[512];
            strcpy(original, line + 3);
            char work[512];
            strcpy(work, original);
            FdFrame frame;
            FdParseResult result = fd_proto_parse(work, strlen(work), &frame);
            if(result != FdParseOk) printf("    vector: %s -> %s\n", original, fd_parse_result_name(result));
            CHECK_INT(result, FdParseOk, "golden OK vector parses");
            if(result == FdParseOk) {
                char encoded[FD_FRAME_MAX + 1];
                size_t n = fd_proto_encode(encoded, sizeof(encoded), frame.type, frame.fields, frame.count);
                CHECK(n > 0 && strncmp(encoded, original, n - 1) == 0 && strlen(original) == n - 1, "golden OK vector re-encodes byte-exact");
            }
        } else if(strncmp(line, "BAD ", 4) == 0) {
            char* reason = line + 4;
            char* space = strchr(reason, ' ');
            char* frame_text = space ? space + 1 : reason + strlen(reason);
            if(space) *space = '\0';
            char work[512];
            strcpy(work, frame_text);
            FdFrame frame;
            FdParseResult result = fd_proto_parse(work, strlen(work), &frame);
            if(strcmp(fd_parse_result_name(result), reason) != 0) {
                printf("    vector: %s -> %s (expected %s)\n", frame_text, fd_parse_result_name(result), reason);
            }
            CHECK_STR(fd_parse_result_name(result), reason, "golden BAD vector rejected for the right reason");
        }
        checked++;
    }
    fclose(file);
    CHECK(checked > 20, "read the golden vectors");
}

int main(int argc, char** argv) {
    test_crc();
    test_round_trip();
    test_empty_fields_and_sanitizing();
    test_encode_limits();
    test_line_reader();
    test_parse_u32();
    test_golden_vectors(argc > 1 ? argv[1] : "../../../docs/protocol-vectors.txt");
    return test_summary("fd_proto");
}
