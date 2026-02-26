#include "richc/str.h"
#include <string.h>
#include <ctype.h>

rc_str rc_str_make(const char *s)
{
    if (!s) return (rc_str) {NULL, 0};
    size_t n = strlen(s);
    return (rc_str) {s, n <= (size_t)UINT32_MAX ? (uint32_t)n : UINT32_MAX};
}

bool rc_str_is_equal(rc_str a, rc_str b)
{
    if (a.len != b.len) return false;
    if (a.len == 0)     return true;
    return memcmp(a.data, b.data, a.len) == 0;
}

int rc_str_compare(rc_str a, rc_str b)
{
    uint32_t min_len = a.len < b.len ? a.len : b.len;
    if (min_len > 0) {
        int r = memcmp(a.data, b.data, min_len);
        if (r != 0) return r;
    }
    if (a.len < b.len) return -1;
    if (a.len > b.len) return  1;
    return 0;
}

int rc_str_compare_insensitive(rc_str a, rc_str b)
{
    uint32_t min_len = a.len < b.len ? a.len : b.len;
    for (uint32_t i = 0; i < min_len; i++) {
        int ca = tolower((unsigned char)a.data[i]);
        int cb = tolower((unsigned char)b.data[i]);
        if (ca != cb) return ca - cb;
    }
    if (a.len < b.len) return -1;
    if (a.len > b.len) return  1;
    return 0;
}

bool rc_str_is_equal_insensitive(rc_str a, rc_str b)
{
    return rc_str_compare_insensitive(a, b) == 0;
}

bool rc_str_starts_with(rc_str s, rc_str prefix)
{
    if (prefix.len > s.len) return false;
    if (prefix.len == 0)    return true;
    return memcmp(s.data, prefix.data, prefix.len) == 0;
}

bool rc_str_ends_with(rc_str s, rc_str suffix)
{
    if (suffix.len > s.len) return false;
    if (suffix.len == 0)    return true;
    return memcmp(s.data + s.len - suffix.len, suffix.data, suffix.len) == 0;
}

uint32_t rc_str_find_first(rc_str haystack, rc_str needle)
{
    if (needle.len == 0)            return 0;
    if (needle.len > haystack.len)  return RC_INDEX_NONE;
    uint32_t limit = haystack.len - needle.len;
    for (uint32_t i = 0; i <= limit; i++) {
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0)
            return i;
    }
    return RC_INDEX_NONE;
}

uint32_t rc_str_find_last(rc_str haystack, rc_str needle)
{
    if (needle.len == 0)            return haystack.len;
    if (needle.len > haystack.len)  return RC_INDEX_NONE;
    uint32_t limit = haystack.len - needle.len;
    for (uint32_t i = limit + 1; i > 0; ) {
        --i;
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0)
            return i;
    }
    return RC_INDEX_NONE;
}

bool rc_str_contains(rc_str haystack, rc_str needle)
{
    return rc_str_find_first(haystack, needle) != RC_INDEX_NONE;
}

rc_str rc_str_remove_prefix(rc_str s, rc_str prefix)
{
    if (rc_str_starts_with(s, prefix))
        return rc_str_skip(s, prefix.len);
    return s;
}

rc_str rc_str_remove_suffix(rc_str s, rc_str suffix)
{
    if (rc_str_ends_with(s, suffix))
        return rc_str_left(s, s.len - suffix.len);
    return s;
}

rc_str_pair rc_str_first_split(rc_str s, rc_str split_by)
{
    uint32_t pos = rc_str_find_first(s, split_by);
    if (pos == RC_INDEX_NONE)
        return (rc_str_pair) {s, {NULL, 0}};
    return (rc_str_pair) {
        rc_str_left(s, pos),
        rc_str_skip(s, pos + split_by.len)
    };
}

rc_str_pair rc_str_last_split(rc_str s, rc_str split_by)
{
    uint32_t pos = rc_str_find_last(s, split_by);
    if (pos == RC_INDEX_NONE)
        return (rc_str_pair) {s, {NULL, 0}};
    return (rc_str_pair) {
        rc_str_left(s, pos),
        rc_str_skip(s, pos + split_by.len)
    };
}

const char *rc_str_as_cstr(rc_str s, char *buf, uint32_t buf_size)
{
    if (s.data && s.data[s.len] == '\0')
        return s.data;
    if (!buf || buf_size == 0) return NULL;
    uint32_t copy_len = (s.len < buf_size - 1) ? s.len : buf_size - 1;
    if (copy_len > 0) memcpy(buf, s.data, copy_len);
    buf[copy_len] = '\0';
    return buf;
}
