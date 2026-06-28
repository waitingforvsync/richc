#include "richc/mstr.h"

#include <stdio.h>
#include <string.h>

#include "richc/arena.h"

// Internal: ensure capacity for at least `capacity` bytes, growing to the larger
// of double the current capacity, the request, or 8 - the rc_array growth policy.
static void grow(rc_mstr *s, uint32_t capacity, rc_arena *arena)
{
    if (capacity <= s->cap) return;
    uint32_t doubled = s->cap * 2;                       // wraps; the request dominates below
    uint32_t new_cap = capacity > doubled ? capacity : doubled;
    if (new_cap < 8) new_cap = 8;
    rc_mstr_reserve(s, new_cap, arena);
}

rc_mstr rc_mstr_make(uint32_t capacity, rc_arena *arena)
{
    rc_mstr s = {0};
    rc_mstr_reserve(&s, capacity, arena);   // capacity 0 leaves it { 0 } (invalid)
    return s;
}

rc_mstr rc_mstr_from_cstr(const char *s, uint32_t minimum_capacity, rc_arena *arena)
{
    if (!s) return (rc_mstr) {0};
    uint32_t len = (uint32_t)strlen(s);
    uint32_t needed = len + 1;   // content + terminator
    rc_mstr m = rc_mstr_make(needed > minimum_capacity ? needed : minimum_capacity, arena);
    memcpy(m.data, s, needed);   // copies the '\0' too
    m.len = len;
    return m;
}

rc_mstr rc_mstr_from_str(rc_str s, uint32_t minimum_capacity, rc_arena *arena)
{
    if (!s.data) return (rc_mstr) {0};
    uint32_t needed = s.len + 1;   // content + terminator
    rc_mstr m = rc_mstr_make(needed > minimum_capacity ? needed : minimum_capacity, arena);
    if (s.len > 0) memcpy(m.data, s.data, s.len);
    m.data[s.len] = '\0';
    m.len = s.len;
    return m;
}

void rc_mstr_reset(rc_mstr *s)
{
    RC_ASSERT(s);
    if (s->data) {
        s->len = 0;
        s->data[0] = '\0';
    }
}

void rc_mstr_deinit(rc_mstr *s, rc_arena *arena)
{
    RC_ASSERT(s);
    // cap is the real allocation in bytes; free guards NULL
    if (s->data) {
        rc_arena_free(arena, s->data, s->cap);
    }
    *s = (rc_mstr) {0};
}

void rc_mstr_reserve(rc_mstr *s, uint32_t capacity, rc_arena *arena)
{
    RC_ASSERT(s);
    if (capacity <= s->cap) return;
    RC_ASSERT(arena);
    bool was_invalid = (s->data == NULL);
    // realloc calls alloc when data == NULL (cap is 0 then), so this covers both
    // growth and the first allocation off an invalid string.
    s->data = rc_arena_realloc(arena, s->data, s->cap, capacity);
    s->cap  = capacity;
    if (was_invalid) s->data[s->len] = '\0';   // len == 0 here: establish the terminator
}

void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *arena)
{
    RC_ASSERT(s);
    RC_ASSERT(rc_str_is_valid(str));
    if (str.len == 0) return;
    uint32_t new_len = s->len + str.len;
    grow(s, new_len + 1, arena);   // room for the content and the terminator
    memcpy(s->data + s->len, str.data, str.len);
    s->len = new_len;
    s->data[new_len] = '\0';
}

void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *arena)
{
    RC_ASSERT(s);
    grow(s, s->len + 2, arena);    // room for the new char and the terminator
    s->data[s->len] = c;
    s->len++;
    s->data[s->len] = '\0';
}

void rc_mstr_append_u64(rc_mstr *s, uint64_t value, rc_arena *arena)
{
    // Decimal text built back to front, no printf in sight.
    char     buf[20];   // 2^64 - 1 is twenty digits, the widest we can be asked for
    uint32_t i = sizeof buf;
    do {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);
    rc_mstr_append(s, rc_str_make(buf + i, sizeof buf - i), arena);
}

