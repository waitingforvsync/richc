#include "richc/test.h"

#define RC_ARRAY_TYPE int
#include "richc/template/array.h"

// plain int sort (default ascending comparator)
#define RC_SORT_TYPE int
#include "richc/template/algorithm/sort.h"

// custom comparator (no context): descending order
#define RC_SORT_TYPE     int
#define RC_SORT_CMP(a, b) ((a) > (b))
#define RC_SORT_NAME     rc_sort_desc
#include "richc/template/algorithm/sort.h"

// context comparator: ctx->sign chooses ascending (+1) or descending (-1)
typedef struct { int sign; } sign_ctx;
#define RC_SORT_TYPE          int
#define RC_SORT_CTX           sign_ctx
#define RC_SORT_CMP(c, a, b)  ((c)->sign * (a) < (c)->sign * (b))
#define RC_SORT_NAME          rc_sort_signed
#include "richc/template/algorithm/sort.h"

// context type with the default comparator (exercises the (void)ctx path)
#define RC_SORT_TYPE int
#define RC_SORT_CTX  sign_ctx
#define RC_SORT_NAME rc_sort_ctxdef
#include "richc/template/algorithm/sort.h"

static bool is_sorted_asc(rc_span_int s)
{
    for (uint32_t i = 1; i < s.num; i++) {
        if (s.data[i] < s.data[i - 1]) return false;
    }
    return true;
}

static bool is_sorted_desc(rc_span_int s)
{
    for (uint32_t i = 1; i < s.num; i++) {
        if (s.data[i] > s.data[i - 1]) return false;
    }
    return true;
}

RC_TEST(sort, empty)
{
    rc_span_int s = {0};
    rc_sort_int(s);   // must not crash
    RC_CHECK(s.num, ==, 0u);
}

RC_TEST(sort, single)
{
    int a[] = {42};
    rc_span_int s = RC_SPAN(a);
    rc_sort_int(s);
    RC_CHECK(a[0], ==, 42);
}

RC_TEST(sort, two)
{
    int a[] = {2, 1};
    rc_span_int s = RC_SPAN(a);
    rc_sort_int(s);
    RC_CHECK(a[0], ==, 1);
    RC_CHECK(a[1], ==, 2);

    int b[] = {5, 5};
    rc_span_int sb = RC_SPAN(b);
    rc_sort_int(sb);
    RC_CHECK(b[0], ==, 5);
    RC_CHECK(b[1], ==, 5);
}

RC_TEST(sort, already_sorted)
{
    int a[] = {1, 2, 3, 4, 5, 6, 7, 8};
    rc_span_int s = RC_SPAN(a);
    rc_sort_int(s);
    RC_CHECK_TRUE(is_sorted_asc(s));
    for (int i = 0; i < 8; i++) RC_CHECK(a[i], ==, i + 1);
}

RC_TEST(sort, reverse_sorted)
{
    int a[] = {8, 7, 6, 5, 4, 3, 2, 1};
    rc_span_int s = RC_SPAN(a);
    rc_sort_int(s);
    for (int i = 0; i < 8; i++) RC_CHECK(a[i], ==, i + 1);
}

RC_TEST(sort, all_equal)
{
    int a[] = {7, 7, 7, 7, 7, 7, 7};
    rc_span_int s = RC_SPAN(a);
    rc_sort_int(s);
    for (int i = 0; i < 7; i++) RC_CHECK(a[i], ==, 7);
}

RC_TEST(sort, duplicates)
{
    int a[] = {3, 1, 2, 3, 1, 2, 3, 1, 2, 5, 4, 5, 4};
    rc_span_int s = RC_SPAN(a);
    rc_sort_int(s);
    RC_CHECK_TRUE(is_sorted_asc(s));
    // the multiset is preserved: three 1s, three 2s, three 3s, two 4s, two 5s
    int expect[] = {1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5};
    for (uint32_t i = 0; i < s.num; i++) RC_CHECK(a[i], ==, expect[i]);
}

// Sorting around the insertion-sort threshold (16): just below, at, and above,
// so both the small-span path and the quicksort partition path are exercised.
RC_TEST(sort, threshold_boundary)
{
    for (uint32_t n = 14; n <= 40; n++) {
        int a[40];
        for (uint32_t i = 0; i < n; i++) a[i] = (int)(n - i);   // reverse order
        rc_span_int s = rc_span_int_make(a, n);
        rc_sort_int(s);
        RC_CHECK_TRUE(is_sorted_asc(s));
        for (uint32_t i = 0; i < n; i++) RC_CHECK(a[i], ==, (int)(i + 1));
    }
}

// A large shuffled permutation of 0..N-1.  Asserting data[i] == i afterwards
// proves the result is both sorted and a true permutation (no element dropped
// or duplicated), and the size drives the quicksort/heapsort path.
RC_TEST(sort, large_permutation)
{
    enum { N = 2000 };
    static int a[N];
    for (uint32_t i = 0; i < N; i++) a[i] = (int)i;

    // Fisher-Yates shuffle with a deterministic LCG (no global RNG needed).
    uint32_t state = 0x12345678u;
    for (uint32_t i = N - 1; i > 0; i--) {
        state = state * 1664525u + 1013904223u;
        uint32_t j = state % (i + 1u);
        int tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }

    rc_span_int s = rc_span_int_make(a, N);
    rc_sort_int(s);
    for (uint32_t i = 0; i < N; i++) RC_CHECK(a[i], ==, (int)i);
}

// Many small organ-pipe / sawtooth inputs: patterns that are awkward for
// median-of-three and can drive deeper recursion.
RC_TEST(sort, patterns)
{
    int organ[20];
    for (int i = 0; i < 10; i++) { organ[i] = i; organ[19 - i] = i; }
    rc_span_int s1 = RC_SPAN(organ);
    rc_sort_int(s1);
    RC_CHECK_TRUE(is_sorted_asc(s1));

    int saw[30];
    for (int i = 0; i < 30; i++) saw[i] = i % 5;
    rc_span_int s2 = RC_SPAN(saw);
    rc_sort_int(s2);
    RC_CHECK_TRUE(is_sorted_asc(s2));
}

RC_TEST(sort, custom_comparator_descending)
{
    int a[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
    rc_span_int s = RC_SPAN(a);
    rc_sort_desc(s);
    RC_CHECK_TRUE(is_sorted_desc(s));
    RC_CHECK(a[0], ==, 9);
    RC_CHECK(a[s.num - 1], ==, 1);
}

RC_TEST(sort, context_comparator)
{
    int a[] = {3, 1, 4, 1, 5, 9, 2, 6};
    rc_span_int s = RC_SPAN(a);

    sign_ctx asc = {.sign = 1};
    rc_sort_signed(s, &asc);
    RC_CHECK_TRUE(is_sorted_asc(s));

    sign_ctx desc = {.sign = -1};
    rc_sort_signed(s, &desc);
    RC_CHECK_TRUE(is_sorted_desc(s));
}

RC_TEST(sort, context_default_comparator)
{
    int a[] = {5, 2, 8, 1, 9, 3, 7, 4, 6, 0};
    rc_span_int s = RC_SPAN(a);
    sign_ctx ctx = {.sign = 0};   // ignored by the default comparator
    rc_sort_ctxdef(s, &ctx);
    for (int i = 0; i < 10; i++) RC_CHECK(a[i], ==, i);
}
