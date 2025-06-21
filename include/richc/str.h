#ifndef RICHC_STR_H_
#define RICHC_STR_H_

#include "richc/arena.h"


typedef struct str_t {
    const char *data;
    int32_t len;
} str_t;

typedef struct str_pair_t {
    str_t first;
    str_t second;
} str_pair_t;

typedef struct mstr_t {
    union {
        struct {
            char *data;
            int32_t len;
        };
        str_t str;
    };
    int32_t capacity;
} mstr_t;


#define STR(s) {s, ssizeof(s) - 1}


mstr_t mstr_make(arena_t *arena, int32_t capacity);
mstr_t mstr_from_cstr(arena_t *arena, const char *src, int32_t capacity);
mstr_t mstr_from_str(arena_t *arena, str_t src, int32_t capacity);
bool mstr_is_valid(const mstr_t *str);
bool mstr_is_empty(const mstr_t *str);
void mstr_reset(mstr_t *str);
void mstr_reserve(mstr_t *str, arena_t *arena, int32_t capacity);
void mstr_append(mstr_t *str, arena_t *arena, str_t append_str);
void mstr_append_char(mstr_t *str, arena_t *arena, char append_char);

str_t str_make(const char *s);
bool str_is_valid(str_t str);
bool str_is_empty(str_t str);
bool str_is_equal(str_t a, str_t b);
bool str_is_equal_insensitive(str_t a, str_t b);
int str_compare(str_t a, str_t b);
int str_compare_insensitive(str_t a, str_t b);
str_t str_left(str_t s, int32_t count);
str_t str_right(str_t s, int32_t count);
str_t str_substr(str_t s, int32_t start, int32_t count);
str_t str_from(str_t s, int32_t start);
bool str_startswith(str_t a, str_t b);
bool str_endswith(str_t a, str_t b);
int32_t str_find_first(str_t a, str_t b);
int32_t str_find_last(str_t a, str_t b);
bool str_contains(str_t a, str_t b);
str_t str_remove_prefix(str_t src, str_t prefix);
str_t str_remove_suffix(str_t src, str_t suffix);
str_pair_t str_first_split(str_t src, str_t split_by);
str_pair_t str_last_split(str_t src, str_t split_by);
uint64_t str_hash(str_t s);
const char *str_as_cstr(str_t s, char *buffer, int32_t buffer_size);

#define STR_PRINT(s) (s).len, (s).data
#define STR_FORMAT "%.*s"


#endif // RICHC_STR_H_
