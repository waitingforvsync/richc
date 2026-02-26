/*
 * file.h - basic file I/O.
 *
 * Error codes
 * -----------
 *   rc_file_error — returned by all file functions; RC_FILE_OK (0) on success.
 *
 *     RC_FILE_OK               — no error
 *     RC_FILE_ERROR_NOT_FOUND  — path does not exist
 *     RC_FILE_ERROR_ACCESS_DENIED — permission denied
 *     RC_FILE_ERROR_TOO_LARGE  — file size does not fit in uint32_t
 *     RC_FILE_ERROR_IO         — any other I/O failure (read/write error, etc.)
 *
 * Text loading
 * ------------
 *   rc_load_text(filename, arena)
 *       Read the entire file into arena memory.  The buffer is always
 *       null-terminated at data[len], so rc_str_as_cstr on the result
 *       returns data directly without copying.
 *       Files are read in binary mode; line endings are not translated.
 *       Returns { text, RC_FILE_OK } on success.
 *       On failure text is invalid ({ NULL, 0 }) and error is non-zero.
 *
 * Text saving
 * -----------
 *   rc_save_text(filename, text)
 *       Write text to filename, creating or truncating the file.
 *       Files are written in binary mode; line endings are not translated.
 *       Returns RC_FILE_OK on success.
 *
 * Binary loading
 * --------------
 *   rc_load_binary(filename, arena)
 *       Read the entire file into arena memory; returns a read-only view.
 *       On failure data is { NULL, 0 } and error is non-zero.
 *
 *   rc_load_binary_array(filename, arena)
 *       Read the entire file into arena memory as a growable rc_bytes array.
 *       cap is set to the file size; call rc_array_uint8_t_push etc. to mutate.
 *       On failure data is { NULL, 0, 0 } and error is non-zero.
 *
 * Binary saving
 * -------------
 *   rc_save_binary(filename, data)
 *       Write data to filename, creating or truncating the file.
 *       Returns RC_FILE_OK on success.
 */

#ifndef RC_FILE_H_
#define RC_FILE_H_

#include "richc/str.h"
#include "richc/arena.h"
#include "richc/bytes.h"

/* ---- error codes ---- */

typedef enum {
    RC_FILE_OK                  = 0,
    RC_FILE_ERROR_NOT_FOUND,
    RC_FILE_ERROR_ACCESS_DENIED,
    RC_FILE_ERROR_TOO_LARGE,
    RC_FILE_ERROR_IO
} rc_file_error;

/* ---- result types ---- */

typedef struct {
    rc_str        text;
    rc_file_error error;
} rc_load_text_result;

typedef struct {
    rc_view_bytes data;
    rc_file_error error;
} rc_load_binary_result;

typedef struct {
    rc_bytes      data;
    rc_file_error error;
} rc_load_binary_array_result;

/* ---- functions ---- */

rc_load_text_result         rc_load_text(const char *filename, rc_arena *a);
rc_file_error               rc_save_text(const char *filename, rc_str text);

rc_load_binary_result       rc_load_binary(const char *filename, rc_arena *a);
rc_load_binary_array_result rc_load_binary_array(const char *filename, rc_arena *a);
rc_file_error               rc_save_binary(const char *filename, rc_view_bytes data);

#endif /* RC_FILE_H_ */
