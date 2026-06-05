#include "richc/file.h"

#include <errno.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <io.h>        // _access
#else
#include <unistd.h>    // access
#endif

// Result of load_raw: a mutable byte array holding the loaded contents (the arena
// owns the allocation), plus an error.
typedef struct {
    rc_array_bytes contents;
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

// Translate a failed file operation's errno into the matching rc_file_error.
static rc_file_error file_error_from_errno(void)
{
    return (errno == ENOENT) ? RC_FILE_ERROR_NOT_FOUND
         : (errno == EACCES) ? RC_FILE_ERROR_ACCESS_DENIED
         :                     RC_FILE_ERROR_IO;
}

// Measure an open file in bytes (leaving the read position at end), reporting
// RC_FILE_ERROR_TOO_LARGE when it does not fit in a uint32_t.
static rc_file_size_result file_measure(FILE *f)
{
    if (fseek(f, 0, SEEK_END) != 0)
        return (rc_file_size_result) {.error = RC_FILE_ERROR_IO};

    long size_long = ftell(f);
    if (size_long < 0)
        return (rc_file_size_result) {.error = RC_FILE_ERROR_IO};
    if ((uint64_t)size_long > (uint64_t)UINT32_MAX)
        return (rc_file_size_result) {.error = RC_FILE_ERROR_TOO_LARGE};

    return (rc_file_size_result) {.size = (uint32_t)size_long, .error = RC_FILE_OK};
}

// Open filename, measure its size, and read it into a fresh rc_array_bytes with
// num == size and capacity max(size + 1, minimum_capacity).  One spare byte beyond
// the content is always reserved: rc_file_load_text turns it into the '\0'
// terminator, rc_file_load_binary just leaves it unused.  minimum_capacity is a
// byte-capacity floor on the result.
static load_raw_result load_raw(rc_str filename, uint32_t minimum_capacity, rc_arena *arena)
{
    RC_ASSERT(arena);

    FILE *f = file_open(filename, "rb");
    if (!f)
        return (load_raw_result) {.error = file_error_from_errno()};

    rc_file_size_result measured = file_measure(f);
    if (measured.error != RC_FILE_OK) {
        fclose(f);
        return (load_raw_result) {.error = measured.error};
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_IO};
    }

    uint32_t size = measured.size;
    if ((uint64_t)size + 1 > (uint64_t)UINT32_MAX) {
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_TOO_LARGE};
    }
    uint32_t needed   = size + 1;   // always reserve a spare byte for a terminator
    uint32_t capacity = needed > minimum_capacity ? needed : minimum_capacity;

    rc_array_bytes contents = rc_array_bytes_make(capacity, arena);
    rc_array_bytes_push_n(&contents, size, arena);   // num = size (the room is already reserved)

    // size > 0 implies a non-empty allocation, so contents.data is non-NULL here.
    if (size > 0 && fread(contents.data, 1, size, f) != (size_t)size) {
        rc_array_bytes_deinit(&contents, arena);
        fclose(f);
        return (load_raw_result) {.error = RC_FILE_ERROR_IO};
    }

    fclose(f);
    return (load_raw_result) {.contents = contents};
}

rc_file_size_result rc_file_size(rc_str filename)
{
    FILE *f = file_open(filename, "rb");
    if (!f)
        return (rc_file_size_result) {.error = file_error_from_errno()};

    rc_file_size_result result = file_measure(f);
    fclose(f);
    return result;
}

bool rc_file_exists(rc_str filename)
{
    char path[1024];
    const char *p = rc_str_as_cstr(filename, path, sizeof path);
    // Existence-only check: does not require read permission or open the file.
#ifdef _MSC_VER
    return _access(p, 0) == 0;
#else
    return access(p, F_OK) == 0;
#endif
}

rc_file_error rc_file_delete(rc_str filename)
{
    char path[1024];
    if (remove(rc_str_as_cstr(filename, path, sizeof path)) != 0)
        return file_error_from_errno();

    return RC_FILE_OK;
}

rc_file_load_text_result rc_file_load_text(rc_str filename, uint32_t minimum_capacity, rc_arena *arena)
{
    load_raw_result raw = load_raw(filename, minimum_capacity, arena);
    if (raw.error != RC_FILE_OK)
        return (rc_file_load_text_result) {.error = raw.error};

    // load_raw reserved the spare byte, so to_mstr's terminator push never grows.
    return (rc_file_load_text_result) {.text = rc_array_bytes_to_mstr(&raw.contents, arena)};
}

rc_file_load_binary_result rc_file_load_binary(rc_str filename, uint32_t minimum_capacity, rc_arena *arena)
{
    load_raw_result raw = load_raw(filename, minimum_capacity, arena);
    if (raw.error != RC_FILE_OK)
        return (rc_file_load_binary_result) {.error = raw.error};

    return (rc_file_load_binary_result) {.contents = raw.contents};
}

rc_file_error rc_file_save_text(rc_str filename, rc_str text)
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

rc_file_error rc_file_save_binary(rc_str filename, rc_view_bytes data)
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
