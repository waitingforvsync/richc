/*
 * mstr.h - mutable/managed string (rc_mstr).
 *
 * rc_mstr is an arena-backed growable string, implemented like rc_array but
 * keeping a '\0' terminator one byte past the content.  The { data, len } fields
 * share layout with rc_str, exposed as s.view, so a read-only view of the current
 * contents is always available without copying.
 *
 * Invariant
 * ---------
 *   data == NULL  - invalid; the zero-initialised "no string" ({ 0 } is valid to
 *                   use as an empty handle, e.g. before the first append).
 *   data != NULL  - valid; at least one byte is allocated (cap >= 1), the content
 *                   occupies len bytes, and data[len] == '\0'.  The used size is
 *                   therefore len + 1, so cap >= len + 1 always holds.
 *
 * Because the buffer always holds '\0' at data[len], rc_str_as_cstr on s.view
 * returns s.data directly (fast path, no copy).
 *
 * Capacity
 * --------
 * cap is the real byte capacity of the allocation (allocation == cap), exactly as
 * for rc_array - there is no hidden byte.  A cap-byte buffer holds a string of up
 * to cap - 1 characters (the terminator takes the last byte).  Growth is geometric
 * (the larger of 2*cap, the request, or 8), matching rc_array.  The capacity
 * parameters of make / reserve / from_* are byte capacities (the cap value).
 *
 * Construction (return by value)
 * ------------------------------
 *   rc_mstr_make(capacity, a)             - empty string in a capacity-byte buffer
 *                                           (capacity == 0 yields the invalid { 0 }).
 *   rc_mstr_from_cstr(s, min_capacity, a) - copy of a null-terminated string;
 *                                           cap = max(strlen(s) + 1, min_capacity).
 *   rc_mstr_from_str(str, min_capacity, a)- copy of an rc_str;
 *                                           cap = max(str.len + 1, min_capacity).
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
 *   rc_mstr_reserve     - ensure the buffer is at least capacity bytes (exact).
 *   rc_mstr_append      - append an rc_str, growing geometrically as needed.
 *   rc_mstr_append_char - append a single character, growing as needed.
 *   rc_mstr_append_n    - append n copies of a character (padding, rules, etc).
 *   rc_mstr_append_<T>  - append the decimal/text form of a number; the i64/u64/f64
 *                         variants do the work and the narrower ones widen into them.
 *   rc_mstr_append_hexN - append the value as fixed-width uppercase hexadecimal,
 *                         zero-padded to N/4 digits (hex8 -> 2, hex16 -> 4, ...).
 *   rc_mstr_replace     - replace all non-overlapping occurrences of find with
 *                         replacement, rewriting in place.
 * append / append_char / reserve accept an invalid (zero-initialised) rc_mstr and
 * allocate on first use, like rc_array push; rc_str arguments must be valid.
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

rc_mstr rc_mstr_make(uint32_t capacity, rc_arena *arena);
rc_mstr rc_mstr_from_cstr(const char *s, uint32_t minimum_capacity, rc_arena *arena);
rc_mstr rc_mstr_from_str(rc_str s, uint32_t minimum_capacity, rc_arena *arena);

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
void rc_mstr_reserve(rc_mstr *s, uint32_t capacity, rc_arena *arena);
void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *arena);
void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *arena);
void rc_mstr_append_n(rc_mstr *s, char c, uint32_t n, rc_arena *arena);
void rc_mstr_append_i64(rc_mstr *s, int64_t value, rc_arena *arena);
void rc_mstr_append_u64(rc_mstr *s, uint64_t value, rc_arena *arena);
void rc_mstr_append_i32(rc_mstr *s, int32_t value, rc_arena *arena);
void rc_mstr_append_u32(rc_mstr *s, uint32_t value, rc_arena *arena);
void rc_mstr_append_f64(rc_mstr *s, double value, rc_arena *arena);
void rc_mstr_append_f32(rc_mstr *s, float value, rc_arena *arena);
void rc_mstr_append_hex8(rc_mstr *s, uint8_t value, rc_arena *arena);
void rc_mstr_append_hex16(rc_mstr *s, uint16_t value, rc_arena *arena);
void rc_mstr_append_hex32(rc_mstr *s, uint32_t value, rc_arena *arena);
void rc_mstr_append_hex64(rc_mstr *s, uint64_t value, rc_arena *arena);
void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *arena);

/* ---- teardown ---- */

void rc_mstr_deinit(rc_mstr *s, rc_arena *arena);

#endif /* RC_MSTR_H_ */
