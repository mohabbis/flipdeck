#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Logging stubs — only defined if not already defined by real headers */
#ifndef FURI_LOG_I
#define FURI_LOG_I(...)  ((void)0)
#endif
#ifndef FURI_LOG_W
#define FURI_LOG_W(...)  ((void)0)
#endif
#ifndef FURI_LOG_E
#define FURI_LOG_E(...)  ((void)0)
#endif
#ifndef FURI_LOG_D
#define FURI_LOG_D(...)  ((void)0)
#endif

/* Filesystem stubs */
typedef struct { int _dummy; } FuriFs;

#define FuriFlagRead      (1 << 0)
#define FuriFlagWrite     (1 << 1)
#define FuriFlagCreate    (1 << 2)
#define FuriFlagDirectory (1 << 3)

static inline FuriFs* furi_open(const char* path, int flags, bool truncate) {
    (void)path; (void)flags; (void)truncate; return NULL;
}
static inline void furi_close(FuriFs* fs) { (void)fs; }
static inline int furi_mkdir(const char* path) { (void)path; return 0; }
static inline uint32_t furi_stream_read(FuriFs* fs, void* buf, uint32_t size) {
    (void)fs; (void)buf; (void)size; return 0;
}
static inline uint32_t furi_stream_write(FuriFs* fs, const void* buf, uint32_t size) {
    (void)fs; (void)buf; (void)size; return 0;
}
static inline bool furi_fs_read_directory(FuriFs* dir, char* name, size_t size) {
    (void)dir; (void)name; (void)size; return false;
}
static inline void furi_delay_ms(uint32_t ms) { (void)ms; }
