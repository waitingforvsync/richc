#include "richc/test.h"

#define RC_ARRAY_TYPE int
#include "richc/template/array.h"

// Most tests run against a fresh default arena provided by this fixture.
RC_TEST_GROUP_DATA(array) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(array, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(array, fix)
{
    rc_arena_destroy(&fix->a);
}

/* ---- construction ---- */

RC_TEST_STEP(array, make, fix)
{
    rc_array_int a = rc_array_int_make(16, &fix->a);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 16u);
    RC_CHECK_TRUE(a.data != NULL);
}

RC_TEST(array, make_empty)
{
    // Zero capacity with a NULL arena is valid and allocates nothing.
    rc_array_int a = rc_array_int_make(0, NULL);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 0u);
}

RC_TEST_STEP(array, make_copy, fix)
{
    int src[] = {1, 2, 3, 4};
    rc_view_int v = RC_VIEW(src);
    rc_array_int a = rc_array_int_make_copy(v, 10, &fix->a);
    RC_CHECK(a.num, ==, 4u);
    RC_CHECK(a.cap, ==, 10u);       // capacity = max(view.num, minimum_capacity)
    RC_CHECK(rc_array_int_get(&a, 0), ==, 1);
    RC_CHECK(rc_array_int_get(&a, 3), ==, 4);
}

/* ---- element access ---- */

RC_TEST_STEP(array, get_set_at, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 10, &fix->a);
    rc_array_int_push(&a, 20, &fix->a);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 10);
    rc_array_int_set(&a, 0, 99);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 99);
    *rc_array_int_at(&a, 1) = 88;
    RC_CHECK(rc_array_int_get(&a, 1), ==, 88);
}

RC_TEST_STEP(array, rc_at, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 11, &fix->a);
    rc_array_int_push(&a, 22, &fix->a);
    RC_CHECK(RC_AT(a, 0), ==, 11);   // read
    RC_AT(a, 1) = 33;                // assign through the lvalue
    RC_CHECK(RC_AT(a, 1), ==, 33);
}

RC_TEST_STEP(array, is_valid_is_empty, fix)
{
    rc_array_int a = rc_array_int_make(0, NULL);   // {0}: no buffer yet
    RC_CHECK_FALSE(rc_array_int_is_valid(&a));
    RC_CHECK_TRUE(rc_array_int_is_empty(&a));
    rc_array_int_push(&a, 1, &fix->a);
    RC_CHECK_TRUE(rc_array_int_is_valid(&a));       // buffer allocated
    RC_CHECK_FALSE(rc_array_int_is_empty(&a));
}

/* ---- push / pop / append ---- */

RC_TEST_STEP(array, push, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    RC_CHECK(rc_array_int_push(&a, 5, &fix->a), ==, 0u);   // returns the index added
    RC_CHECK(rc_array_int_push(&a, 6, &fix->a), ==, 1u);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 5);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 6);
}

RC_TEST_STEP(array, push_no_growth, fix)
{
    rc_array_int a = rc_array_int_make(4, &fix->a);   // reserves exactly 4
    rc_array_int_push(&a, 1, NULL);                   // no growth: arena may be NULL
    rc_array_int_push(&a, 2, NULL);
    RC_CHECK(a.num, ==, 2u);
}

RC_TEST_STEP(array, push_n, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    uint32_t first = rc_array_int_push_n(&a, 3, &fix->a);   // 3 uninitialised
    RC_CHECK(first, ==, 1u);                                // index of the first new element
    RC_CHECK(a.num, ==, 4u);
    for (uint32_t k = first; k < a.num; k++) rc_array_int_set(&a, k, (int)(k * 10));
    RC_CHECK(rc_array_int_get(&a, 1), ==, 10);
    RC_CHECK(rc_array_int_get(&a, 3), ==, 30);
}

RC_TEST_STEP(array, push_n_zero, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 5, &fix->a);
    uint32_t first = rc_array_int_push_n_zero(&a, 2, &fix->a);
    RC_CHECK(first, ==, 1u);
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 5);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 0);     // zeroed
    RC_CHECK(rc_array_int_get(&a, 2), ==, 0);     // zeroed
}

RC_TEST_STEP(array, pop, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 10, &fix->a);
    rc_array_int_push(&a, 20, &fix->a);
    RC_CHECK(rc_array_int_pop(&a), ==, 20);       // returns last by value
    RC_CHECK(a.num, ==, 1u);
    RC_CHECK(rc_array_int_pop(&a), ==, 10);
    RC_CHECK(a.num, ==, 0u);
}

