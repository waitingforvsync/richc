/*
 * array.h - template header: read-only view, mutable span, and growable
 *           arena-backed array for a single element type.
 *
 * One inclusion defines the view, span, and array types for a given element
 * type plus the array operations.  Include it again (after redefining the
 * control macros) to instantiate another element type.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_ARRAY_TYPE   element type (required)
 *   RC_ARRAY_NAME   name suffix (optional; default: RC_ARRAY_TYPE)
 *
 * The generated types are always named rc_array_<suffix>, rc_span_<suffix>, and
 * rc_view_<suffix>.  RC_ARRAY_NAME supplies <suffix>; when it is omitted the
 * element type's own spelling is used (so it must be a single identifier, e.g.
 * int or uint8_t - give an explicit RC_ARRAY_NAME for multi-token types).  Both
 * control macros are undefined again by this header.
 *
 * Generated types
 * ---------------
 *   rc_view_<suffix>  { const RC_ARRAY_TYPE *data; uint32_t num; }
 *   rc_span_<suffix>  {       RC_ARRAY_TYPE *data; uint32_t num; }
 *   rc_array_<suffix> {       RC_ARRAY_TYPE *data; uint32_t num; uint32_t cap; }
 *
 * The span and array embed an anonymous union, so a span converts to its view,
 * and an array to its span or view, with no function call and full type safety:
 *
 *   rc_view_int v = arr.view;     // array  -> view
 *   rc_span_int s = arr.span;     // array  -> span
 *   rc_view_int w = s.view;       // span   -> view
 *
 * Once-only macros (defined on first inclusion, shared by every type)
 * ------------------------------------------------------------------
 *   RC_AT(c, i)   bounds-checked element access for any view/span/array; an
 *                 lvalue, so it can be read or assigned.  Asserts i < c.num.
 *   RC_VIEW(arr)  brace initializer for a view from a C array expression
 *   RC_SPAN(arr)  brace initializer for a span from a C array expression
 *                 (declaration context only; count is derived via sizeof, so
 *                 arr must be a real array, not a pointer).
 *
 * Array operations
 * ----------------
 *   rc_array_<s>_make(initial_capacity, arena)        -> rc_array_<s>
 *   rc_array_<s>_make_copy(view, minimum_capacity, arena) -> rc_array_<s>
 *   rc_array_<s>_get(array, index)                    -> RC_ARRAY_TYPE (by value)
 *   rc_array_<s>_set(array, index, value)             -> void
 *   rc_array_<s>_at(array, index)                     -> RC_ARRAY_TYPE *
 *   rc_array_<s>_is_valid(array)                      -> bool (owns a buffer)
 *   rc_array_<s>_is_empty(array)                      -> bool (num == 0)
 *   rc_array_<s>_reserve(array, capacity, arena)      -> void  (exact capacity)
 *   rc_array_<s>_resize(array, num, arena)            -> rc_span_<s> (whole array)
 *   rc_array_<s>_reset(array)                         -> void
 *   rc_array_<s>_deinit(array, arena)                -> void  (free + zero)
 *   rc_array_<s>_push(array, value, arena)            -> uint32_t (index added)
 *   rc_array_<s>_push_n(array, n, arena)              -> uint32_t (first index; uninitialised)
 *   rc_array_<s>_push_n_zero(array, n, arena)         -> uint32_t (first index; zeroed)
 *   rc_array_<s>_pop(array)                           -> RC_ARRAY_TYPE (asserts non-empty)
 *   rc_array_<s>_pop_n(array, n)                      -> void
 *   rc_array_<s>_append(array, view, arena)           -> uint32_t (new element count)
 *   rc_array_<s>_insert(array, index, value, arena)   -> void
 *   rc_array_<s>_insert_n(array, index, num, arena)   -> void  (uninitialised)
 *   rc_array_<s>_insert_n_zero(array, index, num, arena) -> void  (zeroed)
 *   rc_array_<s>_remove(array, index)                 -> void
 *   rc_array_<s>_remove_n(array, index, num)          -> void
 *
 * Span operations (the span is passed by value; set/at write through to memory)
 * ----------------------------------------------------------------------------
 *   rc_span_<s>_make(data, num)                       -> rc_span_<s> (wraps a pointer)
 *   rc_span_<s>_is_valid(span)                        -> bool (data != NULL)
 *   rc_span_<s>_is_empty(span)                        -> bool (num == 0)
 *   rc_span_<s>_get_subspan(span, start, end)         -> rc_span_<s> (clamped)
 *   rc_span_<s>_get_head(span, n)                     -> rc_span_<s> (clamped)
 *   rc_span_<s>_get_tail(span, n)                     -> rc_span_<s> (clamped)
 *   rc_span_<s>_get(span, index)                      -> RC_ARRAY_TYPE (by value)
 *   rc_span_<s>_set(span, index, value)               -> void
 *   rc_span_<s>_at(span, index)                       -> RC_ARRAY_TYPE *
 *   rc_span_<s>_last_at(span)                         -> RC_ARRAY_TYPE * (asserts non-empty)
 *
 * View operations (read-only; the view is passed by value)
 * --------------------------------------------------------
 *   rc_view_<s>_make(data, num)                       -> rc_view_<s> (wraps a pointer)
 *   rc_view_<s>_is_valid(view)                        -> bool (data != NULL)
 *   rc_view_<s>_is_empty(view)                        -> bool (num == 0)
 *   rc_view_<s>_get_subview(view, start, end)         -> rc_view_<s> (clamped)
 *   rc_view_<s>_get_head(view, n)                     -> rc_view_<s> (clamped)
 *   rc_view_<s>_get_tail(view, n)                     -> rc_view_<s> (clamped)
 *   rc_view_<s>_get(view, index)                      -> RC_ARRAY_TYPE (by value)
 *   rc_view_<s>_at(view, index)                       -> const RC_ARRAY_TYPE *
 *   rc_view_<s>_last_at(view)                         -> const RC_ARRAY_TYPE * (asserts non-empty)
 *
 * Arena parameter
 * ---------------
 * Every operation that may reallocate takes rc_arena *arena last.  Passing NULL
 * is valid when no growth is required; a reallocation with arena == NULL
 * asserts.  Arena allocation never returns NULL (out of memory is a panic).
 *
 * Capacity
 * --------
 * reserve allocates exactly the requested capacity.  The incremental operations
 * (push, append, insert, ...) instead grow through an internal helper to the
 * larger of double the current capacity, the request, or 8 (the std::vector
 * policy), so they stay amortised O(1).  make, make_copy, and resize use exact
 * capacity.
 */

