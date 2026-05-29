#include "richc/file.h"

#include <errno.h>
#include <stdio.h>

// Result of load_raw: a byte array that owns the loaded data, plus an error.
typedef struct {
    rc_array_bytes data;
    rc_file_error  error;
} load_raw_result;

// fopen needs a null-terminated path.  A null-terminated rc_str (the common
// case) is used directly; otherwise it is copied into a stack buffer.  fopen
// reads the path during the call and does not retain the pointer, so the buffer
// need not outlive this function.
static FILE *file_open(rc_str filename, const char *mode)
{
    char path[1024];
    return fopen(rc_str_as_cstr(filename, path, sizeof path), mode);
}

// Open filename, measure its size, allocate max(size, minimum_capacity) + extra
// bytes from the arena, and read the file in.  `extra` reserves room beyond the
// content (1 for a text null terminator, 0 for binary); when non-zero the byte
// at data[size] is set to '\0'.  The returned array's cap excludes `extra`.
static load_raw_result load_raw(rc_str filename, uint32_t minimum_capacity,
                                uint32_t extra, rc_arena *arena)
{
    RC_ASSERT(arena);

    FILE *f = file_open(filename, "rb");
    if (!f) {
        rc_file_error error = (errno == ENOENT) ? RC_FILE_ERROR_NOT_FOUND
                            : (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED
                            :                     RC_FILE_ERROR_IO;
        return (load_raw_result) {.error = error};
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_IO};
    }

    long size_long = ftell(f);
    if (size_long < 0) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_IO};
    }
    if ((uint64_t)size_long > (uint64_t)UINT32_MAX) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_TOO_LARGE};
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_IO};
    }

    uint32_t size = (uint32_t)size_long;
    uint32_t capacity = size > minimum_capacity ? size : minimum_capacity;
    if ((uint64_t)capacity + (uint64_t)extra > (uint64_t)UINT32_MAX) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_TOO_LARGE};
    }

    uint32_t alloc = capacity + extra;
    uint8_t *buf = alloc > 0 ? rc_arena_alloc(arena, alloc) : NULL;

    // size > 0 implies alloc > 0, so buf is non-NULL when we read.
    if (size > 0 && fread(buf, 1, size, f) != (size_t)size) {
        (void)rc_arena_free(arena, buf, alloc);
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_IO};
    }
    if (extra > 0) buf[size] = '\0';

    fclose(f);

    rc_array_bytes data = {.data = buf, .num = size, .cap = capacity};
    return (load_raw_result) {.data = data, .error = RC_FILE_OK};
}

rc_load_text_result rc_load_text(rc_str filename, rc_arena *arena)
{
    load_raw_result raw = load_raw(filename, 0, 1, arena);   // +1 for the null terminator
    if (raw.error != RC_FILE_OK)
        return (rc_load_text_result) {.error = raw.error};

    rc_str text = {.data = (const char *)raw.data.data, .len = raw.data.num};
    return (rc_load_text_result) {.text = text, .error = RC_FILE_OK};
}

rc_load_text_mut_result rc_load_text_mut(rc_str filename, uint32_t minimum_capacity, rc_arena *arena)
{
    load_raw_result raw = load_raw(filename, minimum_capacity, 1, arena);
    if (raw.error != RC_FILE_OK)
        return (rc_load_text_mut_result) {.error = raw.error};

    // cap excludes the terminator, so the allocation (cap + 1) is exactly the
    // rc_mstr invariant.
    rc_mstr text = {.data = (const char *)raw.data.data, .len = raw.data.num, .cap = raw.data.cap};
    return (rc_load_text_mut_result) {.text = text, .error = RC_FILE_OK};
}

rc_load_binary_result rc_load_binary(rc_str filename, rc_arena *arena)
{
    load_raw_result raw = load_raw(filename, 0, 0, arena);
    if (raw.error != RC_FILE_OK)
        return (rc_load_binary_result) {.error = raw.error};

    rc_view_bytes data = {.data = raw.data.data, .num = raw.data.num};
    return (rc_load_binary_result) {.data = data, .error = RC_FILE_OK};
}

rc_load_binary_mut_result rc_load_binary_mut(rc_str filename, uint32_t minimum_capacity, rc_arena *arena)
{
    load_raw_result raw = load_raw(filename, minimum_capacity, 0, arena);
    if (raw.error != RC_FILE_OK)
        return (rc_load_binary_mut_result) {.error = raw.error};

    return (rc_load_binary_mut_result) {.data = raw.data, .error = RC_FILE_OK};
}

rc_file_error rc_save_text(rc_str filename, rc_str text)
{
    RC_ASSERT(text.len == 0 || text.data != NULL);

    FILE *f = file_open(filename, "wb");
    if (!f)
        return (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED : RC_FILE_ERROR_IO;

    if (text.len > 0 && fwrite(text.data, 1, text.len, f) != (size_t)text.len) {
        fclose(f);
        return RC_FILE_ERROR_IO;
    }

    fclose(f);
    return RC_FILE_OK;
}

rc_file_error rc_save_binary(rc_str filename, rc_view_bytes data)
{
    RC_ASSERT(data.num == 0 || data.data != NULL);

    FILE *f = file_open(filename, "wb");
    if (!f)
        return (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED : RC_FILE_ERROR_IO;

    if (data.num > 0 && fwrite(data.data, 1, data.num, f) != (size_t)data.num) {
        fclose(f);
        return RC_FILE_ERROR_IO;
    }

    fclose(f);
    return RC_FILE_OK;
}
