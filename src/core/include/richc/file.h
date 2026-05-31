/*
 * file.h - whole-file load and save.
 *
 * Filenames are rc_str.  All I/O is binary mode (no line-ending translation),
 * and loaded data is allocated from the supplied arena.
 *
 * Errors
 * ------
 * Every function reports rc_file_error; RC_FILE_OK (0) is success.  On a failed
 * load the returned data/text is the invalid (empty) state and error is set.
 *
 * Size
 * ----
 *   rc_file_size(filename) -> { uint32_t size; rc_file_error error; }
 * Measures the file without reading it; needs no arena.
 *
 * Loading
 * -------
 *   rc_file_load_text   -> rc_mstr
 *   rc_file_load_binary -> rc_array_bytes
 * A load returns an owning, growable result, so the caller can grow it or
 * rc_*_deinit it; for read-only access take the view from the result
 * (result.text.view / result.contents.view).  minimum_capacity sets a floor on
 * the result's capacity, letting it grow a little before reallocating.  Text
 * loads keep a trailing '\0' (the rc_mstr invariant), so rc_str_as_cstr on
 * result.text.view takes its no-copy fast path.
 *
 * Saving
 * ------
 *   rc_file_save_text(filename, text)    writes an rc_str
 *   rc_file_save_binary(filename, data)  writes an rc_view_bytes
 * Both create or truncate the file.
 */

#ifndef RC_FILE_H_
#define RC_FILE_H_

#include "richc/bytes.h"   // also provides rc_mstr (via rc_array_bytes_to_mstr)

/* ---- error codes ---- */

typedef enum {
    RC_FILE_OK = 0,
    RC_FILE_ERROR_NOT_FOUND,
    RC_FILE_ERROR_ACCESS_DENIED,
    RC_FILE_ERROR_TOO_LARGE,
    RC_FILE_ERROR_IO
} rc_file_error;

/* ---- result types ---- */

typedef struct rc_file_size_result        { uint32_t       size;     rc_file_error error; } rc_file_size_result;
typedef struct rc_file_load_text_result   { rc_mstr        text;     rc_file_error error; } rc_file_load_text_result;
typedef struct rc_file_load_binary_result { rc_array_bytes contents; rc_file_error error; } rc_file_load_binary_result;

/* ---- functions ---- */

rc_file_size_result rc_file_size(rc_str filename);

rc_file_load_text_result   rc_file_load_text(rc_str filename, uint32_t minimum_capacity, rc_arena *arena);
rc_file_load_binary_result rc_file_load_binary(rc_str filename, uint32_t minimum_capacity, rc_arena *arena);

rc_file_error rc_file_save_text(rc_str filename, rc_str text);
rc_file_error rc_file_save_binary(rc_str filename, rc_view_bytes data);

#endif /* RC_FILE_H_ */
