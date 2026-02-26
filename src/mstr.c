#include "richc/mstr.h"
#include <string.h>

rc_mstr rc_mstr_make(uint32_t cap, rc_arena *a)
{
    RC_ASSERT(a != NULL);
    char *buf = rc_arena_alloc(a, cap + 1);
    buf[0] = '\0';
    return (rc_mstr) {.data = (const char *)buf, .len = 0, .cap = cap};
}

rc_mstr rc_mstr_from_cstr(const char *s, uint32_t max_cap, rc_arena *a)
{
    RC_ASSERT(a != NULL);
    if (!s) return (rc_mstr) {.data = NULL, .len = 0, .cap = 0};
    uint32_t len = (uint32_t)strlen(s);
    uint32_t cap = len > max_cap ? len : max_cap;
    char *buf = rc_arena_alloc(a, cap + 1);
    memcpy(buf, s, len + 1);   /* copies the '\0' too */
    return (rc_mstr) {.data = (const char *)buf, .len = len, .cap = cap};
}

rc_mstr rc_mstr_from_str(rc_str s, uint32_t max_cap, rc_arena *a)
{
    RC_ASSERT(a != NULL);
    if (!s.data) return (rc_mstr) {.data = NULL, .len = 0, .cap = 0};
    uint32_t cap = s.len > max_cap ? s.len : max_cap;
    char *buf = rc_arena_alloc(a, cap + 1);
    if (s.len > 0) memcpy(buf, s.data, s.len);
    buf[s.len] = '\0';
    return (rc_mstr) {.data = (const char *)buf, .len = s.len, .cap = cap};
}

void rc_mstr_reset(rc_mstr *s)
{
    if (s->data) {
        s->len = 0;
        ((char *)s->data)[0] = '\0';
    }
}

void rc_mstr_reserve(rc_mstr *s, uint32_t new_cap, rc_arena *a)
{
    if (new_cap <= s->cap) return;
    RC_ASSERT(a != NULL);
    char *buf = rc_arena_realloc(a, (char *)s->data, s->cap + 1, new_cap + 1);
    s->data = (const char *)buf;
    s->cap  = new_cap;
}

void rc_mstr_append(rc_mstr *s, rc_str str, rc_arena *a)
{
    if (str.len == 0) return;
    uint32_t new_len = s->len + str.len;
    if (new_len > s->cap) {
        uint32_t new_cap = s->cap < 8 ? 8 : s->cap * 2;
        if (new_cap < new_len) new_cap = new_len;
        rc_mstr_reserve(s, new_cap, a);
    }
    memcpy((char *)s->data + s->len, str.data, str.len);
    s->len = new_len;
    ((char *)s->data)[new_len] = '\0';
}

void rc_mstr_append_char(rc_mstr *s, char c, rc_arena *a)
{
    if (s->len == s->cap) {
        uint32_t new_cap = s->cap < 8 ? 8 : s->cap * 2;
        rc_mstr_reserve(s, new_cap, a);
    }
    ((char *)s->data)[s->len] = c;
    s->len++;
    ((char *)s->data)[s->len] = '\0';
}

void rc_mstr_replace(rc_mstr *s, rc_str find, rc_str replacement, rc_arena *a)
{
    if (find.len == 0 || s->len == 0) return;

    /* Count non-overlapping occurrences. */
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
        /*
         * Left-to-right in-place (string shrinks or stays the same size).
         * dst never overtakes src, so no data is clobbered before it is read.
         * memmove handles the dst==src case at the start of the first step.
         */
        char *base = (char *)s->data;
        char *src  = base;
        char *end  = base + s->len;
        char *dst  = base;
        for (uint32_t i = 0; i < count; i++) {
            rc_str rem = (rc_str) {src, (uint32_t)(end - src)};
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
        /*
         * Right-to-left in-place (string grows).
         *
         * Reserve to new_len first so the buffer is large enough, then scan
         * occurrences from right to left.  At each step: copy the tail after
         * the current occurrence to its final position (shifting right), write
         * the replacement, then shrink the working window to before the match.
         *
         * Invariant: dst is always >= remaining.data + remaining.len, so
         * memmove handles the overlap correctly and no source byte is
         * overwritten before it is read.
         */
        rc_mstr_reserve(s, new_len, a);   /* may update s->data */
        uint32_t old_len  = s->len;
        char    *base     = (char *)s->data;
        char    *dst      = base + new_len;
        *dst = '\0';
        rc_str remaining = (rc_str) {base, old_len};
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
        /* Copy the prefix (text before the first occurrence). */
        dst -= remaining.len;
        if (remaining.len > 0) memmove(dst, remaining.data, remaining.len);
        s->len = new_len;
    }
}
