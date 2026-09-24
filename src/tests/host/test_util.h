#pragma once
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if(cond) {                                                         \
            g_pass++;                                                      \
        } else {                                                           \
            g_fail++;                                                      \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));      \
        }                                                                  \
    } while(0)

#define CHECK_STR(a, b, msg) CHECK(strcmp((a), (b)) == 0, msg)
#define CHECK_INT(a, b, msg) CHECK((long)(a) == (long)(b), msg)

static int test_summary(const char* name) {
    printf("%s: %d passed, %d failed\n", name, g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
