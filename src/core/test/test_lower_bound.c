#include "richc/test.h"

#define RC_ARRAY_TYPE int
#include "richc/template/array.h"

// plain int lower bound (default comparator)
#define RC_LOWER_BOUND_TYPE int
#include "richc/template/algorithm/lower_bound.h"

// custom comparator (no context): a key/value record sorted by key
typedef struct { int key; int val; } kv;
#define RC_ARRAY_TYPE kv
#include "richc/template/array.h"

#define RC_LOWER_BOUND_TYPE     kv
#define RC_LOWER_BOUND_CMP(a, b) ((a).key < (b).key)
#include "richc/template/algorithm/lower_bound.h"

// context comparator: ctx->sign flips the ordering so the same routine searches
// an ascending (sign +1) or descending (sign -1) array
typedef struct { int sign; } sign_ctx;
#define RC_LOWER_BOUND_TYPE          int
#define RC_LOWER_BOUND_CTX           sign_ctx
#define RC_LOWER_BOUND_CMP(c, a, b)  ((c)->sign * (a) < (c)->sign * (b))
#define RC_LOWER_BOUND_NAME          rc_lower_bound_signed
#include "richc/template/algorithm/lower_bound.h"

// context type with the default comparator (exercises the (void)ctx path)
#define RC_LOWER_BOUND_TYPE int
#define RC_LOWER_BOUND_CTX  sign_ctx
#define RC_LOWER_BOUND_NAME rc_lower_bound_ctxdef
#include "richc/template/algorithm/lower_bound.h"

RC_TEST(lower_bound, empty)
{
    rc_view_int v = {0};
    RC_CHECK(rc_lower_bound_int(v, 42), ==, 0u);
}

RC_TEST(lower_bound, single)
{
    int a[] = {10};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_lower_bound_int(v, 5), ==, 0u);    // before
    RC_CHECK(rc_lower_bound_int(v, 10), ==, 0u);   // equal -> the element itself
    RC_CHECK(rc_lower_bound_int(v, 11), ==, 1u);   // after -> past the end
}

RC_TEST(lower_bound, all_less)
{
    int a[] = {1, 2, 3, 4, 5};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_lower_bound_int(v, 100), ==, 5u);  // every element < value
}

RC_TEST(lower_bound, all_greater_equal)
{
    int a[] = {10, 20, 30};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_lower_bound_int(v, 0), ==, 0u);    // first element already >= value
    RC_CHECK(rc_lower_bound_int(v, 10), ==, 0u);
}

RC_TEST(lower_bound, general)
{
    int a[] = {2, 4, 6, 8, 10, 12};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_lower_bound_int(v, 1), ==, 0u);
    RC_CHECK(rc_lower_bound_int(v, 2), ==, 0u);
    RC_CHECK(rc_lower_bound_int(v, 5), ==, 2u);    // not present -> first > 5
    RC_CHECK(rc_lower_bound_int(v, 6), ==, 2u);    // present -> its index
    RC_CHECK(rc_lower_bound_int(v, 7), ==, 3u);
    RC_CHECK(rc_lower_bound_int(v, 12), ==, 5u);
    RC_CHECK(rc_lower_bound_int(v, 13), ==, 6u);
}

RC_TEST(lower_bound, duplicates)
{
    int a[] = {1, 3, 3, 3, 5, 5, 7};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_lower_bound_int(v, 3), ==, 1u);    // first of the run of 3s
    RC_CHECK(rc_lower_bound_int(v, 5), ==, 4u);    // first of the run of 5s
    RC_CHECK(rc_lower_bound_int(v, 4), ==, 4u);    // gap -> first element > 4
}

RC_TEST(lower_bound, custom_comparator)
{
    kv a[] = {{1, 100}, {3, 300}, {5, 500}, {5, 501}, {9, 900}};
    rc_view_kv v = RC_VIEW(a);
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 5}), ==, 2u);   // first key == 5
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 4}), ==, 2u);   // first key > 4
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 0}), ==, 0u);
    RC_CHECK(rc_lower_bound_kv(v, (kv) {.key = 10}), ==, 5u);
}

RC_TEST(lower_bound, context_ascending)
{
    int a[] = {10, 20, 30, 40, 50};
    rc_view_int v = RC_VIEW(a);
    sign_ctx ctx = {.sign = 1};
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 30), ==, 2u);
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 25), ==, 2u);
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 5), ==, 0u);
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 55), ==, 5u);
}

RC_TEST(lower_bound, context_descending)
{
    // sorted ascending under the sign=-1 comparison, i.e. descending values
    int a[] = {50, 40, 30, 20, 10};
    rc_view_int v = RC_VIEW(a);
    sign_ctx ctx = {.sign = -1};
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 30), ==, 2u);   // first value <= 30
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 35), ==, 2u);
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 55), ==, 0u);
    RC_CHECK(rc_lower_bound_signed(v, &ctx, 5), ==, 5u);
}

RC_TEST(lower_bound, context_default_comparator)
{
    int a[] = {2, 4, 6, 8};
    rc_view_int v = RC_VIEW(a);
    sign_ctx ctx = {.sign = 0};   // ignored by the default comparator
    RC_CHECK(rc_lower_bound_ctxdef(v, &ctx, 5), ==, 2u);
    RC_CHECK(rc_lower_bound_ctxdef(v, &ctx, 6), ==, 2u);
}