#ifndef RC_TEMPLATE_ARRAY_H_
#define RC_TEMPLATE_ARRAY_H_

#include <string.h>

#include "richc/arena.h"

// RC_AT(c, i): bounds-checked access for any view/span/array (anything with
// .data and .num).  Yields an lvalue and asserts i < c.num.
#define RC_AT(c, i) \
    (*(RC_ASSERT((uint32_t)(i) < (c).num), (c).data + (uint32_t)(i)))

// RC_VIEW(arr) / RC_SPAN(arr): brace initializers from a C array expression,
// for use in a declaration only.  The count is derived via sizeof, so arr must
// be a real array, not a pointer.
#define RC_VIEW(arr) {.data = (arr), .num = (uint32_t)(sizeof(arr) / sizeof((arr)[0]))}
#define RC_SPAN(arr) {.data = (arr), .num = (uint32_t)(sizeof(arr) / sizeof((arr)[0]))}

#endif /* RC_TEMPLATE_ARRAY_H_ */

/* ===== per-type section ===== */

#ifndef RC_ARRAY_TYPE
#  define RC_ARRAY_TYPE int     // to keep intellisense happy
#  error "RC_ARRAY_TYPE must be defined before including richc/template/array.h"
#endif

#ifndef RC_ARRAY_NAME
#  define RC_ARRAY_NAME RC_ARRAY_TYPE
#endif

