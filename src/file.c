#include "richc/file.h"
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

typedef struct { rc_file_error error; void *buf; uint32_t size; } load_raw_result;

static inline load_raw_result load_raw_ok(void *buf, uint32_t size)
{
    return (load_raw_result) {RC_FILE_OK, buf, size};
}

static inline load_raw_result load_raw_fail(rc_file_error error)
{
    return (load_raw_result) {error, NULL, 0};
}

/*
 * Shared helper: open filename, measure its size, allocate size+extra_alloc
 * bytes from the arena, read the file contents into the buffer.
 *
 * On success: returns { RC_FILE_OK, buf, size }.
 * On failure: returns { error, NULL, 0 }.
 * The arena is left clean on failure: any allocation made by this function
 * is freed before returning an error.
 *
 * If size == 0 and extra_alloc == 0, buf is NULL.
 */
static load_raw_result load_raw(const char *filename, rc_arena *a,
                                 uint32_t extra_alloc)
{
    RC_ASSERT(a != NULL && a->base != NULL && "valid arena required");

    FILE *f = fopen(filename, "rb");
    if (!f) {
        rc_file_error err = (errno == ENOENT) ? RC_FILE_ERROR_NOT_FOUND
                          : (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED
                          :                     RC_FILE_ERROR_IO;
        return load_raw_fail(err);
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return load_raw_fail(RC_FILE_ERROR_IO); }

    long size_long = ftell(f);
    if (size_long < 0)               { fclose(f); return load_raw_fail(RC_FILE_ERROR_IO); }

    /* Check that size + extra_alloc fits in uint32_t. */
    if ((uint64_t)size_long + (uint64_t)extra_alloc > (uint64_t)UINT32_MAX) {
        fclose(f);
        return load_raw_fail(RC_FILE_ERROR_TOO_LARGE);
    }

    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return load_raw_fail(RC_FILE_ERROR_IO); }

    uint32_t size       = (uint32_t)size_long;
    uint32_t alloc_size = size + extra_alloc;
    void    *buf        = NULL;
    if (alloc_size > 0) {
        buf = rc_arena_alloc(a, alloc_size);
        RC_PANIC(buf != NULL && "arena OOM");
    }

    /* size > 0 implies alloc_size > 0, so buf is non-NULL here. */
    if (size > 0 && fread(buf, 1, size, f) != (size_t)size) {
        rc_arena_free(a, buf, alloc_size);
        fclose(f);
        return load_raw_fail(RC_FILE_ERROR_IO);
    }

    fclose(f);
    return load_raw_ok(buf, size);
}

rc_load_text_result rc_load_text(const char *filename, rc_arena *a)
{
    load_raw_result r = load_raw(filename, a, 1);
    if (r.error != RC_FILE_OK)
        return (rc_load_text_result) {(rc_str) {NULL, 0}, r.error};
    ((char *)r.buf)[r.size] = '\0';
    return (rc_load_text_result) {(rc_str) {r.buf, r.size}, RC_FILE_OK};
}

rc_file_error rc_save_text(const char *filename, rc_str text)
{
    RC_ASSERT(text.len == 0 || text.data != NULL);

    FILE *f = fopen(filename, "wb");
    if (!f) {
        return (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED : RC_FILE_ERROR_IO;
    }

    if (text.len > 0 && fwrite(text.data, 1, text.len, f) != (size_t)text.len) {
        fclose(f);
        return RC_FILE_ERROR_IO;
    }

    fclose(f);
    return RC_FILE_OK;
}

rc_load_binary_result rc_load_binary(const char *filename, rc_arena *a)
{
    load_raw_result r = load_raw(filename, a, 0);
    if (r.error != RC_FILE_OK)
        return (rc_load_binary_result) {(rc_view_bytes) {NULL, 0}, r.error};
    return (rc_load_binary_result) {(rc_view_bytes) {r.buf, r.size}, RC_FILE_OK};
}

rc_load_binary_array_result rc_load_binary_array(const char *filename, rc_arena *a)
{
    load_raw_result r = load_raw(filename, a, 0);
    if (r.error != RC_FILE_OK)
        return (rc_load_binary_array_result) {(rc_bytes) {0}, r.error};
    return (rc_load_binary_array_result) {
        (rc_bytes) {.data = (uint8_t *)r.buf, .num = r.size, .cap = r.size},
        RC_FILE_OK
    };
}

rc_file_error rc_save_binary(const char *filename, rc_view_bytes data)
{
    RC_ASSERT(data.num == 0 || data.data != NULL);

    FILE *f = fopen(filename, "wb");
    if (!f) {
        return (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED : RC_FILE_ERROR_IO;
    }

    if (data.num > 0 && fwrite(data.data, 1, data.num, f) != (size_t)data.num) {
        fclose(f);
        return RC_FILE_ERROR_IO;
    }

    fclose(f);
    return RC_FILE_OK;
}
