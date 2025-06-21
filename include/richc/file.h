#ifndef RICHC_FILE_H_
#define RICHC_FILE_H_

#include "richc/arena.h"
#include "richc/byte_array.h"
#include "richc/str.h"


enum file_errors_t {
    file_error_none,
    file_error_not_found,
    file_error_unspecified,
};

typedef struct file_error_t {
    uint32_t type;
} file_error_t;

typedef struct file_text_load_result_t {
    mstr_t text;
    file_error_t error;
} file_text_load_result_t;

typedef struct file_load_result_t {
    byte_array_t bytes;
    file_error_t error;
} file_load_result_t;


str_t file_remove_path(str_t filename);
str_t file_get_path(str_t filename);
file_text_load_result_t file_text_load(arena_t *arena, str_t filename);
file_load_result_t file_load(arena_t *arena, str_t filename);
file_error_t file_save(str_t filename, byte_array_view_t bytes);
const char *file_error_as_text(file_error_t error);


#endif // ifndef RICHC_FILE_H_