/* ---- generated type names ---- */

#define RC_ARRAY_  RC_CONCAT(rc_array_, RC_ARRAY_NAME)
#define RC_SPAN_   RC_CONCAT(rc_span_, RC_ARRAY_NAME)
#define RC_VIEW_   RC_CONCAT(rc_view_, RC_ARRAY_NAME)

/* ---- generated function names ---- */

#define RC_ARRAY_MAKE_          RC_CONCAT(RC_ARRAY_, _make)
#define RC_ARRAY_MAKE_COPY_     RC_CONCAT(RC_ARRAY_, _make_copy)
#define RC_ARRAY_GET_           RC_CONCAT(RC_ARRAY_, _get)
#define RC_ARRAY_SET_           RC_CONCAT(RC_ARRAY_, _set)
#define RC_ARRAY_AT_            RC_CONCAT(RC_ARRAY_, _at)
#define RC_ARRAY_IS_VALID_      RC_CONCAT(RC_ARRAY_, _is_valid)
#define RC_ARRAY_IS_EMPTY_      RC_CONCAT(RC_ARRAY_, _is_empty)
#define RC_ARRAY_RESERVE_       RC_CONCAT(RC_ARRAY_, _reserve)
#define RC_ARRAY_GROW_          RC_CONCAT(RC_ARRAY_, _grow_)
#define RC_ARRAY_RESIZE_        RC_CONCAT(RC_ARRAY_, _resize)
#define RC_ARRAY_RESET_         RC_CONCAT(RC_ARRAY_, _reset)
#define RC_ARRAY_DEINIT_        RC_CONCAT(RC_ARRAY_, _deinit)
#define RC_ARRAY_PUSH_          RC_CONCAT(RC_ARRAY_, _push)
#define RC_ARRAY_PUSH_N_        RC_CONCAT(RC_ARRAY_, _push_n)
#define RC_ARRAY_PUSH_N_ZERO_   RC_CONCAT(RC_ARRAY_, _push_n_zero)
#define RC_ARRAY_POP_           RC_CONCAT(RC_ARRAY_, _pop)
#define RC_ARRAY_POP_N_         RC_CONCAT(RC_ARRAY_, _pop_n)
#define RC_ARRAY_APPEND_        RC_CONCAT(RC_ARRAY_, _append)
#define RC_ARRAY_INSERT_        RC_CONCAT(RC_ARRAY_, _insert)
#define RC_ARRAY_INSERT_N_      RC_CONCAT(RC_ARRAY_, _insert_n)
#define RC_ARRAY_INSERT_N_ZERO_ RC_CONCAT(RC_ARRAY_, _insert_n_zero)
#define RC_ARRAY_REMOVE_        RC_CONCAT(RC_ARRAY_, _remove)
#define RC_ARRAY_REMOVE_N_      RC_CONCAT(RC_ARRAY_, _remove_n)

#define RC_SPAN_MAKE_           RC_CONCAT(RC_SPAN_, _make)
#define RC_SPAN_IS_VALID_       RC_CONCAT(RC_SPAN_, _is_valid)
#define RC_SPAN_IS_EMPTY_       RC_CONCAT(RC_SPAN_, _is_empty)
#define RC_SPAN_GET_SUBSPAN_    RC_CONCAT(RC_SPAN_, _get_subspan)
#define RC_SPAN_GET_HEAD_       RC_CONCAT(RC_SPAN_, _get_head)
#define RC_SPAN_GET_TAIL_       RC_CONCAT(RC_SPAN_, _get_tail)
#define RC_SPAN_GET_            RC_CONCAT(RC_SPAN_, _get)
#define RC_SPAN_SET_            RC_CONCAT(RC_SPAN_, _set)
#define RC_SPAN_AT_             RC_CONCAT(RC_SPAN_, _at)
#define RC_SPAN_LAST_AT_        RC_CONCAT(RC_SPAN_, _last_at)