RC_TEST_STEP(array, pop_n, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    for (int k = 0; k < 5; k++) rc_array_int_push(&a, k, &fix->a);   // {0,1,2,3,4}
    rc_array_int_pop_n(&a, 2);                                       // drop last 2
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_int_get(&a, 2), ==, 2);
    rc_array_int_pop_n(&a, 3);                                       // drop the rest
    RC_CHECK(a.num, ==, 0u);
}

RC_TEST_STEP(array, append, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    int more[] = {2, 3, 4};
    rc_view_int v = RC_VIEW(more);
    uint32_t count = rc_array_int_append(&a, v, &fix->a);
    RC_CHECK(count, ==, 4u);         // returns the new element count
    RC_CHECK(a.num, ==, 4u);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 2);
    RC_CHECK(rc_array_int_get(&a, 3), ==, 4);
}

RC_TEST_STEP(array, append_from_array_view, fix)
{
    // Appending one array's contents to another via the array -> view union.
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int b = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&b, 9, &fix->a);
    rc_array_int_push(&b, 8, &fix->a);
    rc_array_int_append(&a, b.view, &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 9);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 8);
}

/* ---- reserve / resize / reset ---- */

RC_TEST_STEP(array, reserve, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_reserve(&a, 100, &fix->a);
    RC_CHECK(a.cap, ==, 100u);            // exact capacity, no doubling
    RC_CHECK(a.num, ==, 0u);
    rc_array_int_reserve(&a, 10, NULL);   // smaller: no-op, arena may be NULL
    RC_CHECK(a.cap, ==, 100u);
}

RC_TEST_STEP(array, resize_grow, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 7, &fix->a);
    rc_span_int s = rc_array_int_resize(&a, 4, &fix->a);
    RC_CHECK(a.num, ==, 4u);
    RC_CHECK(s.num, ==, 4u);              // span over the whole array
    RC_CHECK(RC_AT(s, 0), ==, 7);         // existing element is visible
    RC_AT(s, 1) = 100;                    // operate via the span as if fixed-size
    RC_AT(s, 2) = 101;
    RC_AT(s, 3) = 102;
    RC_CHECK(rc_array_int_get(&a, 3), ==, 102);
}

RC_TEST_STEP(array, resize_shrink, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_resize(&a, 5, &fix->a);
    rc_span_int s = rc_array_int_resize(&a, 2, &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(s.num, ==, 2u);              // span over the whole (shrunk) array
}

RC_TEST_STEP(array, reset, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    rc_array_int_push(&a, 2, &fix->a);
    uint32_t cap = a.cap;
    rc_array_int_reset(&a);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, cap);             // buffer retained
}

RC_TEST_STEP(array, deinit, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    rc_array_int_push(&a, 2, &fix->a);
    rc_array_int_deinit(&a, &fix->a);
    RC_CHECK(a.num, ==, 0u);
    RC_CHECK(a.cap, ==, 0u);
    RC_CHECK_TRUE(a.data == NULL);
    // deinit on an already-empty array is a no-op
    rc_array_int_deinit(&a, &fix->a);
    RC_CHECK_TRUE(a.data == NULL);
}

/* ---- insert / remove ---- */

RC_TEST_STEP(array, insert, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    rc_array_int_push(&a, 3, &fix->a);
    rc_array_int_insert(&a, 1, 2, &fix->a);    // {1, 2, 3}
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 1);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 2);
    RC_CHECK(rc_array_int_get(&a, 2), ==, 3);
    rc_array_int_insert(&a, 3, 4, &fix->a);    // insert at the end -> {1,2,3,4}
    RC_CHECK(rc_array_int_get(&a, 3), ==, 4);
}

RC_TEST_STEP(array, insert_n, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    rc_array_int_push(&a, 4, &fix->a);
    rc_array_int_insert_n(&a, 1, 2, &fix->a);  // {1, _, _, 4}; gap uninitialised
    RC_CHECK(a.num, ==, 4u);
    rc_array_int_set(&a, 1, 2);
    rc_array_int_set(&a, 2, 3);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 1);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 2);
    RC_CHECK(rc_array_int_get(&a, 2), ==, 3);
    RC_CHECK(rc_array_int_get(&a, 3), ==, 4);
}