void rc_mstr_append_i64(rc_mstr *s, int64_t value, rc_arena *arena)
{
    if (value < 0) {
        rc_mstr_append_char(s, '-', arena);
    }
    // Take the magnitude in unsigned so INT64_MIN does not overflow its own negation.
    rc_mstr_append_u64(s, (value < 0) ? (uint64_t)0 - (uint64_t)value : (uint64_t)value, arena);
}

void rc_mstr_append_u32(rc_mstr *s, uint32_t value, rc_arena *arena)
{
    rc_mstr_append_u64(s, value, arena);   // widening is lossless
}

void rc_mstr_append_i32(rc_mstr *s, int32_t value, rc_arena *arena)
{
    rc_mstr_append_i64(s, value, arena);
}

void rc_mstr_append_f64(rc_mstr *s, double value, rc_arena *arena)
{
    // Interim: a fixed, compile-time-checked snprintf until richc grows real number formatting.
    char buf[32];
    int  n = snprintf(buf, sizeof buf, "%g", value);
    RC_ASSERT(n >= 0 && (uint32_t)n < sizeof buf);
    rc_mstr_append(s, rc_str_make(buf, (uint32_t)n), arena);
}

void rc_mstr_append_f32(rc_mstr *s, float value, rc_arena *arena)
{
    rc_mstr_append_f64(s, value, arena);
}

void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *arena)
{
    RC_ASSERT(s);
    RC_ASSERT(rc_str_is_valid(find));
    RC_ASSERT(rc_str_is_valid(replacement));
    if (find.len == 0 || s->len == 0) return;

    // Count non-overlapping occurrences.
    uint32_t count = 0;
    {
        rc_str rem = s->view;
        while (true) {
            uint32_t pos = rc_str_find_first(rem, find);
            if (pos == RC_INDEX_NONE) break;
            count++;
            rem = rc_str_skip(rem, pos + find.len);
        }
    }
    if (count == 0) return;

    uint32_t new_len = s->len - count * find.len + count * replacement.len;

    if (replacement.len <= find.len) {
        // Left-to-right in-place (the string shrinks or stays the same size).
        // dst never overtakes src, so no data is clobbered before it is read.
        // memmove handles the dst == src case at the start of the first step.
        char *base = s->data;
        char *src  = base;
        char *end  = base + s->len;
        char *dst  = base;
        for (uint32_t i = 0; i < count; i++) {
            rc_str rem = rc_str_make(src, (uint32_t)(end - src));
            uint32_t pos = rc_str_find_first(rem, find);
            if (pos > 0) { memmove(dst, src, pos); dst += pos; }
            if (replacement.len > 0) {
                memcpy(dst, replacement.data, replacement.len);
                dst += replacement.len;
            }
            src += pos + find.len;
        }
        uint32_t tail = (uint32_t)(end - src);
        if (tail > 0) { memmove(dst, src, tail); dst += tail; }
        *dst = '\0';
        s->len = new_len;
    } else {
        // Right-to-left in-place (the string grows).
        //
        // Reserve to new_len first so the buffer is large enough, then scan
        // occurrences from right to left.  At each step: copy the tail after the
        // current occurrence to its final position (shifting right), write the
        // replacement, then shrink the working window to before the match.
        //
        // Invariant: dst is always >= remaining.data + remaining.len, so memmove
        // handles the overlap correctly and no source byte is overwritten before
        // it is read.
        grow(s, new_len + 1, arena);          // may update s->data
        uint32_t old_len   = s->len;
        char    *base      = s->data;
        char    *dst       = base + new_len;
        *dst = '\0';
        rc_str remaining = rc_str_make(base, old_len);
        for (uint32_t i = 0; i < count; i++) {
            uint32_t pos      = rc_str_find_last(remaining, find);
            uint32_t tail_len = remaining.len - pos - find.len;
            dst -= tail_len;
            if (tail_len > 0)
                memmove(dst, remaining.data + pos + find.len, tail_len);
            dst -= replacement.len;
            memcpy(dst, replacement.data, replacement.len);
            remaining = rc_str_left(remaining, pos);
        }
        // Copy the prefix (text before the first occurrence).
        dst -= remaining.len;
        if (remaining.len > 0) memmove(dst, remaining.data, remaining.len);
        s->len = new_len;
    }
}