#define RC_VIEW_MAKE_           RC_CONCAT(RC_VIEW_, _make)
#define RC_VIEW_IS_VALID_       RC_CONCAT(RC_VIEW_, _is_valid)
#define RC_VIEW_IS_EMPTY_       RC_CONCAT(RC_VIEW_, _is_empty)
#define RC_VIEW_GET_SUBVIEW_    RC_CONCAT(RC_VIEW_, _get_subview)
#define RC_VIEW_GET_HEAD_       RC_CONCAT(RC_VIEW_, _get_head)
#define RC_VIEW_GET_TAIL_       RC_CONCAT(RC_VIEW_, _get_tail)
#define RC_VIEW_GET_            RC_CONCAT(RC_VIEW_, _get)
#define RC_VIEW_AT_             RC_CONCAT(RC_VIEW_, _at)
#define RC_VIEW_LAST_AT_        RC_CONCAT(RC_VIEW_, _last_at)

/* ---- generated structs ---- */

// View: read-only slice and base type of the family.
typedef struct RC_VIEW_ {
    const RC_ARRAY_TYPE *data;
    uint32_t             num;
} RC_VIEW_;

// Span: mutable slice.  The anonymous union gives a const upcast (.view) and
// the mutable fields (.data / .num) over the same memory.
typedef struct RC_SPAN_ {
    union {
        RC_VIEW_ view;
        struct { RC_ARRAY_TYPE *data; uint32_t num; };
    };
} RC_SPAN_;

// Array: growable, arena-backed.  The union extends the same pattern with a
// span upcast; cap lives outside it.
typedef struct RC_ARRAY_ {
    union {
        RC_SPAN_ span;
        RC_VIEW_ view;
        struct { RC_ARRAY_TYPE *data; uint32_t num; };
    };
    uint32_t cap;
} RC_ARRAY_;

/* ---- generated predicates ---- */

// True when the array owns a buffer (data is non-NULL).
static inline bool RC_ARRAY_IS_VALID_(const RC_ARRAY_ *array)
{
    RC_ASSERT(array);
    return array->data != NULL;
}

// True when the array has no elements.
static inline bool RC_ARRAY_IS_EMPTY_(const RC_ARRAY_ *array)
{
    RC_ASSERT(array);
    return array->num == 0;
}

// True when the span points to a buffer (data is non-NULL).
static inline bool RC_SPAN_IS_VALID_(RC_SPAN_ span)
{
    return span.data != NULL;
}

// True when the span has no elements.
static inline bool RC_SPAN_IS_EMPTY_(RC_SPAN_ span)
{
    return span.num == 0;
}

// True when the view points to a buffer (data is non-NULL).
static inline bool RC_VIEW_IS_VALID_(RC_VIEW_ view)
{
    return view.data != NULL;
}

// True when the view has no elements.
static inline bool RC_VIEW_IS_EMPTY_(RC_VIEW_ view)
{
    return view.num == 0;
}

/* ---- generated array operations ---- */

// Ensure capacity for exactly `capacity` elements; no-op when already at least
// that large.  The arena is required only when a reallocation happens.
static inline void RC_ARRAY_RESERVE_(RC_ARRAY_ *array, uint32_t capacity, rc_arena *arena)
{
    RC_ASSERT(array);
    if (capacity <= array->cap) return;
    RC_ASSERT((size_t)capacity <= UINT32_MAX / sizeof(RC_ARRAY_TYPE));     // byte size overflow
    RC_ASSERT(arena);                                                      // arena required to reallocate
    void *p = rc_arena_realloc(arena, array->data,
                               (uint32_t)(array->cap * sizeof(RC_ARRAY_TYPE)),
                               (uint32_t)(capacity * sizeof(RC_ARRAY_TYPE)));
    array->data = (RC_ARRAY_TYPE *)p;
    array->cap  = capacity;
}

