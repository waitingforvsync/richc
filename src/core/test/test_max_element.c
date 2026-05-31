#include "richc/test.h"

#define RC_ARRAY_TYPE int
#include "richc/template/array.h"

#define RC_MAX_ELEMENT_TYPE int
#include "richc/template/algorithm/max_element.h"

// custom comparator (no context): a key/value record compared by key
typedef struct { int key; int val; } kv;
#define RC_ARRAY_TYPE kv
#include "richc/template/array.h"

#define RC_MAX_ELEMENT_TYPE     kv
#define RC_MAX_ELEMENT_CMP(a, b) ((a).key < (b).key)
#include "richc/template/algorithm/max_element.h"

// context comparator: ctx->sign flips the ordering (sign -1 turns max into min)
typedef struct { int sign; } sign_ctx;
#define RC_MAX_ELEMENT_TYPE          int
#define RC_MAX_ELEMENT_CTX           sign_ctx
#define RC_MAX_ELEMENT_CMP(c, a, b)  ((c)->sign * (a) < (c)->sign * (b))
#define RC_MAX_ELEMENT_NAME          rc_max_element_signed
#include "richc/template/algorithm/max_element.h"

RC_TEST(max_element, empty)
{
    rc_view_int v = {0};
    RC_CHECK(rc_max_element_int(v), ==, RC_INDEX_NONE);
}

RC_TEST(max_element, single)
{
    int a[] = {42};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_max_element_int(v), ==, 0u);
}

RC_TEST(max_element, general)
{
    int a[] = {5, 3, 8, 1, 9, 2};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_max_element_int(v), ==, 4u);   // value 9 at index 4
}

RC_TEST(max_element, leftmost_of_duplicates)
{
    int a[] = {3, 9, 4, 9, 5};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_max_element_int(v), ==, 1u);   // first 9, not the later one
}

RC_TEST(max_element, all_equal)
{
    int a[] = {7, 7, 7, 7};
    rc_view_int v = RC_VIEW(a);
    RC_CHECK(rc_max_element_int(v), ==, 0u);
}

RC_TEST(max_element, custom_comparator)
{
    kv a[] = {{5, 50}, {8, 80}, {3, 30}, {8, 81}};
    rc_view_kv v = RC_VIEW(a);
    RC_CHECK(rc_max_element_kv(v), ==, 1u);    // first key == 8
}

RC_TEST(max_element, context_comparator)
{
    int a[] = {10, 40, 20, 40, 30};
    rc_view_int v = RC_VIEW(a);
    sign_ctx asc = {.sign = 1};
    RC_CHECK(rc_max_element_signed(v, &asc), ==, 1u);   // first max (40)
    sign_ctx desc = {.sign = -1};
    RC_CHECK(rc_max_element_signed(v, &desc), ==, 0u);  // sign flip -> min (10)
}
