#include <ctype.h>
#include <string.h>
#include "richc/str.h"


mstr_t mstr_make(arena_t *arena, int32_t capacity) {
	check(arena);
	check(capacity > 0);
	char *data = arena_alloc(arena, capacity);
	data[0] = 0;
    return (mstr_t) {
        .data = data,
        .len = 0,
        .capacity = capacity
    };
}

mstr_t mstr_make_from_cstr(arena_t *arena, const char *src, int32_t capacity) {
	check(arena);
	check(src);
	check(capacity >= 0);
	int32_t srclen = (int32_t)strlen(src);
	if (srclen >= capacity) {
		capacity = srclen + 1;
	}

	char *data = arena_alloc(arena, capacity);
	memcpy(data, src, srclen + 1);

    return (mstr_t) {
        .data = data,
        .len = srclen,
        .capacity = capacity
    };
}

mstr_t mstr_make_from_str(arena_t *arena, str_t src, int32_t capacity) {
	check(arena);
	check(src.data);
	check(capacity >= 0);
	if (src.len >= capacity) {
		capacity = src.len + 1;
	}

	char *data = arena_alloc(arena, capacity);
	memcpy(data, src.data, src.len);
	data[src.len] = 0;

    return (mstr_t) {
        .data = data,
        .len = src.len,
        .capacity = capacity
    };
}

bool mstr_is_valid(const mstr_t *str) {
	return str && str->data;
}

bool mstr_is_empty(const mstr_t *str) {
	return str && str->len == 0;
}

void mstr_reset(mstr_t *str) {
	check(str);
	str->len = 0;
}

void mstr_reserve(mstr_t *str, arena_t *arena, int32_t capacity) {
	check(str);
	check(arena);
	check(capacity >= 0);
	if (capacity > str->capacity) {
		str->data = arena_realloc(arena, str->data, str->capacity, capacity);
		str->capacity = capacity;
	}
}

void mstr_append(mstr_t *str, arena_t *arena, str_t append_str) {
	check(str);
	check(append_str.data);
	if (str->len + append_str.len >= str->capacity) {
		mstr_reserve(str, arena, (str->len + append_str.len + 1) * 2);
	}
	check(str->len + append_str.len < str->capacity);
	memcpy(str->data + str->len, append_str.data, append_str.len);
	str->len += append_str.len;
	str->data[str->len] = 0;
}

void mstr_append_char(mstr_t *str, arena_t *arena, char append_char) {
	check(str);
	if (str->len + 1 >= str->capacity) {
		mstr_reserve(str, arena, (str->len + 2) * 2);
	}
	check(str->len + 1 < str->capacity);
	str->data[str->len] = append_char;
	str->data[++str->len] = 0;
}


str_t str_make(const char *s) {
	return (str_t) {s, (int32_t)strlen(s)};
}

bool str_is_valid(str_t s) {
	return s.data;
}

bool str_is_empty(str_t s) {
	check(s.data);
	return s.len == 0;
}

bool str_is_equal(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

bool str_is_equal_insensitive(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	if (a.len != b.len) {
		return false;
	}
	for (int32_t i = 0; i < a.len; i++) {
		if (tolower((unsigned char)a.data[i]) != tolower((unsigned char)b.data[i])) {
			return false;
		}
	}
	return true;
}

int str_compare(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	int result = memcmp(a.data, b.data, (a.len < b.len) ? a.len : b.len);
	if (result == 0) {
		result += (a.len > b.len) - (b.len > a.len);
	}
	return result;
}

int str_compare_insensitive(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	int32_t i = 0;
	int32_t min_len = (a.len < b.len) ? a.len : b.len;
	while (i < min_len) {
		int diff = tolower((unsigned char)a.data[i]) - tolower((unsigned char)b.data[i]);
		if (diff) {
			return diff;
		}
		i++;
	}
	return a.len - b.len;
}

str_t str_left(str_t s, int32_t n) {
	check(s.data);
	return (str_t) {s.data, (n < s.len) ? n : s.len};
}

str_t str_right(str_t s, int32_t n) {
	check(s.data);
	n = (n < s.len) ? n : s.len;
	return (str_t) {s.data + s.len - n, n};
}

str_t str_substr(str_t s, int32_t i, int32_t n) {
	check(s.data);
	i = (i < s.len) ? i : s.len;
	n = (n < s.len - i) ? n : s.len - i;
	return (str_t) {s.data + i, n};
}

str_t str_from(str_t s, int32_t i) {
	check(s.data);
	i = (i < s.len) ? i : s.len;
	return (str_t) {s.data + i, s.len - i};
}

bool str_startswith(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	return a.len >= b.len && memcmp(a.data, b.data, b.len) == 0;
}

bool str_endswith(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	return a.len >= b.len && memcmp(a.data + a.len - b.len, b.data, b.len) == 0;
}

int32_t str_find_first(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	if (a.len >= b.len) {
		for (int32_t i = 0; i <= a.len - b.len; i++) {
			if (memcmp(a.data + i, b.data, b.len) == 0) {
				return i;
			}
		}
	}
	return INDEX_NONE;
}

int32_t str_find_last(str_t a, str_t b) {
	check(a.data);
	check(b.data);
	if (a.len >= b.len) {
		for (int32_t i = a.len - b.len + 1; i-- > 0; ) {
			if (memcmp(a.data + i, b.data, b.len) == 0) {
				return i;
			}
		}
	}
	return INDEX_NONE;
}

bool str_contains(str_t a, str_t b) {
	return str_find_first(a, b) != INDEX_NONE;
}

str_t str_remove_prefix(str_t src, str_t prefix) {
	return str_startswith(src, prefix) ? str_from(src, prefix.len) : src;
}

str_t str_remove_suffix(str_t src, str_t suffix) {
	return str_endswith(src, suffix) ? str_left(src, src.len - suffix.len) : src;
}

str_pair_t str_first_split(str_t src, str_t split_by) {
	int32_t index = str_find_first(src, split_by);
	if (index == INDEX_NONE) {
		return (str_pair_t) {
			src,
			str_right(src, 0)
		};
	}
	return (str_pair_t) {
		str_left(src, index),
		str_from(src, index + split_by.len)
	};
}

str_pair_t str_last_split(str_t src, str_t split_by) {
	int32_t index = str_find_last(src, split_by);
	if (index == INDEX_NONE) {
		return (str_pair_t) {
			str_left(src, 0),
			src
		};
	}
	return (str_pair_t) {
		str_left(src, index),
		str_from(src, index + split_by.len)
	};
}

uint64_t str_hash(str_t s) {
	check(s.data);
	uint64_t hash = 0xB3A5;
	for (int32_t i = 0; i < s.len; i++) {
		hash ^= s.data[i];
		hash *= 0xD8D535FDE0FEED0DULL;
	}
	return hash;
}

const char *str_as_cstr(str_t s, char *buffer, int32_t buffer_size) {
	if (s.data && s.data[s.len] == 0) {
		return s.data;
	}
	else if (s.data && s.len < buffer_size) {
		check(buffer);
		memcpy(buffer, s.data, s.len);
		buffer[s.len] = 0;
		return buffer;
	}
	return 0;
}