// Internal: ensure capacity for at least `capacity` elements, growing to the
// larger of double the current capacity, the request, or 8 (the std::vector
// policy).  Backs the incremental operations so they stay amortised O(1).
static inline void RC_ARRAY_GROW_(RC_ARRAY_ *array, uint32_t capacity, rc_arena *arena)
{
    RC_ASSERT(array);
    if (capacity <= array->cap) return;
    uint32_t doubled = array->cap * 2;                  // wraps on overflow; request dominates below
    uint32_t new_cap = capacity > doubled ? capacity : doubled;
    if (new_cap < 8) new_cap = 8;
    RC_ARRAY_RESERVE_(array, new_cap, arena);           // asserts on byte-size overflow
}

// Create an empty array with initial_capacity pre-allocated (exact).
// initial_capacity == 0 with arena == NULL yields a valid empty array.
static inline RC_ARRAY_ RC_ARRAY_MAKE_(uint32_t initial_capacity, rc_arena *arena)
{
    RC_ARRAY_ array = {0};
    RC_ARRAY_RESERVE_(&array, initial_capacity, arena);
    return array;
}

// Create an array holding a copy of view, with capacity max(view.num,
// minimum_capacity).
static inline RC_ARRAY_ RC_ARRAY_MAKE_COPY_(RC_VIEW_ view, uint32_t minimum_capacity, rc_arena *arena)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view) || RC_VIEW_IS_EMPTY_(view));
    uint32_t capacity = view.num > minimum_capacity ? view.num : minimum_capacity;
    RC_ARRAY_ array = {0};
    RC_ARRAY_RESERVE_(&array, capacity, arena);
    if (view.num > 0) memcpy(array.data, view.data, view.num * sizeof(RC_ARRAY_TYPE));
    array.num = view.num;
    return array;
}

// Return element `index` by value.  Asserts index < num.
static inline RC_ARRAY_TYPE RC_ARRAY_GET_(const RC_ARRAY_ *array, uint32_t index)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    return array->data[index];
}

// Write `value` to element `index`.  Asserts index < num.
static inline void RC_ARRAY_SET_(RC_ARRAY_ *array, uint32_t index, RC_ARRAY_TYPE value)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    array->data[index] = value;
}

// Return a mutable pointer to element `index`.  Asserts index < num.  With the
// intended one-growable-array-per-arena usage the buffer is always the arena's
// latest allocation, so it grows in place (the VM-backed arena preserves the
// address) and the pointer survives growth; sharing an arena among growables can
// relocate a non-latest buffer on growth and invalidate the pointer.
static inline RC_ARRAY_TYPE *RC_ARRAY_AT_(RC_ARRAY_ *array, uint32_t index)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    return &array->data[index];
}

// Resize to `num` elements and return a span over the whole array.  Added
// elements (when growing) are left uninitialised.
static inline RC_SPAN_ RC_ARRAY_RESIZE_(RC_ARRAY_ *array, uint32_t num, rc_arena *arena)
{
    RC_ARRAY_RESERVE_(array, num, arena);   // asserts the array
    array->num = num;
    return array->span;
}

// Set num to 0, retaining the buffer.
static inline void RC_ARRAY_RESET_(RC_ARRAY_ *array)
{
    RC_ASSERT(array);
    array->num = 0;
}

// Free the backing allocation (best-effort, see rc_arena_free) and zero the
// array, leaving it in a valid empty (zero-initialised) state.
static inline void RC_ARRAY_DEINIT_(RC_ARRAY_ *array, rc_arena *arena)
{
    RC_ASSERT(array);
    if (array->data) {
        rc_arena_free(arena, array->data, (uint32_t)((size_t)array->cap * sizeof(RC_ARRAY_TYPE)));
    }
    *array = (RC_ARRAY_) {0};
}

// Append `value`; return its index.  arena may be NULL when no growth is needed.
static inline uint32_t RC_ARRAY_PUSH_(RC_ARRAY_ *array, RC_ARRAY_TYPE value, rc_arena *arena)
{
    RC_ASSERT(array);
    uint32_t index = array->num;
    RC_ARRAY_GROW_(array, index + 1, arena);
    array->data[index] = value;
    array->num = index + 1;
    return index;
}

