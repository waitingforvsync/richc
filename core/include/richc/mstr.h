/*
 * mstr.h - mutable/managed string (rc_mstr).
 *
 * rc_mstr is an arena-backed growable string.  The { data, len } fields share
 * layout with rc_str, exposed as s.view, so a non-owning view of the current
 * contents is always available without copying.
 *
 * States
 * ------
 *   Invalid : { NULL, 0, 0 }   - not initialised / allocation failed.
 *   Valid   : { ptr, len, cap } where ptr is non-NULL and len <= cap.
 *
 * The buffer always holds a '\0' byte at data[len], so rc_str_as_cstr on
 * s.view returns s.data directly (fast path, no copy).
 *
 * Capacity
 * --------
 * cap is the number of characters the buffer can hold, not counting the null
 * terminator.  The underlying allocation is always cap + 1 bytes.
 *
 * Construction (return by value)
 * ------------------------------
 *   rc_mstr_make(cap, a)              - empty string with the given capacity.
 *   rc_mstr_from_cstr(s, max_cap, a)  - copy of a null-terminated string;
 *                                       cap = max(strlen(s), max_cap).
 *   rc_mstr_from_str(str, max_cap, a) - copy of an rc_str;
 *                                       cap = max(str.len, max_cap).
 * The from_* constructors return the invalid state when given a NULL / invalid
 * source.
 *
 * Predicates (inline)
 * -------------------
 *   rc_mstr_is_valid, rc_mstr_is_empty
 *
 * Mutation
 * --------
 *   rc_mstr_reset       - set len to 0, retain the buffer.
 *   rc_mstr_reserve     - ensure capacity for at least new_cap characters.
 *   rc_mstr_append      - append an rc_str, growing as needed (doubling, min 8).
 *   rc_mstr_append_char - append a single character, growing as needed.
 *   rc_mstr_replace     - replace all non-overlapping occurrences of find with
 *                         replacement, rewriting in place.
 * The mutation functions require a valid rc_mstr and valid rc_str arguments.
 *
 * Teardown
 * --------
 *   rc_mstr_deinit - free the backing allocation and zero the struct back to the
 *                    invalid state.  Use rc_mstr_reset to keep the buffer.
 */

#ifndef RC_MSTR_H_
#define RC_MSTR_H_

#include "richc/str.h"   // also provides <stdint.h> / <stdbool.h>

// rc_mstr only ever takes an arena by pointer, so a forward declaration is
// enough here; translation units needing the full type include arena.h.
typedef struct rc_arena rc_arena;

/* ---- type ---- */

typedef struct rc_mstr {
    union {
        // data is the writable, owned buffer; view aliases it as a read-only
        // rc_str (char * and const char * share representation), so the current
        // contents are always available as s.view without a copy.
        struct { char *data; uint32_t len; };
        rc_str view;
    };
    uint32_t cap;
} rc_mstr;

/* ---- construction (return by value) ---- */

rc_mstr rc_mstr_make(uint32_t cap, rc_arena *arena);
rc_mstr rc_mstr_from_cstr(const char *s, uint32_t max_cap, rc_arena *arena);
rc_mstr rc_mstr_from_str(rc_str s, uint32_t max_cap, rc_arena *arena);

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
void rc_mstr_reserve(rc_mstr *s, uint32_t new_cap, rc_arena *arena);
void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *arena);
void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *arena);
void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *arena);

/* ---- teardown ---- */

void rc_mstr_deinit(rc_mstr *s, rc_arena *arena);

#endif /* RC_MSTR_H_ */