RC_TEST_STEP(array, insert_n_zero, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    rc_array_int_push(&a, 4, &fix->a);              // {1, 4}
    rc_array_int_insert_n_zero(&a, 1, 2, &fix->a); // {1, 0, 0, 4}
    RC_CHECK(a.num, ==, 4u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 1);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 0);      // zeroed
    RC_CHECK(rc_array_int_get(&a, 2), ==, 0);      // zeroed
    RC_CHECK(rc_array_int_get(&a, 3), ==, 4);
}

RC_TEST_STEP(array, remove, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    for (int i = 0; i < 4; i++) rc_array_int_push(&a, i, &fix->a);   // {0,1,2,3}
    rc_array_int_remove(&a, 1);                                     // {0,2,3}
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 0);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 2);
    RC_CHECK(rc_array_int_get(&a, 2), ==, 3);
}

RC_TEST_STEP(array, remove_n, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    for (int i = 0; i < 5; i++) rc_array_int_push(&a, i, &fix->a);   // {0,1,2,3,4}
    rc_array_int_remove_n(&a, 1, 2);                                // {0,3,4}
    RC_CHECK(a.num, ==, 3u);
    RC_CHECK(rc_array_int_get(&a, 0), ==, 0);
    RC_CHECK(rc_array_int_get(&a, 1), ==, 3);
    RC_CHECK(rc_array_int_get(&a, 2), ==, 4);
}

/* ---- view / span macros and union conversions ---- */

RC_TEST(array, view_span_macros)
{
    int raw[] = {5, 6, 7};
    rc_view_int v = RC_VIEW(raw);
    RC_CHECK(v.num, ==, 3u);
    RC_CHECK(RC_AT(v, 2), ==, 7);

    rc_span_int s = RC_SPAN(raw);
    RC_CHECK(s.num, ==, 3u);
    RC_AT(s, 0) = 50;                // writes through to raw
    RC_CHECK(raw[0], ==, 50);

    rc_view_int sv = s.view;         // span -> view via the union
    RC_CHECK(RC_AT(sv, 0), ==, 50);
}

RC_TEST_STEP(array, array_to_span, fix)
{
    rc_array_int a = rc_array_int_make(0, &fix->a);
    rc_array_int_push(&a, 1, &fix->a);
    rc_span_int s = a.span;          // array -> span via the union
    RC_CHECK(s.num, ==, 1u);
    RC_AT(s, 0) = 77;                // writes through to the array
    RC_CHECK(rc_array_int_get(&a, 0), ==, 77);
    rc_view_int v = a.view;          // array -> view via the union
    RC_CHECK(RC_AT(v, 0), ==, 77);
}

/* ================= span operations ================= */

RC_TEST(span, make)
{
    int raw[] = {1, 2, 3};
    rc_span_int s = rc_span_int_make(raw, 3);   // wraps existing memory
    RC_CHECK(s.num, ==, 3u);
    RC_CHECK(rc_span_int_get(s, 0), ==, 1);
    rc_span_int_set(s, 0, 9);
    RC_CHECK(raw[0], ==, 9);                     // writes through to the memory
}

RC_TEST(span, is_valid_is_empty)
{
    int raw[] = {1, 2};
    rc_span_int s = rc_span_int_make(raw, 2);
    RC_CHECK_TRUE(rc_span_int_is_valid(s));
    RC_CHECK_FALSE(rc_span_int_is_empty(s));
    rc_span_int e = rc_span_int_make(NULL, 0);
    RC_CHECK_FALSE(rc_span_int_is_valid(e));
    RC_CHECK_TRUE(rc_span_int_is_empty(e));
}

RC_TEST(span, get_subspan)
{
    int raw[] = {0, 1, 2, 3, 4};
    rc_span_int s = RC_SPAN(raw);
    rc_span_int sub = rc_span_int_get_subspan(s, 1, 4);   // [1, 4) -> {1, 2, 3}
    RC_CHECK(sub.num, ==, 3u);
    RC_CHECK(rc_span_int_get(sub, 0), ==, 1);
    RC_CHECK(rc_span_int_get(sub, 2), ==, 3);
    RC_CHECK(rc_span_int_get_subspan(s, 3, 99).num, ==, 2u);  // end clamped
    RC_CHECK(rc_span_int_get_subspan(s, 4, 2).num, ==, 0u);   // end < start -> empty
}

RC_TEST(span, get_head)
{
    int raw[] = {0, 1, 2, 3, 4};
    rc_span_int s = RC_SPAN(raw);
    rc_span_int h = rc_span_int_get_head(s, 2);
    RC_CHECK(h.num, ==, 2u);
    RC_CHECK(rc_span_int_get(h, 0), ==, 0);
    RC_CHECK(rc_span_int_get_head(s, 99).num, ==, 5u);   // clamped
}