// Append `n` uninitialised elements; return the index of the first.  arena may
// be NULL when no growth is needed.
static inline uint32_t RC_ARRAY_PUSH_N_(RC_ARRAY_ *array, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(array);
    uint32_t index = array->num;
    RC_ARRAY_GROW_(array, index + n, arena);
    array->num = index + n;
    return index;
}

// Append `n` zeroed elements; return the index of the first.  arena may be NULL
// when no growth is needed.
static inline uint32_t RC_ARRAY_PUSH_N_ZERO_(RC_ARRAY_ *array, uint32_t n, rc_arena *arena)
{
    RC_ASSERT(array);
    uint32_t index = array->num;
    RC_ARRAY_GROW_(array, index + n, arena);
    if (n > 0) memset(array->data + index, 0, n * sizeof(RC_ARRAY_TYPE));
    array->num = index + n;
    return index;
}

// Remove and return the last element by value.  Asserts the array is non-empty.
static inline RC_ARRAY_TYPE RC_ARRAY_POP_(RC_ARRAY_ *array)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(!RC_ARRAY_IS_EMPTY_(array));
    array->num--;
    return array->data[array->num];
}

// Drop the last `n` elements (no value returned).  Asserts n <= num.
static inline void RC_ARRAY_POP_N_(RC_ARRAY_ *array, uint32_t n)
{
    RC_ASSERT(array);
    RC_ASSERT(n <= array->num);
    array->num -= n;
}

// Append all elements of view; return the array's new element count.  arena may
// be NULL when no growth is needed.
static inline uint32_t RC_ARRAY_APPEND_(RC_ARRAY_ *array, RC_VIEW_ view, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(RC_VIEW_IS_VALID_(view) || RC_VIEW_IS_EMPTY_(view));
    if (view.num > 0) {
        uint32_t index = array->num;
        RC_ARRAY_GROW_(array, index + view.num, arena);
        memcpy(array->data + index, view.data, view.num * sizeof(RC_ARRAY_TYPE));
        array->num = index + view.num;
    }
    return array->num;
}

// Insert `value` at `index`, shifting [index, num) one to the right.
// Asserts index <= num.
static inline void RC_ARRAY_INSERT_(RC_ARRAY_ *array, uint32_t index, RC_ARRAY_TYPE value, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(index <= array->num);
    RC_ARRAY_GROW_(array, array->num + 1, arena);
    memmove(array->data + index + 1, array->data + index,
            (array->num - index) * sizeof(RC_ARRAY_TYPE));
    array->data[index] = value;
    array->num++;
}

// Insert `num` uninitialised elements at `index`, shifting the tail right.
// Asserts index <= num.  No-op when num == 0.
static inline void RC_ARRAY_INSERT_N_(RC_ARRAY_ *array, uint32_t index, uint32_t num, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(index <= array->num);
    if (num == 0) return;
    RC_ARRAY_GROW_(array, array->num + num, arena);
    memmove(array->data + index + num, array->data + index,
            (array->num - index) * sizeof(RC_ARRAY_TYPE));
    array->num += num;
}

// Insert `num` zeroed elements at `index`, shifting the tail right.
// Asserts index <= num.  No-op when num == 0.
static inline void RC_ARRAY_INSERT_N_ZERO_(RC_ARRAY_ *array, uint32_t index, uint32_t num, rc_arena *arena)
{
    RC_ASSERT(array);
    RC_ASSERT(index <= array->num);
    if (num == 0) return;
    RC_ARRAY_GROW_(array, array->num + num, arena);
    memmove(array->data + index + num, array->data + index,
            (array->num - index) * sizeof(RC_ARRAY_TYPE));
    memset(array->data + index, 0, num * sizeof(RC_ARRAY_TYPE));
    array->num += num;
}

