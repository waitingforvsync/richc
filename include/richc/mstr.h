/*
 * mstr.h - mutable/managed string (rc_mstr).
 *
 * rc_mstr is an arena-backed growable string.  The { data, len } fields
 * share layout with rc_str, exposed as s.view, so a non-owning view of the
 * current contents is always available without copying.
 *
 * States
 * ------
 *   Invalid : { NULL, 0, 0 }  — not initialised / allocation failed.
 *   Valid   : { ptr, len, cap } where ptr non-NULL, len <= cap.
 *
 * The buffer always holds a '\0' byte at data[len], so rc_str_as_cstr on
 * s.view returns s.data directly (fast path, no copy).
 *
 * Capacity
 * --------
 * cap is the number of characters the buffer can hold, not counting the
 * null terminator.  The underlying allocation is always cap + 1 bytes.
 *
 * Arena rules
 * -----------
 * Functions that may allocate assert that the arena pointer is non-NULL.
 * Functions that never allocate (is_valid, is_empty, reset) do not take
 * an arena parameter.
 *
 * Types
 * -----
 *   rc_mstr  { union { struct { const char *data; uint32_t len; }; rc_str view; }; uint32_t cap; }
 *
 * Construction (return by value)
 * --------------------------------
 *   rc_mstr_make(cap, arena)               — empty string, given initial capacity.
 *   rc_mstr_from_cstr(s, max_cap, a)  — copy of null-terminated string;
 *                                            actual_cap = max(strlen(s), max_cap).
 *   rc_mstr_from_str(str, max_cap, a)     — copy of rc_str;
 *                                            actual_cap = max(str.len, max_cap).
 *
 * Predicates (inline)
 * -------------------
 *   rc_mstr_is_valid, rc_mstr_is_empty
 *
 * Mutation
 * --------
 *   rc_mstr_reset          — set len to 0, retain buffer.
 *   rc_mstr_reserve        — ensure at least N characters of capacity.
 *   rc_mstr_append         — append rc_str, growing as needed (doubling, min 8).
 *   rc_mstr_append_char    — append single character, growing as needed.
 *   rc_mstr_replace        — replace all non-overlapping occurrences of find
 *                            with replacement.  When replacement is no longer
 *                            than find, rewrites left to right in-place.  When
 *                            replacement is longer, reserves to the new length
 *                            then rewrites right to left in-place (no fresh
 *                            buffer allocation; a is asserted non-NULL).
 */

#ifndef RC_MSTR_H_
#define RC_MSTR_H_

#include <stdint.h>
#include <stdbool.h>
#include "richc/str.h"
#include "richc/arena.h"

/* ---- type ---- */

typedef struct {
    union {
        struct { const char *data; uint32_t len; };
        rc_str view;
    };
    uint32_t cap;
} rc_mstr;

/* ---- construction (return by value) ---- */

rc_mstr rc_mstr_make(uint32_t cap, rc_arena *a);
rc_mstr rc_mstr_from_cstr(const char *s, uint32_t max_cap, rc_arena *a);
rc_mstr rc_mstr_from_str(rc_str s, uint32_t max_cap, rc_arena *a);

/* ---- predicates (inline) ---- */

/* True when the rc_mstr holds a valid (non-NULL) buffer. */
static inline bool rc_mstr_is_valid(const rc_mstr *s)
{
    return s->data != NULL;
}

/* True when len is 0.  Also true for the invalid state. */
static inline bool rc_mstr_is_empty(const rc_mstr *s)
{
    return s->len == 0;
}

/* ---- mutation ---- */

void rc_mstr_reset(rc_mstr *s);
void rc_mstr_reserve(rc_mstr *s, uint32_t new_cap, rc_arena *a);
void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *a);
void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *a);
void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *a);

#endif /* RC_MSTR_H_ */
