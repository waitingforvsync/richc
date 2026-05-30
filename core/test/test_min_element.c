#include "richc/test.h"

#define RC_ARRAY_TYPE int
#include "richc/template/array.h"

#define RC_MIN_ELEMENT_TYPE int
#include "richc/template/min_element.h"

// custom comparator (no context): a key/value record compared by key
typedef struct { int key; int val; } kv;
#define RC_ARRAY_TYPE kv
#include "richc/template/array.h"

#define RC_MIN_ELEMENT_TYPE     kv
#define RC_MIN_ELEMENT_CMP(a, b) ((a).key < (b).key)
#include "richc/template/min_element.h"

// context comparator: ctx->sign flips the ordering (sign -1 turns min into max)
typedef struct { int sign; } sign_ctx;
#define RC_MIN_ELEMENT_TYPE          int
#define RC_MIN_ELEMENT_CTX           sign_ctx
#define RC_MIN_ELEMENT_CMP(c, a, b)  ((c)->sign * (a) < (c)->sign * (b))
#define RC_MIN_ELEMENT_NAME          rc_min_element_signed
#include "richc/template/min_element.h"

RC_TEST(min_element, empty)
{
    rc_view_int v = {0};
    RC_CHECK(rc_min_element_int(v), ==, RC_INDEX_NONE);
}

RC_TEST(min_element, single)
{
    int a[] = {42};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_min_element_int(v), ==, 0u);
}

RC_TEST(min_element, general)
{
    int a[] = {5, 3, 8, 1, 9, 2};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_min_element_int(v), ==, 3u);   // value 1 at index 3
}

RC_TEST(min_element, leftmost_of_duplicates)
{
    int a[] = {3, 1, 4, 1, 5};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_min_element_int(v), ==, 1u);   // first 1, not the later one
}

RC_TEST(min_element, all_equal)
{
    int a[] = {7, 7, 7, 7};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_min_element_int(v), ==, 0u);
}

RC_TEST(min_element, custom_comparator)
{
    kv a[] = {{5, 50}, {3, 30}, {8, 80}, {3, 31}};
    rc_view_kv v = RC_VIEW(a);
    RC_CHECK(rc_min_element_kv(v), ==, 1u);    // first key == 3
}

RC_TEST(min_element, context_comparator)
{
    int a[] = {10, 40, 20, 40, 30};
    rc_view_int v = RC_VIEW(a);
    sign_ctx asc = {.sign = 1};
    RC_CHECK(rc_min_element_signed(v, &asc), ==, 0u);   // value 10
    sign_ctx desc = {.sign = -1};
    RC_CHECK(rc_min_element_signed(v, &desc), ==, 1u);  // sign flip -> first max (40)
}