// Remove the element at `index`, shifting [index + 1, num) left.
// Asserts index < num.
static inline void RC_ARRAY_REMOVE_(RC_ARRAY_ *array, uint32_t index)
{
    RC_ASSERT(RC_ARRAY_IS_VALID_(array));
    RC_ASSERT(index < array->num);
    memmove(array->data + index, array->data + index + 1,
            (array->num - index - 1) * sizeof(RC_ARRAY_TYPE));
    array->num--;
}

// Remove `num` elements starting at `index`, shifting the tail left.
// Asserts index + num <= num.
static inline void RC_ARRAY_REMOVE_N_(RC_ARRAY_ *array, uint32_t index, uint32_t num)
{
    RC_ASSERT(array);
    RC_ASSERT(index + num <= array->num);
    memmove(array->data + index, array->data + index + num,
            (array->num - index - num) * sizeof(RC_ARRAY_TYPE));
    array->num -= num;
}

/* ---- generated span operations ---- */

// Wrap an existing pointer and count as a span (no allocation).  The function
// form of RC_SPAN for an explicit pointer and count.
static inline RC_SPAN_ RC_SPAN_MAKE_(RC_ARRAY_TYPE *data, uint32_t num)
{
    RC_ASSERT(num == 0 || data != NULL);
    return (RC_SPAN_) {.data = data, .num = num};
}

// Return the sub-span [start, end).  start and end are clamped to the span's
// bounds (and end to >= start), so the result is always valid.
static inline RC_SPAN_ RC_SPAN_GET_SUBSPAN_(RC_SPAN_ span, uint32_t start, uint32_t end)
{
    if (start > span.num) start = span.num;
    if (end > span.num)   end = span.num;
    if (end < start)      end = start;
    return (RC_SPAN_) {.data = span.data + start, .num = end - start};
}

// Return the first `n` elements (clamped to the span's length).
static inline RC_SPAN_ RC_SPAN_GET_HEAD_(RC_SPAN_ span, uint32_t n)
{
    if (n > span.num) n = span.num;
    return (RC_SPAN_) {.data = span.data, .num = n};
}

// Return the elements from index `n` onward (n clamped to the span's length).
static inline RC_SPAN_ RC_SPAN_GET_TAIL_(RC_SPAN_ span, uint32_t n)
{
    if (n > span.num) n = span.num;
    return (RC_SPAN_) {.data = span.data + n, .num = span.num - n};
}

// Return element `index` by value.  Asserts index < num.
static inline RC_ARRAY_TYPE RC_SPAN_GET_(RC_SPAN_ span, uint32_t index)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(index < span.num);
    return span.data[index];
}

// Write `value` to element `index`.  Asserts index < num.
static inline void RC_SPAN_SET_(RC_SPAN_ span, uint32_t index, RC_ARRAY_TYPE value)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(index < span.num);
    span.data[index] = value;
}

// Return a mutable pointer to element `index`.  Asserts index < num.
static inline RC_ARRAY_TYPE *RC_SPAN_AT_(RC_SPAN_ span, uint32_t index)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(index < span.num);
    return &span.data[index];
}

// Return a mutable pointer to the last element.  Asserts the span is non-empty.
static inline RC_ARRAY_TYPE *RC_SPAN_LAST_AT_(RC_SPAN_ span)
{
    RC_ASSERT(RC_SPAN_IS_VALID_(span));
    RC_ASSERT(!RC_SPAN_IS_EMPTY_(span));
    return &span.data[span.num - 1];
}

/* ---- generated view operations ---- */

// Wrap an existing pointer and count as a view (no allocation).  The function
// form of RC_VIEW for an explicit pointer and count.
static inline RC_VIEW_ RC_VIEW_MAKE_(const RC_ARRAY_TYPE *data, uint32_t num)
{
    RC_ASSERT(num == 0 || data != NULL);
    return (RC_VIEW_) {.data = data, .num = num};
}

