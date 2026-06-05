/*
 * bytes.h - byte containers: rc_view_bytes, rc_span_bytes, rc_array_bytes.
 *
 * A convenience instantiation of the array template (richc/template/array.h)
 * for uint8_t.  Include it once to get the generated types and their operations
 * (rc_array_bytes_make, rc_array_bytes_push, rc_span_bytes_get, ...), plus the
 * string bridges rc_view_bytes_as_str / rc_span_bytes_as_str / rc_array_bytes_to_mstr.
 */
#ifndef RC_BYTES_H_
#define RC_BYTES_H_

#include "richc/mstr.h"   // for rc_str / rc_mstr (the string bridges below)

#define RC_ARRAY_TYPE uint8_t
#define RC_ARRAY_NAME bytes
#include "richc/template/array.h"

// Reinterpret a byte view (or span) as a read-only rc_str - no copy, no
// allocation; the bytes need not be null-terminated, since rc_str carries its own
// length.  Non-destructive (the source stays usable), hence as_, not to_.
static inline rc_str rc_view_bytes_as_str(rc_view_bytes bytes)
{
    return rc_str_make((const char *)bytes.data, bytes.num);
}

static inline rc_str rc_span_bytes_as_str(rc_span_bytes bytes)
{
    return rc_view_bytes_as_str(bytes.view);
}

// Move a byte buffer into an rc_mstr: append a '\0' terminator (growing by one
// byte only if the buffer was exactly full), reinterpret the preceding bytes as a
// mutable string, and reset *bytes to the empty { 0 } (its len excludes the
// terminator, cap is the array's real capacity).  Clearing the source is what
// makes this a move: the arena still owns the memory, but two mutable containers
// must not both refer to one buffer (either could grow or rewrite it and corrupt
// the other), so the source is cleared to leave the rc_mstr as the single writer.
static inline rc_mstr rc_array_bytes_to_mstr(rc_array_bytes *bytes, rc_arena *arena)
{
    rc_array_bytes_push(bytes, 0, arena);
    rc_mstr s = {.data = (char *)bytes->data, .len = bytes->num - 1, .cap = bytes->cap};
    *bytes = (rc_array_bytes) {0};   // sole mutable handle is now the mstr
    return s;
}

#endif /* RC_BYTES_H_ */
