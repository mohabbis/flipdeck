#pragma once
/* storage/storage.h stub for host-only unit tests. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RECORD_STORAGE "storage"

typedef struct {
    int _dummy;
} Storage;

typedef struct {
    int _dummy;
} File;

typedef struct {
    uint8_t flags;
    uint64_t size;
} FileInfo;

typedef enum {
    FSAM_READ = (1 << 0),
    FSAM_WRITE = (1 << 1),
    FSAM_READ_WRITE = FSAM_READ | FSAM_WRITE,
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_OPEN_ALWAYS = 2,
    FSOM_OPEN_APPEND = 4,
    FSOM_CREATE_NEW = 8,
    FSOM_CREATE_ALWAYS = 16,
} FS_OpenMode;

typedef enum {
    FSE_OK = 0,
    FSE_NOT_EXIST = 1,
} FS_Error;

static inline File* storage_file_alloc(Storage* storage) {
    (void)storage;
    return NULL;
}

static inline void storage_file_free(File* file) {
    (void)file;
}

static inline bool storage_file_open(
    File* file,
    const char* path,
    FS_AccessMode access_mode,
    FS_OpenMode open_mode) {
    (void)file;
    (void)path;
    (void)access_mode;
    (void)open_mode;
    return false;
}

static inline bool storage_file_close(File* file) {
    (void)file;
    return true;
}

static inline size_t storage_file_read(File* file, void* buff, size_t bytes_to_read) {
    (void)file;
    (void)buff;
    (void)bytes_to_read;
    return 0;
}

static inline size_t storage_file_write(File* file, const void* buff, size_t bytes_to_write) {
    (void)file;
    (void)buff;
    return bytes_to_write;
}

static inline bool storage_dir_open(File* file, const char* path) {
    (void)file;
    (void)path;
    return false;
}

static inline bool storage_dir_close(File* file) {
    (void)file;
    return true;
}

static inline bool storage_dir_read(
    File* file,
    FileInfo* fileinfo,
    char* name,
    uint16_t name_length) {
    (void)file;
    (void)fileinfo;
    (void)name;
    (void)name_length;
    return false;
}

static inline FS_Error storage_common_mkdir(Storage* storage, const char* path) {
    (void)storage;
    (void)path;
    return FSE_OK;
}