// Return the sub-view [start, end).  start and end are clamped to the view's
// bounds (and end to >= start), so the result is always valid.
static inline RC_VIEW_ RC_VIEW_GET_SUBVIEW_(RC_VIEW_ view, uint32_t start, uint32_t end)
{
    if (start > view.num) start = view.num;
    if (end > view.num)   end = view.num;
    if (end < start)      end = start;
    return (RC_VIEW_) {.data = view.data + start, .num = end - start};
}

// Return the first `n` elements (clamped to the view's length).
static inline RC_VIEW_ RC_VIEW_GET_HEAD_(RC_VIEW_ view, uint32_t n)
{
    if (n > view.num) n = view.num;
    return (RC_VIEW_) {.data = view.data, .num = n};
}

// Return the elements from index `n` onward (n clamped to the view's length).
static inline RC_VIEW_ RC_VIEW_GET_TAIL_(RC_VIEW_ view, uint32_t n)
{
    if (n > view.num) n = view.num;
    return (RC_VIEW_) {.data = view.data + n, .num = view.num - n};
}

// Return element `index` by value.  Asserts index < num.
static inline RC_ARRAY_TYPE RC_VIEW_GET_(RC_VIEW_ view, uint32_t index)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view));
    RC_ASSERT(index < view.num);
    return view.data[index];
}

// Return a const pointer to element `index`.  Asserts index < num.
static inline const RC_ARRAY_TYPE *RC_VIEW_AT_(RC_VIEW_ view, uint32_t index)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view));
    RC_ASSERT(index < view.num);
    return &view.data[index];
}

// Return a const pointer to the last element.  Asserts the view is non-empty.
static inline const RC_ARRAY_TYPE *RC_VIEW_LAST_AT_(RC_VIEW_ view)
{
    RC_ASSERT(RC_VIEW_IS_VALID_(view));
    RC_ASSERT(!RC_VIEW_IS_EMPTY_(view));
    return &view.data[view.num - 1];
}

/* ---- cleanup ---- */

#undef RC_ARRAY_MAKE_
#undef RC_ARRAY_MAKE_COPY_
#undef RC_ARRAY_GET_
#undef RC_ARRAY_SET_
#undef RC_ARRAY_AT_
#undef RC_ARRAY_IS_VALID_
#undef RC_ARRAY_IS_EMPTY_
#undef RC_ARRAY_RESERVE_
#undef RC_ARRAY_GROW_
#undef RC_ARRAY_RESIZE_
#undef RC_ARRAY_RESET_
#undef RC_ARRAY_DEINIT_
#undef RC_ARRAY_PUSH_
#undef RC_ARRAY_PUSH_N_
#undef RC_ARRAY_PUSH_N_ZERO_
#undef RC_ARRAY_POP_
#undef RC_ARRAY_POP_N_
#undef RC_ARRAY_APPEND_
#undef RC_ARRAY_INSERT_
#undef RC_ARRAY_INSERT_N_
#undef RC_ARRAY_INSERT_N_ZERO_
#undef RC_ARRAY_REMOVE_
#undef RC_ARRAY_REMOVE_N_

#undef RC_SPAN_MAKE_
#undef RC_SPAN_IS_VALID_
#undef RC_SPAN_IS_EMPTY_
#undef RC_SPAN_GET_SUBSPAN_
#undef RC_SPAN_GET_HEAD_
#undef RC_SPAN_GET_TAIL_
#undef RC_SPAN_GET_
#undef RC_SPAN_SET_
#undef RC_SPAN_AT_
#undef RC_SPAN_LAST_AT_

#undef RC_VIEW_MAKE_
#undef RC_VIEW_IS_VALID_
#undef RC_VIEW_IS_EMPTY_
#undef RC_VIEW_GET_SUBVIEW_
#undef RC_VIEW_GET_HEAD_
#undef RC_VIEW_GET_TAIL_
#undef RC_VIEW_GET_
#undef RC_VIEW_AT_
#undef RC_VIEW_LAST_AT_

#undef RC_ARRAY_
#undef RC_SPAN_
#undef RC_VIEW_

#undef RC_ARRAY_NAME
#undef RC_ARRAY_TYPE
