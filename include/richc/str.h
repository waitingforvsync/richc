/*
 * str.h - non-owning string view (rc_str).
 *
 * rc_str holds a pointer and a length; it does not own its data and never
 * allocates.
 *
 * Two distinct states
 * -------------------
 *   Invalid : { NULL, 0 }   — "no string" / "not found" sentinel.
 *   Empty   : { ptr,  0 }   — valid but zero-length (ptr is non-NULL).
 *
 * Types
 * -----
 *   rc_str      { const char *data; uint32_t len; }
 *   rc_str_pair { rc_str first; rc_str second; }
 *
 * Construction
 * ------------
 *   RC_STR(literal)               compile-time view from a string literal;
 *                                 DO NOT pass a char * pointer (sizeof of a
 *                                 pointer gives the pointer width, not the
 *                                 string length).
 *   rc_str_make(const char *)     run-time view from a null-terminated string;
 *                                 returns invalid when s is NULL.
 *
 * Slicing (inline)
 * ----------------
 * rc_str_left, rc_str_right, rc_str_substr, rc_str_skip all clamp
 * out-of-range counts/indices to the valid range rather than asserting.
 *
 * Searching
 * ---------
 * rc_str_find_first / rc_str_find_last return RC_INDEX_NONE on failure.
 * An empty needle is always found: find_first returns 0, find_last returns
 * haystack.len (i.e., at the virtual position just past the last character).
 *
 * Splitting
 * ---------
 * rc_str_first_split / rc_str_last_split return a pair where .first is the
 * text before the delimiter and .second is the text after it (neither
 * includes the delimiter).  When the delimiter is absent .first is the
 * entire input and .second is invalid ({ NULL, 0 }); check with
 * rc_str_is_valid(pair.second) to detect the no-split case.
 *
 * rc_str_as_cstr
 * --------------
 * Fast path: if s.data is non-NULL and already followed by a '\0' byte,
 * s.data is returned directly — no copy.
 * Slow path: otherwise up to buf_size-1 bytes are copied into buf and a
 * '\0' is appended.  Returns NULL when buf is NULL or buf_size is 0.
 */

#ifndef RC_STR_H_
#define RC_STR_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* RC_INDEX_NONE is also defined in find.h; the guard ensures only one copy. */
#ifndef RC_INDEX_NONE
#  define RC_INDEX_NONE ((uint32_t)-1)
#endif

/* ---- types ---- */

typedef struct {
    const char *data;
    uint32_t    len;
} rc_str;

typedef struct {
    rc_str first;
    rc_str second;
} rc_str_pair;

/* ---- construction ---- */

/*
 * Build a view from a string literal at compile time.
 * sizeof(literal) includes the terminating '\0', so subtract 1.
 * Only use with string literals or char[] arrays.
 */
#define RC_STR(s) ((rc_str) {(s), (uint32_t)(sizeof(s) - 1)})

/* Build a view over a null-terminated C string.
 * Returns { NULL, 0 } when s is NULL. */
rc_str rc_str_make(const char *s);

/* ---- predicates (inline) ---- */

/* True when data is non-NULL. */
static inline bool rc_str_is_valid(rc_str s)
{
    return s.data != NULL;
}

/* True when len is 0.  Also true for the invalid state. */
static inline bool rc_str_is_empty(rc_str s)
{
    return s.len == 0;
}

/* ---- predicates ---- */

bool rc_str_is_equal(rc_str a, rc_str b);
bool rc_str_is_equal_insensitive(rc_str a, rc_str b);
int  rc_str_compare(rc_str a, rc_str b);
int  rc_str_compare_insensitive(rc_str a, rc_str b);

/* ---- slicing (inline) ---- */

/* First count characters.  Clamped to s.len when count > s.len. */
static inline rc_str rc_str_left(rc_str s, uint32_t count)
{
    if (count > s.len) count = s.len;
    return (rc_str) {s.data, count};
}

/* Last count characters.  Clamped to s.len when count > s.len. */
static inline rc_str rc_str_right(rc_str s, uint32_t count)
{
    if (count > s.len) count = s.len;
    return (rc_str) {s.data + (s.len - count), count};
}

/* Substring of count characters starting at start.
 * Both start and count are clamped to the valid range. */
static inline rc_str rc_str_substr(rc_str s, uint32_t start, uint32_t count)
{
    if (start > s.len)          start = s.len;
    if (count > s.len - start)  count = s.len - start;
    return (rc_str) {s.data + start, count};
}

/* Suffix of s beginning at start.  Clamped when start > s.len. */
static inline rc_str rc_str_skip(rc_str s, uint32_t start)
{
    if (start > s.len) start = s.len;
    return (rc_str) {s.data + start, s.len - start};
}

/* ---- searching ---- */

bool     rc_str_starts_with(rc_str s, rc_str prefix);
bool     rc_str_ends_with(rc_str s, rc_str suffix);
uint32_t rc_str_find_first(rc_str haystack, rc_str needle);
uint32_t rc_str_find_last(rc_str haystack, rc_str needle);
bool     rc_str_contains(rc_str haystack, rc_str needle);

/* ---- trimming ---- */

rc_str rc_str_remove_prefix(rc_str s, rc_str prefix);
rc_str rc_str_remove_suffix(rc_str s, rc_str suffix);

/* ---- splitting ---- */

rc_str_pair rc_str_first_split(rc_str s, rc_str split_by);
rc_str_pair rc_str_last_split(rc_str s, rc_str split_by);

/* ---- conversion ---- */

const char *rc_str_as_cstr(rc_str s, char *buf, uint32_t buf_size);

#endif /* RC_STR_H_ */