RC_TEST(span, get_tail)
{
    int raw[] = {0, 1, 2, 3, 4};
    rc_span_int s = RC_SPAN(raw);
    rc_span_int t = rc_span_int_get_tail(s, 2);          // from index 2 -> {2, 3, 4}
    RC_CHECK(t.num, ==, 3u);
    RC_CHECK(rc_span_int_get(t, 0), ==, 2);
    RC_CHECK(rc_span_int_get_tail(s, 99).num, ==, 0u);   // clamped
}

RC_TEST(span, get_set_at)
{
    int raw[] = {10, 20, 30};
    rc_span_int s = RC_SPAN(raw);
    RC_CHECK(rc_span_int_get(s, 1), ==, 20);
    rc_span_int_set(s, 1, 99);
    RC_CHECK(raw[1], ==, 99);             // writes through to the memory
    *rc_span_int_at(s, 2) = 77;
    RC_CHECK(raw[2], ==, 77);
}

RC_TEST(span, last_at)
{
    int raw[] = {5, 6, 7};
    rc_span_int s = RC_SPAN(raw);
    RC_CHECK_TRUE(rc_span_int_last_at(s) == &raw[2]);
    *rc_span_int_last_at(s) = 70;
    RC_CHECK(raw[2], ==, 70);
}

/* ================= view operations ================= */

RC_TEST(view, make)
{
    int raw[] = {1, 2, 3, 4};
    rc_view_int v = rc_view_int_make(raw, 4);   // wraps existing memory
    RC_CHECK(v.num, ==, 4u);
    RC_CHECK(rc_view_int_get(v, 0), ==, 1);
    RC_CHECK(rc_view_int_get(v, 3), ==, 4);
}

RC_TEST(view, is_valid_is_empty)
{
    int raw[] = {1, 2};
    rc_view_int v = rc_view_int_make(raw, 2);
    RC_CHECK_TRUE(rc_view_int_is_valid(v));
    RC_CHECK_FALSE(rc_view_int_is_empty(v));
    rc_view_int e = rc_view_int_make(NULL, 0);
    RC_CHECK_FALSE(rc_view_int_is_valid(e));
    RC_CHECK_TRUE(rc_view_int_is_empty(e));
}

RC_TEST(view, get_subview)
{
    int raw[] = {0, 1, 2, 3, 4};
    rc_view_int v = RC_VIEW(raw);
    rc_view_int sub = rc_view_int_get_subview(v, 1, 4);   // [1, 4) -> {1, 2, 3}
    RC_CHECK(sub.num, ==, 3u);
    RC_CHECK(rc_view_int_get(sub, 0), ==, 1);
    RC_CHECK(rc_view_int_get_subview(v, 3, 99).num, ==, 2u);  // end clamped
    RC_CHECK(rc_view_int_get_subview(v, 4, 2).num, ==, 0u);   // end < start -> empty
}

RC_TEST(view, get_head)
{
    int raw[] = {0, 1, 2, 3, 4};
    rc_view_int v = RC_VIEW(raw);
    rc_view_int h = rc_view_int_get_head(v, 2);
    RC_CHECK(h.num, ==, 2u);
    RC_CHECK(rc_view_int_get(h, 1), ==, 1);
    RC_CHECK(rc_view_int_get_head(v, 99).num, ==, 5u);   // clamped
}

RC_TEST(view, get_tail)
{
    int raw[] = {0, 1, 2, 3, 4};
    rc_view_int v = RC_VIEW(raw);
    rc_view_int t = rc_view_int_get_tail(v, 3);          // from index 3 -> {3, 4}
    RC_CHECK(t.num, ==, 2u);
    RC_CHECK(rc_view_int_get(t, 0), ==, 3);
    RC_CHECK(rc_view_int_get_tail(v, 99).num, ==, 0u);   // clamped
}

RC_TEST(view, get_at)
{
    int raw[] = {10, 20, 30};
    rc_view_int v = RC_VIEW(raw);
    RC_CHECK(rc_view_int_get(v, 1), ==, 20);
    RC_CHECK_TRUE(rc_view_int_at(v, 0) == &raw[0]);
}

RC_TEST(view, last_at)
{
    int raw[] = {5, 6, 7};
    rc_view_int v = RC_VIEW(raw);
    RC_CHECK_TRUE(rc_view_int_last_at(v) == &raw[2]);
    RC_CHECK(*rc_view_int_last_at(v), ==, 7);
}
