#include <stdio.h>
#include "richc/file.h"


str_t file_remove_path(str_t filename) {
    check(filename.data);
    return str_last_split(str_last_split(filename, str_make("/")).second, str_make("\\")).second;
}


str_t file_get_path(str_t filename) {
    check(filename.data);
    return (str_t) {filename.data, file_remove_path(filename).data - filename.data};
}


file_text_load_result_t file_text_load(arena_t *arena, str_t filename) {
    check(arena);
    
    char buffer[1024];
    const char *filename_cstr = str_as_cstr(filename, buffer, sizeof buffer);

    FILE *file = filename_cstr ? fopen(filename_cstr, "r") : stdin;
    if (!file) {
        return (file_text_load_result_t) {
            .error = {file_error_not_found}
        };
    }

    mstr_t text = mstr_make(arena, 0x1000);
    char line[0xFFF];
    do {
        uint32_t n = (uint32_t)fread(line, 1, sizeof line, file);
        if (n > 0) {
            mstr_append(&text, arena, (str_t) {line, n});
        }
    }
    while (!feof(file));

    fclose(file);

    return (file_text_load_result_t) {
        .text = text,
        .error = {file_error_none}
    };
}


file_load_result_t file_load(arena_t *arena, str_t filename) {
    check(arena);

    char name_buffer[1024];
    const char *filename_cstr = str_as_cstr(filename, name_buffer, sizeof name_buffer);

    FILE *file = fopen(filename_cstr, "rb");
    if (!file) {
        return (file_load_result_t) {
            .error = {file_error_not_found}
        };
    }

    byte_array_t bytes = byte_array_make(arena, 0x1000);
    uint8_t buffer[0x1000];
    do {
        uint32_t n = (uint32_t)fread(buffer, 1, sizeof buffer, file);
        if (n > 0) {
            byte_array_append(&bytes, arena, (byte_array_view_t) {buffer, n});
        }
    }
    while (!feof(file));

    fclose(file);

    return (file_load_result_t) {
        .bytes = bytes,
        .error = {file_error_none}
    };
}


file_error_t file_save(str_t filename, byte_array_view_t bytes) {
    check(bytes.data);

    char buffer[1024];
    const char *filename_cstr = str_as_cstr(filename, buffer, sizeof buffer);

    FILE *file = fopen(filename_cstr, "wb");
    if (!file) {
        fclose(file);
        return (file_error_t) {file_error_unspecified};
    }

    int32_t size_written = (int32_t)fwrite(bytes.data, 1, bytes.num, file);
    if (size_written != bytes.num) {
        fclose(file);
        return (file_error_t) {file_error_unspecified};
    }

    fclose(file);
    return (file_error_t) {file_error_none};
}


const char *file_error_as_text(file_error_t error) {
    switch (error.type) {
        case file_error_none:
            return "";
        case file_error_not_found:
            return "File not found";
        case file_error_unspecified:
            return "Unspecified file error";
        default:
            return "Unknown file error";
    }
}
