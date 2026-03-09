/*
 * test.c - correctness tests for lower_bound, upper_bound, sort,
 *          bounds-checked access, platform virtual memory, rc_arena, array,
 *          transform, find, remove, hash_map, hash_trie, accumulate, and str.
 *
 * array.h is the combined header: one inclusion defines the rc_view, rc_span, and
 * Array types for a given element type, and also provides RC_VIEW_AT / RC_SPAN_AT
 * and the RC_AS_VIEW / RC_AS_SPAN conversion macros.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "richc/platform.h"
#include "richc/arena.h"
#include "richc/bitset.h"
#include "richc/hash.h"
#include "richc/str.h"
#include "richc/mstr.h"
#include "richc/math/math.h"
#include "richc/math/vec2i.h"
#include "richc/math/vec3i.h"
#include "richc/math/vec2f.h"
#include "richc/math/vec3f.h"
#include "richc/math/vec4f.h"
#include "richc/math/box2f.h"
#include "richc/math/box2i.h"
#include "richc/math/mat22f.h"
#include "richc/math/mat23f.h"
#include "richc/math/mat33f.h"
#include "richc/math/mat34f.h"
#include "richc/math/mat44f.h"
#include "richc/math/quatf.h"
#include "richc/math/rational.h"
#include "richc/math/bigint.h"
#include "richc/file.h"
#include "richc/bytes.h"

/* ---- instantiate all types and algorithms for int ---- */

#define ARRAY_T int          /* defines rc_view_int, rc_span_int, rc_array_int        */
#include "richc/template/array.h"           /* also defines RC_VIEW_AT, RC_SPAN_AT, RICHC_AS_*    */

#define LOWER_BOUND_T int
#include "richc/template/lower_bound.h"

#define UPPER_BOUND_T int
#include "richc/template/upper_bound.h"

#define SORT_T int
#include "richc/template/sort.h"

/* ---- also instantiate for a struct to exercise custom comparators ---- */

typedef struct { int key; int payload; } Record;

#define ARRAY_T Record        /* defines rc_view_Record, rc_span_Record, rc_array_Record */
#include "richc/template/array.h"

#define LOWER_BOUND_T         Record
#define LOWER_BOUND_CMP(a, b) ((a).key < (b).key)
#include "richc/template/lower_bound.h"

#define UPPER_BOUND_T         Record
#define UPPER_BOUND_CMP(a, b) ((a).key < (b).key)
#include "richc/template/upper_bound.h"

#define SORT_T          Record
#define SORT_CMP(a, b)  ((a).key < (b).key)
#include "richc/template/sort.h"

/* ---- context-aware instantiations ---- */

/*
 * IntCmpCtx: drives ascending or descending comparison.
 * Used to test all three context-aware templates with a single comparator
 * that meaningfully differs from the default (a) < (b).
 */
typedef struct { int ascending; } IntCmpCtx;

#define SORT_T   int
#define SORT_CTX IntCmpCtx
#define SORT_CMP(ctx, a, b)  ((ctx)->ascending ? (a) < (b) : (b) < (a))
#define SORT_NAME int_sort_ctx
#include "richc/template/sort.h"

#define LOWER_BOUND_T    int
#define LOWER_BOUND_CTX  IntCmpCtx
#define LOWER_BOUND_CMP(ctx, a, b)  ((ctx)->ascending ? (a) < (b) : (b) < (a))
#define LOWER_BOUND_NAME int_lower_bound_ctx
#include "richc/template/lower_bound.h"

#define UPPER_BOUND_T    int
#define UPPER_BOUND_CTX  IntCmpCtx
#define UPPER_BOUND_CMP(ctx, a, b)  ((ctx)->ascending ? (a) < (b) : (b) < (a))
#define UPPER_BOUND_NAME int_upper_bound_ctx
#include "richc/template/upper_bound.h"

/* Default CMP with context: ctx defined, no CMP supplied.
 * Verifies that the default (a)<(b) path compiles without warnings. */
#define SORT_T    int
#define SORT_CTX  IntCmpCtx
#define SORT_NAME int_sort_default_ctx
#include "richc/template/sort.h"

/* ---- min_element / max_element instantiations ---- */

#define MIN_ELEMENT_T int
#include "richc/template/min_element.h"
/* defines: uint32_t rc_min_element_int(rc_view_int view) */

#define MAX_ELEMENT_T int
#include "richc/template/max_element.h"
/* defines: uint32_t rc_max_element_int(rc_view_int view) */

/* Record: compare by .key field */
#define MIN_ELEMENT_T               Record
#define MIN_ELEMENT_CMP(a, b)       ((a).key < (b).key)
#define MIN_ELEMENT_NAME            record_min_element
#include "richc/template/min_element.h"

#define MAX_ELEMENT_T               Record
#define MAX_ELEMENT_CMP(a, b)       ((a).key < (b).key)
#define MAX_ELEMENT_NAME            record_max_element
#include "richc/template/max_element.h"

/* Context-aware: reuse IntCmpCtx (ascending/descending toggle) */
#define MIN_ELEMENT_T               int
#define MIN_ELEMENT_CTX             IntCmpCtx
#define MIN_ELEMENT_CMP(ctx, a, b)  ((ctx)->ascending ? (a) < (b) : (b) < (a))
#define MIN_ELEMENT_NAME            int_min_element_ctx
#include "richc/template/min_element.h"

#define MAX_ELEMENT_T               int
#define MAX_ELEMENT_CTX             IntCmpCtx
#define MAX_ELEMENT_CMP(ctx, a, b)  ((ctx)->ascending ? (a) < (b) : (b) < (a))
#define MAX_ELEMENT_NAME            int_max_element_ctx
#include "richc/template/max_element.h"

/* Default CMP with context: verify the (void)ctx path compiles cleanly. */
#define MIN_ELEMENT_T    int
#define MIN_ELEMENT_CTX  IntCmpCtx
#define MIN_ELEMENT_NAME int_min_element_default_ctx
#include "richc/template/min_element.h"

#define MAX_ELEMENT_T    int
#define MAX_ELEMENT_CTX  IntCmpCtx
#define MAX_ELEMENT_NAME int_max_element_default_ctx
#include "richc/template/max_element.h"

/* ---- find instantiations ---- */

#define FIND_T int
#include "richc/template/find.h"
/* defines: size_t rc_find_int(rc_view_int view, int value) */

#define FIND_T           Record
#define FIND_PRED(e, v)  ((e).key == (v).key)
#include "richc/template/find.h"
/* defines: size_t rc_find_Record(rc_view_Record view, Record value) */

/* Context predicate: find within ±tolerance of value. */
typedef struct { int tolerance; } TolCtx;

#define FIND_T               int
#define FIND_CTX             TolCtx
#define FIND_PRED(ctx, e, v) ((e) >= (v) - (ctx)->tolerance && \
                              (e) <= (v) + (ctx)->tolerance)
#define FIND_NAME            int_find_tol
#include "richc/template/find.h"
/* defines: size_t int_find_tol(rc_view_int view, TolCtx *ctx, int value) */

/* Default PRED with context: verifies the (void)ctx path compiles cleanly. */
#define FIND_T    int
#define FIND_CTX  TolCtx
#define FIND_NAME int_find_default_ctx
#include "richc/template/find.h"

/* ---- remove instantiations ---- */

/* Remove negative ints; no context. */
#define REMOVE_T       int
#define REMOVE_PRED(e) ((e) < 0)
#define REMOVE_NAME    int_remove_negative
#include "richc/template/remove.h"
/* defines: uint32_t int_remove_negative(rc_span_int *span) */

/* Remove Records with even key; no context. */
#define REMOVE_T            Record
#define REMOVE_PRED(e)      (((e).key % 2) == 0)
#define REMOVE_NAME         record_remove_even_key
#include "richc/template/remove.h"
/* defines: uint32_t record_remove_even_key(rc_span_Record *span) */

/* Remove ints below a threshold supplied via context. */
typedef struct { int threshold; } ThreshCtx;

#define REMOVE_T            int
#define REMOVE_CTX          ThreshCtx
#define REMOVE_PRED(ctx, e) ((e) < (ctx)->threshold)
#define REMOVE_NAME         int_remove_below
#include "richc/template/remove.h"
/* defines: uint32_t int_remove_below(rc_span_int *span, ThreshCtx *ctx) */

/* ---- rotate instantiations ---- */

#define ROTATE_T int
#include "richc/template/rotate.h"
/* defines: void rc_rotate_int(rc_span_int span, uint32_t k) */

#define ROTATE_T    Record
#define ROTATE_NAME record_rotate
#include "richc/template/rotate.h"
/* defines: void record_rotate(rc_span_Record span, uint32_t k) */

/* ---- all_of / any_of / none_of instantiations ---- */

/* Plain int, no context. */
#define ALL_OF_T       int
#define ALL_OF_PRED(e) ((e) > 0)
#define ALL_OF_NAME    int_all_positive
#include "richc/template/all_of.h"
/* defines: bool int_all_positive(rc_view_int view) */

#define ANY_OF_T       int
#define ANY_OF_PRED(e) ((e) > 0)
#define ANY_OF_NAME    int_any_positive
#include "richc/template/any_of.h"
/* defines: bool int_any_positive(rc_view_int view) */

#define NONE_OF_T       int
#define NONE_OF_PRED(e) ((e) > 0)
#define NONE_OF_NAME    int_none_positive
#include "richc/template/none_of.h"
/* defines: bool int_none_positive(rc_view_int view) */

/* Context: ThreshCtx reused from remove instantiations above. */
#define ALL_OF_T               int
#define ALL_OF_CTX             ThreshCtx
#define ALL_OF_PRED(ctx, e)    ((e) >= (ctx)->threshold)
#define ALL_OF_NAME            int_all_at_least
#include "richc/template/all_of.h"
/* defines: bool int_all_at_least(rc_view_int view, ThreshCtx *ctx) */

#define ANY_OF_T               int
#define ANY_OF_CTX             ThreshCtx
#define ANY_OF_PRED(ctx, e)    ((e) >= (ctx)->threshold)
#define ANY_OF_NAME            int_any_at_least
#include "richc/template/any_of.h"
/* defines: bool int_any_at_least(rc_view_int view, ThreshCtx *ctx) */

#define NONE_OF_T               int
#define NONE_OF_CTX             ThreshCtx
#define NONE_OF_PRED(ctx, e)    ((e) >= (ctx)->threshold)
#define NONE_OF_NAME            int_none_at_least
#include "richc/template/none_of.h"
/* defines: bool int_none_at_least(rc_view_int view, ThreshCtx *ctx) */

/* ---- mismatch instantiations ---- */

/* Plain int, default equality. */
#define MISMATCH_T int
#include "richc/template/mismatch.h"
/* defines: uint32_t rc_mismatch_int(rc_view_int view1, rc_view_int view2) */

/* Record: match when keys are equal. */
#define MISMATCH_T            Record
#define MISMATCH_PRED(a, b)   ((a).key == (b).key)
#define MISMATCH_NAME         record_mismatch_by_key
#include "richc/template/mismatch.h"
/* defines: uint32_t record_mismatch_by_key(rc_view_Record v1, rc_view_Record v2) */

/* Context: TolCtx reused from find instantiations — match within tolerance. */
#define MISMATCH_T                int
#define MISMATCH_CTX              TolCtx
#define MISMATCH_PRED(ctx, a, b)  (((a) - (b)) <= (ctx)->tolerance && \
                                   ((b) - (a)) <= (ctx)->tolerance)
#define MISMATCH_NAME             int_mismatch_tol
#include "richc/template/mismatch.h"
/* defines: uint32_t int_mismatch_tol(rc_view_int v1, rc_view_int v2, TolCtx *ctx) */

/* Default PRED with context: verifies the (void)ctx path compiles cleanly. */
#define MISMATCH_T    int
#define MISMATCH_CTX  TolCtx
#define MISMATCH_NAME int_mismatch_default_ctx
#include "richc/template/mismatch.h"

/* ---- transform instantiations ---- */

#define TRANSFORM_SRC_T   int
#define TRANSFORM_DST_T   int
#define TRANSFORM_FUNC(e) ((e) * (e))
#define TRANSFORM_NAME    int_square
#include "richc/template/transform.h"
/* defines: size_t int_square(rc_view_int src, rc_array_int *dst, rc_arena *a) */

#define TRANSFORM_SRC_T   int
#define TRANSFORM_DST_T   Record
#define TRANSFORM_FUNC(e) ((Record) {.key = (e), .payload = (e) * 10})
#define TRANSFORM_NAME    int_to_record
#include "richc/template/transform.h"
/* defines: size_t int_to_record(rc_view_int src, rc_array_Record *dst, rc_arena *a) */

typedef struct { int factor; } ScaleCtx;

#define TRANSFORM_SRC_T        int
#define TRANSFORM_DST_T        int
#define TRANSFORM_CTX          ScaleCtx
#define TRANSFORM_FUNC(ctx, e) ((e) * (ctx)->factor)
#define TRANSFORM_NAME         int_scale
#include "richc/template/transform.h"
/* defines: size_t int_scale(rc_view_int src, rc_array_int *dst, ScaleCtx *ctx, rc_arena *a) */

/* Default FUNC with context: verifies the identity/(void)ctx path compiles cleanly. */
#define TRANSFORM_SRC_T   int
#define TRANSFORM_DST_T   int
#define TRANSFORM_CTX     ScaleCtx
#define TRANSFORM_NAME    int_transform_default_ctx
#include "richc/template/transform.h"

/* ---- accumulate instantiations ---- */

/* Default addition: int sum */
#define ACCUM_T        int
#define ACCUM_RESULT_T int
#include "richc/template/accumulate.h"
/* defines: int rc_accumulate_int(rc_view_int view, int init) */

/* Custom func: running product */
#define ACCUM_T            int
#define ACCUM_RESULT_T     int
#define ACCUM_FUNC(acc, e) ((acc) * (e))
#define ACCUM_NAME         int_product
#include "richc/template/accumulate.h"
/* defines: int int_product(rc_view_int view, int init) */

/* Heterogeneous types: sum int elements into a double accumulator */
#define ACCUM_T        int
#define ACCUM_RESULT_T double
#define ACCUM_NAME     int_sum_to_double
#include "richc/template/accumulate.h"
/* defines: double int_sum_to_double(rc_view_int view, double init) */

/* Record: extract and sum keys into an int accumulator */
#define ACCUM_T            Record
#define ACCUM_RESULT_T     int
#define ACCUM_FUNC(acc, e) ((acc) + (e).key)
#define ACCUM_NAME         Record_key_sum
#include "richc/template/accumulate.h"
/* defines: int Record_key_sum(rc_view_Record view, int init) */

/* Context: weighted sum (each element multiplied by ctx->weight) */
typedef struct { int weight; } WeightCtx;

#define ACCUM_T                 int
#define ACCUM_RESULT_T          int
#define ACCUM_CTX               WeightCtx
#define ACCUM_FUNC(ctx, acc, e) ((acc) + (e) * (ctx)->weight)
#define ACCUM_NAME              int_weighted_sum
#include "richc/template/accumulate.h"
/* defines: int int_weighted_sum(rc_view_int view, WeightCtx *ctx, int init) */

/* Default FUNC with context: verifies the (void)ctx path compiles cleanly. */
#define ACCUM_T        int
#define ACCUM_RESULT_T int
#define ACCUM_CTX      WeightCtx
#define ACCUM_NAME     int_accumulate_default_ctx
#include "richc/template/accumulate.h"

/* ---- hash trie instantiations ---- */

/*
 * U64IntTrie: uint64_t → int, identity hash (key is already a 64-bit hash).
 * Default equality (==).
 */
#define TRIE_KEY_T   uint64_t
#define TRIE_VAL_T   int
#define TRIE_HASH(k) (k)
#define TRIE_NAME    U64IntTrie
#include "richc/template/hash_trie.h"
/* defines: U64IntTrie_node, U64IntTrie_pool, U64IntTrie,
 *          U64IntTrie_create, U64IntTrie_find, U64IntTrie_add,
 *          U64IntTrie_delete                                     */

/* ---- hash map instantiations ---- */

/*
 * rc_map_int: int → int.
 */
#define MAP_KEY_T   int
#define MAP_VAL_T   int
#define MAP_HASH(k) rc_hash_i32(k)
#include "richc/template/hash_map.h"
/* defines: rc_map_int, rc_map_int_add, rc_map_int_remove, rc_map_int_find,
 *          rc_map_int_contains, rc_map_int_reserve                    */

/*
 * int_Record_Map: int → Record.
 * Same hash function; explicit MAP_NAME required because MAP_KEY_T is
 * int again (the default rc_map_int would collide with the previous one).
 */
#define MAP_KEY_T   int
#define MAP_VAL_T   Record
#define MAP_HASH(k) rc_hash_i32(k)
#define MAP_NAME    int_Record_Map
#include "richc/template/hash_map.h"
/* defines: int_Record_Map, int_Record_Map_add, etc. */

/*
 * Record_int_Map: Record → int, custom hash and equality on .key field.
 */
#undef MAP_NAME
#define MAP_KEY_T         Record
#define MAP_VAL_T         int
#define MAP_HASH(k)       rc_hash_i32((k).key)
#define MAP_EQUAL(a, b)   ((a).key == (b).key)
#define MAP_NAME          Record_int_Map
#include "richc/template/hash_map.h"
/* defines: Record_int_Map, Record_int_Map_add, etc. */

/*
 * rc_set_int: int set.
 */
#define SET_KEY_T   int
#define SET_HASH(k) rc_hash_i32(k)
#include "richc/template/hash_set.h"
/* defines: rc_set_int, rc_set_int_add, rc_set_int_remove,
 *          rc_set_int_contains, rc_set_int_reserve              */

/*
 * Record_set: Record set, custom hash and equality on .key field.
 */
#define SET_KEY_T         Record
#define SET_HASH(k)       rc_hash_i32((k).key)
#define SET_EQUAL(a, b)   ((a).key == (b).key)
#define SET_NAME          Record_set
#include "richc/template/hash_set.h"
/* defines: Record_set, Record_set_add, etc. */

/* ---- hash multimap instantiations ---- */

/*
 * rc_mmap_int: int → int.
 */
#define MMAP_KEY_T   int
#define MMAP_VAL_T   int
#define MMAP_HASH(k) rc_hash_i32(k)
#include "richc/template/hash_multimap.h"
/* defines: rc_mmap_int, rc_mmap_int_pool, rc_mmap_int_node,
 *          rc_mmap_int_pool_make, rc_mmap_int_make,
 *          rc_mmap_int_add, rc_mmap_int_remove_all, rc_mmap_int_find_head,
 *          rc_mmap_int_contains, rc_mmap_int_reserve, rc_mmap_int_pool_reserve,
 *          rc_mmap_int_next, rc_mmap_int_key_at, rc_mmap_int_head_at,
 *          rc_mmap_int_node_val, rc_mmap_int_node_next               */

/* ---- minimal test framework ---- */

static int g_failures = 0;
static int g_total    = 0;
static int g_group_failures = 0;

#define ASSERT(cond) do {                                              \
    g_total++;                                                         \
    if (!(cond)) {                                                    \
        fprintf(stderr, "    FAIL %s:%d: %s\n",                       \
                __FILE__, __LINE__, #cond);                            \
        g_failures++;                                                  \
        g_group_failures++;                                            \
    }                                                                  \
} while (0)

#define BEGIN_GROUP(name) do { g_group_failures = 0; printf("  %-36s", name); } while(0)
#define END_GROUP()       do { puts(g_group_failures ? "FAIL" : "pass"); } while (0)

/* ---- helpers ---- */

static int ints_sorted(const int *a, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (a[i] < a[i - 1]) return 0;
    return 1;
}

static int records_sorted_by_key(const Record *a, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (a[i].key < a[i - 1].key) return 0;
    return 1;
}

/* ---- lower_bound ---- */

static void test_lower_bound(void)
{
    printf("lower_bound\n");

    BEGIN_GROUP("empty view");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(rc_lower_bound_int(v, 0) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("single element");
    {
        int arr[] = {5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_lower_bound_int(v, 4) == 0); /* below  → index 0 (first element) */
        ASSERT(rc_lower_bound_int(v, 5) == 0); /* equal  → index 0                 */
        ASSERT(rc_lower_bound_int(v, 6) == 1); /* above  → past end                */
    }
    END_GROUP();

    BEGIN_GROUP("sorted, search at every gap");
    {
        int arr[] = {1, 3, 5, 7, 9};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_lower_bound_int(v,  0) == 0); /* below all            */
        ASSERT(rc_lower_bound_int(v,  1) == 0); /* on first element     */
        ASSERT(rc_lower_bound_int(v,  2) == 1); /* gap between 1 and 3  */
        ASSERT(rc_lower_bound_int(v,  3) == 1);
        ASSERT(rc_lower_bound_int(v,  4) == 2); /* gap between 3 and 5  */
        ASSERT(rc_lower_bound_int(v,  5) == 2);
        ASSERT(rc_lower_bound_int(v,  6) == 3);
        ASSERT(rc_lower_bound_int(v,  7) == 3);
        ASSERT(rc_lower_bound_int(v,  8) == 4);
        ASSERT(rc_lower_bound_int(v,  9) == 4); /* on last element      */
        ASSERT(rc_lower_bound_int(v, 10) == 5); /* above all → past end */
    }
    END_GROUP();

    BEGIN_GROUP("repeated values: first occurrence");
    {
        int arr[] = {1, 3, 3, 3, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_lower_bound_int(v, 3) == 1); /* first of three 3s     */
        ASSERT(rc_lower_bound_int(v, 2) == 1); /* gap just before 3s    */
        ASSERT(rc_lower_bound_int(v, 4) == 4); /* gap just after 3s     */
    }
    END_GROUP();

    BEGIN_GROUP("all elements equal");
    {
        int arr[] = {7, 7, 7, 7};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_lower_bound_int(v, 6) == 0); /* below → index 0       */
        ASSERT(rc_lower_bound_int(v, 7) == 0); /* equal → first element */
        ASSERT(rc_lower_bound_int(v, 8) == 4); /* above → past end      */
    }
    END_GROUP();
}

/* ---- upper_bound ---- */

static void test_upper_bound(void)
{
    printf("upper_bound\n");

    BEGIN_GROUP("empty view");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(rc_upper_bound_int(v, 0) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("single element");
    {
        int arr[] = {5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_upper_bound_int(v, 4) == 0); /* below → first element > 4 is at 0 */
        ASSERT(rc_upper_bound_int(v, 5) == 1); /* equal → no element > 5, past end  */
        ASSERT(rc_upper_bound_int(v, 6) == 1); /* above → no element > 6, past end  */
    }
    END_GROUP();

    BEGIN_GROUP("sorted, search at every gap");
    {
        int arr[] = {1, 3, 5, 7, 9};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_upper_bound_int(v,  0) == 0); /* below all             */
        ASSERT(rc_upper_bound_int(v,  1) == 1); /* one past first        */
        ASSERT(rc_upper_bound_int(v,  2) == 1); /* gap between 1 and 3   */
        ASSERT(rc_upper_bound_int(v,  3) == 2);
        ASSERT(rc_upper_bound_int(v,  4) == 2);
        ASSERT(rc_upper_bound_int(v,  5) == 3);
        ASSERT(rc_upper_bound_int(v,  6) == 3);
        ASSERT(rc_upper_bound_int(v,  7) == 4);
        ASSERT(rc_upper_bound_int(v,  8) == 4);
        ASSERT(rc_upper_bound_int(v,  9) == 5); /* one past last         */
        ASSERT(rc_upper_bound_int(v, 10) == 5); /* above all → past end  */
    }
    END_GROUP();

    BEGIN_GROUP("repeated values: one past last");
    {
        int arr[] = {1, 3, 3, 3, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_upper_bound_int(v, 3) == 4); /* one past last 3     */
        ASSERT(rc_upper_bound_int(v, 2) == 1); /* before the 3s       */
        ASSERT(rc_upper_bound_int(v, 4) == 4); /* after 3s, before 5  */
    }
    END_GROUP();

    BEGIN_GROUP("all elements equal");
    {
        int arr[] = {7, 7, 7, 7};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_upper_bound_int(v, 6) == 0); /* below → first element > 6 is at 0 */
        ASSERT(rc_upper_bound_int(v, 7) == 4); /* equal → past end                   */
        ASSERT(rc_upper_bound_int(v, 8) == 4); /* above → past end                   */
    }
    END_GROUP();

    BEGIN_GROUP("[lower, upper) gives equal range");
    {
        /* value present with multiplicity */
        int arr[] = {1, 3, 3, 3, 5, 5, 9};
        rc_view_int v = RC_VIEW(arr);
        size_t lo, hi;

        lo = rc_lower_bound_int(v, 3); hi = rc_upper_bound_int(v, 3);
        ASSERT(lo == 1 && hi == 4); /* three 3s at indices 1,2,3 */

        lo = rc_lower_bound_int(v, 5); hi = rc_upper_bound_int(v, 5);
        ASSERT(lo == 4 && hi == 6); /* two 5s at indices 4,5     */

        /* value absent: lower == upper (empty range) */
        lo = rc_lower_bound_int(v, 4); hi = rc_upper_bound_int(v, 4);
        ASSERT(lo == hi);

        lo = rc_lower_bound_int(v, 0); hi = rc_upper_bound_int(v, 0);
        ASSERT(lo == hi && lo == 0);

        lo = rc_lower_bound_int(v, 10); hi = rc_upper_bound_int(v, 10);
        ASSERT(lo == hi && lo == 7);
    }
    END_GROUP();
}

/* ---- min_element / max_element ---- */

static void test_min_max_element(void)
{
    printf("min_element / max_element\n");

    BEGIN_GROUP("empty view: returns RC_INDEX_NONE");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(rc_min_element_int(v) == RC_INDEX_NONE);
        ASSERT(rc_max_element_int(v) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("single element: index 0");
    {
        int arr[] = {42};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 0);
        ASSERT(rc_max_element_int(v) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("two elements: ascending");
    {
        int arr[] = {3, 7};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 0);
        ASSERT(rc_max_element_int(v) == 1);
    }
    END_GROUP();

    BEGIN_GROUP("two elements: descending");
    {
        int arr[] = {7, 3};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 1);
        ASSERT(rc_max_element_int(v) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("min/max at beginning and end");
    {
        int arr[] = {1, 5, 3, 9, 2};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 0);   /* 1 at index 0 */
        ASSERT(rc_max_element_int(v) == 3);   /* 9 at index 3 */
    }
    END_GROUP();

    BEGIN_GROUP("min/max in middle");
    {
        int arr[] = {4, 1, 6, 2, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 1);   /* 1 at index 1 */
        ASSERT(rc_max_element_int(v) == 2);   /* 6 at index 2 */
    }
    END_GROUP();

    BEGIN_GROUP("all equal: returns index 0 (first)");
    {
        int arr[] = {5, 5, 5, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 0);
        ASSERT(rc_max_element_int(v) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("duplicate extremes: returns first occurrence");
    {
        int arr[] = {3, 1, 5, 1, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 1);   /* first 1 at index 1 */
        ASSERT(rc_max_element_int(v) == 2);   /* first 5 at index 2 */
    }
    END_GROUP();

    BEGIN_GROUP("negative values");
    {
        int arr[] = {-3, -1, -7, -2};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_min_element_int(v) == 2);   /* -7 at index 2 */
        ASSERT(rc_max_element_int(v) == 1);   /* -1 at index 1 */
    }
    END_GROUP();

    BEGIN_GROUP("custom type: Record compared by .key");
    {
        Record arr[] = {{5,0},{2,0},{8,0},{1,0},{6,0}};
        rc_view_Record v = RC_VIEW(arr);
        ASSERT(record_min_element(v) == 3);   /* key=1 at index 3 */
        ASSERT(record_max_element(v) == 2);   /* key=8 at index 2 */
    }
    END_GROUP();

    BEGIN_GROUP("context: ascending order");
    {
        int arr[] = {4, 1, 7, 2};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {1};
        ASSERT(int_min_element_ctx(v, &ctx) == 1);   /* 1 at index 1 */
        ASSERT(int_max_element_ctx(v, &ctx) == 2);   /* 7 at index 2 */
    }
    END_GROUP();

    BEGIN_GROUP("context: descending order (inverted comparator)");
    {
        int arr[] = {4, 1, 7, 2};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {0};   /* descending: b < a means a is "less" */
        ASSERT(int_min_element_ctx(v, &ctx) == 2);   /* 7 is min under desc */
        ASSERT(int_max_element_ctx(v, &ctx) == 1);   /* 1 is max under desc */
    }
    END_GROUP();

    BEGIN_GROUP("default CMP with context: (void)ctx path");
    {
        int arr[] = {3, 1, 4, 1, 5};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {1};
        ASSERT(int_min_element_default_ctx(v, &ctx) == 1);
        ASSERT(int_max_element_default_ctx(v, &ctx) == 4);
    }
    END_GROUP();
}

/* ---- sort (int) ---- */

static void test_sort_int(void)
{
    printf("sort (int)\n");

    BEGIN_GROUP("empty span");
    {
        rc_span_int s = {.data = NULL, .num = 0};
        rc_sort_int(s); /* must not crash */
        ASSERT(1);
    }
    END_GROUP();

    BEGIN_GROUP("single element");
    {
        int arr[] = {42};
        rc_span_int s = RC_SPAN(arr);
        rc_sort_int(s);
        ASSERT(arr[0] == 42);
    }
    END_GROUP();

    BEGIN_GROUP("two elements");
    {
        int a[] = {1, 2}; rc_sort_int((rc_span_int) {.data = a, .num = 2}); ASSERT(ints_sorted(a, 2));
        int b[] = {2, 1}; rc_sort_int((rc_span_int) {.data = b, .num = 2}); ASSERT(ints_sorted(b, 2));
        int c[] = {5, 5}; rc_sort_int((rc_span_int) {.data = c, .num = 2}); ASSERT(ints_sorted(c, 2));
    }
    END_GROUP();

    BEGIN_GROUP("already sorted");
    {
        int arr[] = {1, 2, 3, 4, 5, 6, 7, 8};
        rc_span_int s = RC_SPAN(arr);
        rc_sort_int(s);
        ASSERT(ints_sorted(arr, 8));
    }
    END_GROUP();

    BEGIN_GROUP("reverse sorted");
    {
        int arr[] = {8, 7, 6, 5, 4, 3, 2, 1};
        rc_span_int s = RC_SPAN(arr);
        rc_sort_int(s);
        ASSERT(ints_sorted(arr, 8));
        for (int i = 0; i < 8; i++) ASSERT(arr[i] == i + 1);
    }
    END_GROUP();

    BEGIN_GROUP("all equal");
    {
        int arr[] = {3, 3, 3, 3, 3};
        rc_span_int s = RC_SPAN(arr);
        rc_sort_int(s);
        ASSERT(ints_sorted(arr, 5));
        for (int i = 0; i < 5; i++) ASSERT(arr[i] == 3);
    }
    END_GROUP();

    BEGIN_GROUP("repeated values");
    {
        int arr[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 2};
        rc_span_int s = RC_SPAN(arr);
        rc_sort_int(s);
        ASSERT(ints_sorted(arr, 17));
    }
    END_GROUP();

    BEGIN_GROUP("just above insertion-sort threshold (17)");
    {
        /* reversed 1..17 forces worst-case input for naive quicksort */
        int arr[] = {17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
        rc_span_int s = RC_SPAN(arr);
        rc_sort_int(s);
        ASSERT(ints_sorted(arr, 17));
        for (int i = 0; i < 17; i++) ASSERT(arr[i] == i + 1);
    }
    END_GROUP();

    BEGIN_GROUP("large permutation (200 elements)");
    {
        /* (i*37 + 13) % 200 is a bijection on [0,200) since gcd(37,200)=1 */
#define N 200
        int arr[N];
        for (int i = 0; i < N; i++) arr[i] = (i * 37 + 13) % N;
        rc_sort_int((rc_span_int) {.data = arr, .num = N});
        ASSERT(ints_sorted(arr, N));
        for (int i = 0; i < N; i++) ASSERT(arr[i] == i);
#undef N
    }
    END_GROUP();

    BEGIN_GROUP("large already-sorted (200 elements)");
    {
#define N 200
        int arr[N];
        for (int i = 0; i < N; i++) arr[i] = i;
        rc_sort_int((rc_span_int) {.data = arr, .num = N});
        ASSERT(ints_sorted(arr, N));
#undef N
    }
    END_GROUP();

    BEGIN_GROUP("large reverse-sorted (200 elements)");
    {
#define N 200
        int arr[N];
        for (int i = 0; i < N; i++) arr[i] = N - 1 - i;
        rc_sort_int((rc_span_int) {.data = arr, .num = N});
        ASSERT(ints_sorted(arr, N));
        for (int i = 0; i < N; i++) ASSERT(arr[i] == i);
#undef N
    }
    END_GROUP();
}

/* ---- sort / search (Record, custom comparator) ---- */

static void test_record(void)
{
    printf("Record (custom comparator)\n");

    BEGIN_GROUP("sort by key");
    {
        Record arr[] = {
            {5, 10}, {2, 20}, {8, 30}, {1, 40}, {4, 50}
        };
        rc_span_Record s = RC_SPAN(arr);
        rc_sort_Record(s);
        ASSERT(records_sorted_by_key(arr, 5));
        /* payloads should have moved with their keys */
        ASSERT(arr[0].key == 1 && arr[0].payload == 40);
        ASSERT(arr[4].key == 8 && arr[4].payload == 30);
    }
    END_GROUP();

    BEGIN_GROUP("lower/upper_bound by key");
    {
        Record arr[] = {
            {1,0}, {3,0}, {3,0}, {3,0}, {5,0}
        };
        rc_view_Record v = RC_VIEW(arr);
        Record key;

        key = (Record) {3, 0};
        ASSERT(rc_lower_bound_Record(v, key) == 1); /* first 3 */
        ASSERT(rc_upper_bound_Record(v, key) == 4); /* one past last 3 */

        key = (Record) {2, 0};
        size_t lo = rc_lower_bound_Record(v, key);
        size_t hi = rc_upper_bound_Record(v, key);
        ASSERT(lo == hi); /* 2 is absent: empty range */
    }
    END_GROUP();
}

/* ---- bounds-checked access ---- */

static void test_access(void)
{
    printf("bounds-checked access\n");

    BEGIN_GROUP("RC_VIEW_AT read");
    {
        int arr[] = {10, 20, 30, 40, 50};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(RC_VIEW_AT(v, 0) == 10);
        ASSERT(RC_VIEW_AT(v, 2) == 30);
        ASSERT(RC_VIEW_AT(v, 4) == 50);
    }
    END_GROUP();

    BEGIN_GROUP("RC_SPAN_AT read and write");
    {
        int buf[] = {1, 2, 3};
        rc_span_int s = RC_SPAN(buf);
        ASSERT(RC_SPAN_AT(s, 0) == 1);
        RC_SPAN_AT(s, 1) = 99;          /* use as lvalue */
        ASSERT(buf[1] == 99);
        ASSERT(RC_SPAN_AT(s, 1) == 99);
    }
    END_GROUP();

    BEGIN_GROUP("RC_VIEW_AT on Record");
    {
        Record arr[] = {{1, 10}, {2, 20}, {3, 30}};
        rc_view_Record v = RC_VIEW(arr);
        ASSERT(RC_VIEW_AT(v, 0).key     == 1);
        ASSERT(RC_VIEW_AT(v, 2).payload == 30);
    }
    END_GROUP();

    BEGIN_GROUP("RC_SPAN_AT on Record");
    {
        Record arr[] = {{1, 10}, {2, 20}, {3, 30}};
        rc_span_Record s = RC_SPAN(arr);
        RC_SPAN_AT(s, 1).payload = 99;
        ASSERT(arr[1].payload == 99);
    }
    END_GROUP();

    BEGIN_GROUP("RC_VIEW_AT directly on Array");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 10, &a);
        rc_array_int_push(&arr, 20, &a);
        rc_array_int_push(&arr, 30, &a);
        ASSERT(RC_VIEW_AT(arr, 0) == 10);
        ASSERT(RC_VIEW_AT(arr, 2) == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("RC_SPAN_AT read/write directly on Array");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 1, &a);
        rc_array_int_push(&arr, 2, &a);
        rc_array_int_push(&arr, 3, &a);
        RC_SPAN_AT(arr, 1) = 99;
        ASSERT(arr.data[1] == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("RC_AS_VIEW: span->view and array->view");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i + 1, &a);

        rc_span_int  s  = rc_array_int_as_span(&arr);
        rc_view_int  v1 = RC_AS_VIEW(int, s);     /* span  → view */
        ASSERT(v1.num == 5 && v1.data[2] == 3);

        rc_view_int  v2 = RC_AS_VIEW(int, arr);   /* array → view */
        ASSERT(v2.num == 5 && v2.data[0] == 1);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("RC_AS_SPAN: array->span");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 4; i++) rc_array_int_push(&arr, i * 10, &a);

        rc_span_int s = RC_AS_SPAN(int, arr);     /* array → span */
        ASSERT(s.num == 4);
        RC_SPAN_AT(s, 1) = 99;
        ASSERT(arr.data[1] == 99);                /* writes through to array */

        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- platform virtual memory ---- */

static void test_platform(void)
{
    printf("platform virtual memory\n");

    BEGIN_GROUP("page size: reasonable and power-of-two");
    {
        size_t page = rc_platform_page_size();
        ASSERT(page >= 4096);
        ASSERT((page & (page - 1)) == 0);   /* power of two */
    }
    END_GROUP();

    BEGIN_GROUP("round_up_to_page");
    {
        size_t page = rc_platform_page_size();
        ASSERT(rc_platform_round_up_to_page(0)          == 0);
        ASSERT(rc_platform_round_up_to_page(1)          == page);
        ASSERT(rc_platform_round_up_to_page(page - 1)   == page);
        ASSERT(rc_platform_round_up_to_page(page)       == page);
        ASSERT(rc_platform_round_up_to_page(page + 1)   == 2 * page);
        ASSERT(rc_platform_round_up_to_page(2 * page)   == 2 * page);
    }
    END_GROUP();

    BEGIN_GROUP("reserve, commit, write, release");
    {
        size_t page = rc_platform_page_size();
        size_t size = 4 * page;

        void *mem = rc_platform_reserve(size);
        ASSERT(mem != NULL);

        ASSERT(rc_platform_commit(mem, page));

        /* Write to first and last byte of the committed page. */
        unsigned char *p = mem;
        p[0]        = 0xAB;
        p[page - 1] = 0xCD;
        ASSERT(p[0]        == 0xAB);
        ASSERT(p[page - 1] == 0xCD);

        rc_platform_release(mem, size);
        ASSERT(1);
    }
    END_GROUP();

    BEGIN_GROUP("commit multiple pages");
    {
        size_t page = rc_platform_page_size();

        void *mem = rc_platform_reserve(4 * page);
        ASSERT(mem != NULL);

        /* Commit pages one at a time and verify each is writable. */
        for (size_t i = 0; i < 4; i++) {
            unsigned char *p = (unsigned char *)mem + i * page;
            ASSERT(rc_platform_commit(p, page));
            p[0]        = (unsigned char)(i * 10);
            p[page - 1] = (unsigned char)(i * 10 + 1);
        }
        unsigned char *p = mem;
        for (size_t i = 0; i < 4; i++) {
            ASSERT(p[i * page]        == (unsigned char)(i * 10));
            ASSERT(p[i * page + page - 1] == (unsigned char)(i * 10 + 1));
        }

        rc_platform_release(mem, 4 * page);
        ASSERT(1);
    }
    END_GROUP();

    BEGIN_GROUP("decommit and recommit");
    {
        size_t page = rc_platform_page_size();

        void *mem = rc_platform_reserve(2 * page);
        ASSERT(mem != NULL);
        ASSERT(rc_platform_commit(mem, page));

        unsigned char *p = mem;
        p[0] = 42;

        rc_platform_decommit(mem, page);

        /* Recommit and verify the page is writable (content may be zeroed). */
        ASSERT(rc_platform_commit(mem, page));
        p[0] = 99;
        ASSERT(p[0] == 99);

        rc_platform_release(mem, 2 * page);
        ASSERT(1);
    }
    END_GROUP();
}

/* ---- rc_arena ---- */

static void test_arena(void)
{
    printf("rc_arena\n");

    BEGIN_GROUP("create and destroy");
    {
        rc_arena a = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        ASSERT(a.base      != NULL);
        ASSERT(a.top       == 0);            /* no allocations yet          */
        ASSERT(a.committed >  0);            /* first page is committed     */
        ASSERT(a.reserved  >  a.committed);  /* reserved space beyond that  */
        rc_arena_destroy(&a);
        ASSERT(a.base == NULL);              /* zeroed after destroy        */
    }
    END_GROUP();

    BEGIN_GROUP("basic alloc and write");
    {
        rc_arena a = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        ASSERT(a.base != NULL);

        int   *i = rc_arena_alloc_type(&a, int,  1);
        float *f = rc_arena_alloc_type(&a, float, 4);
        ASSERT(i != NULL && f != NULL);
        ASSERT((void *)f > (void *)i);   /* f is after i in the rc_arena    */

        *i = 99;
        f[0] = 1.0f; f[3] = 4.0f;
        ASSERT(*i    == 99);
        ASSERT(f[0]  == 1.0f && f[3] == 4.0f);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("alignment");
    {
        rc_arena  a     = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        size_t align = RC_MAX_ALIGN;

        /* Allocate odd sizes; every returned pointer must be aligned. */
        for (size_t sz = 1; sz <= 33; sz++) {
            void *p = rc_arena_alloc(&a, (uint32_t)sz);
            ASSERT(p != NULL);
            ASSERT(((uintptr_t)p % align) == 0);
        }
        /* Consecutive allocations must not overlap. */
        void *p1 = rc_arena_alloc(&a, 1);
        void *p2 = rc_arena_alloc(&a, 1);
        ASSERT((char *)p2 - (char *)p1 >= (ptrdiff_t)align);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("auto-commit past initial page");
    {
        rc_arena    a    = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        size_t   page = rc_platform_page_size();
        uint32_t committed_before = a.committed;

        /* Allocate just over one page to force a second commit. */
        void *p = rc_arena_alloc(&a, (uint32_t)(page + 1));
        ASSERT(p != NULL);
        ASSERT(a.committed > committed_before);

        /* Write to the newly committed area. */
        ((char *)p)[page] = 0x42;
        ASSERT(((char *)p)[page] == 0x42);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_free_to: reuse freed space");
    {
        rc_arena a = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        ASSERT(a.base != NULL);

        uint32_t mark = a.top;           /* save position at start        */
        void *p1   = rc_arena_alloc(&a, 64);
        void *p2   = rc_arena_alloc(&a, 64);
        ASSERT(p1 != NULL && p2 != NULL);

        rc_arena_free_to(&a, mark);
        ASSERT(a.top == mark);           /* offset was reset              */

        /* Re-allocating the same sizes must give the same addresses.    */
        ASSERT(rc_arena_alloc(&a, 64) == p1);
        ASSERT(rc_arena_alloc(&a, 64) == p2);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_free_to: mid-rc_arena rollback");
    {
        rc_arena    a  = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        void    *p1 = rc_arena_alloc(&a, 128);
        uint32_t mark = a.top;           /* save after p1                 */
        void    *p2   = rc_arena_alloc(&a, 128);
        void    *p3   = rc_arena_alloc(&a, 128);
        (void)p2; (void)p3;

        rc_arena_free_to(&a, mark);
        ASSERT(a.top == mark);

        /* p1 is still below the mark and unaffected. */
        memset(p1, 0xAB, 128);
        ASSERT(((unsigned char *)p1)[127] == 0xAB);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_reset: decommits extra pages");
    {
        rc_arena    a    = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        size_t   page = rc_platform_page_size();
        uint32_t committed_before = a.committed;

        /* Force several extra pages to be committed. */
        ASSERT(rc_arena_alloc(&a, (uint32_t)(page * 4)) != NULL);
        ASSERT(a.committed > committed_before);

        rc_arena_reset(&a);
        ASSERT(a.top       == 0);                   /* bump pointer reset  */
        ASSERT(a.committed == committed_before);    /* extra pages gone    */

        /* rc_arena must still be usable after reset. */
        void *p = rc_arena_alloc(&a, 64);
        ASSERT(p == a.base);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("scratch pattern: caller unaffected");
    {
        rc_arena a = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        ASSERT(a.base != NULL);

        int *permanent = rc_arena_alloc_type(&a, int, 1);
        *permanent = 123;
        uint32_t top_before = a.top;

        /* Simulate passing rc_arena by value to a scratch function. */
        {
            rc_arena scratch = a;                           /* copy           */
            ASSERT(rc_arena_alloc(&scratch, 1024) != NULL); /* scratch grows  */
            ASSERT(scratch.top > a.top);                 /* local copy did */
            ASSERT(a.top == top_before);                 /* original didn't*/
        }

        /* Original rc_arena and its data are intact. */
        ASSERT(a.top       == top_before);
        ASSERT(*permanent  == 123);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("null return on zero-size alloc");
    {
        rc_arena a = rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
        ASSERT(rc_arena_alloc(&a, 0) == NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_make_default");
    {
        rc_arena a = rc_arena_make_default();
        ASSERT(a.base     != NULL);
        ASSERT(a.reserved == RC_ARENA_DEFAULT_RESERVE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_free: last item reclaimed");
    {
        rc_arena    a  = rc_arena_make_default();
        void    *p1 = rc_arena_alloc(&a, 64);
        uint32_t top_after_p1 = a.top;          /* offset after p1, before p2 */
        void    *p2 = rc_arena_alloc(&a, 64);

        rc_arena_free(&a, p2, 64);
        ASSERT(a.top == top_after_p1);          /* top moved back        */

        /* Re-allocating gives the same address. */
        ASSERT(rc_arena_alloc(&a, 64) == p2);

        rc_arena_destroy(&a);
        (void)p1;
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_free: non-last item is no-op");
    {
        rc_arena    a  = rc_arena_make_default();
        void    *p1 = rc_arena_alloc(&a, 64);
        void    *p2 = rc_arena_alloc(&a, 64);
        uint32_t top_before = a.top;

        rc_arena_free(&a, p1, 64);                 /* p1 is not last        */
        ASSERT(a.top == top_before);            /* top unchanged         */

        rc_arena_destroy(&a);
        (void)p2;
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc: grow last item in place");
    {
        rc_arena    a   = rc_arena_make_default();
        uint32_t top_before = a.top;

        int *p = rc_arena_alloc_type(&a, int, 4);
        p[0] = 1; p[3] = 4;
        uint32_t top_after_first = a.top;

        int *p2 = rc_arena_realloc(&a, p, sizeof(int)*4, sizeof(int)*8);
        ASSERT(p2 == p);                        /* same pointer          */
        ASSERT(a.top > top_after_first);        /* top advanced          */
        ASSERT(p2[0] == 1 && p2[3] == 4);      /* old data preserved    */
        /* new bytes [4..7] are unspecified — no zero assertion          */

        rc_arena_destroy(&a);
        (void)top_before;
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc: shrink last item in place");
    {
        rc_arena    a  = rc_arena_make_default();
        int     *p  = rc_arena_alloc_type(&a, int, 8);
        uint32_t top_after_8 = a.top;

        int *p2 = rc_arena_realloc(&a, p, sizeof(int)*8, sizeof(int)*4);
        ASSERT(p2 == p);                        /* same pointer          */
        ASSERT(a.top < top_after_8);            /* top moved back        */

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc: grow non-last copies");
    {
        rc_arena a  = rc_arena_make_default();
        int  *p1 = rc_arena_alloc_type(&a, int, 4);
        p1[0] = 10; p1[3] = 40;
        int  *p2 = rc_arena_alloc_type(&a, int, 4); /* allocate after p1    */
        (void)p2;

        int *p3 = rc_arena_realloc(&a, p1, sizeof(int)*4, sizeof(int)*8);
        ASSERT(p3 != p1);                       /* new allocation        */
        ASSERT(p3[0] == 10 && p3[3] == 40);    /* data copied           */
        /* new bytes [4..7] are unspecified — no zero assertion          */

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc: shrink non-last is no-op");
    {
        rc_arena a  = rc_arena_make_default();
        int  *p1 = rc_arena_alloc_type(&a, int, 8);
        int     *p2 = rc_arena_alloc_type(&a, int, 4); /* allocate after p1    */
        uint32_t top_before = a.top;

        int *p3 = rc_arena_realloc(&a, p1, sizeof(int)*8, sizeof(int)*4);
        ASSERT(p3 == p1);                       /* same pointer          */
        ASSERT(a.top == top_before);            /* top unchanged         */

        rc_arena_destroy(&a);
        (void)p2;
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc: NULL ptr acts as alloc");
    {
        rc_arena a = rc_arena_make_default();
        int  *p = rc_arena_realloc(&a, NULL, 0, sizeof(int) * 4);
        ASSERT(p != NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc: new_size 0 acts as free");
    {
        rc_arena    a          = rc_arena_make_default();
        uint32_t top_before = a.top;
        void    *p          = rc_arena_alloc(&a, 64);
        void    *ret        = rc_arena_realloc(&a, p, 64, 0);
        ASSERT(ret   == NULL);
        ASSERT(a.top == top_before);            /* freed as last item    */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_alloc: dirty memory after free");
    {
        rc_arena         a = rc_arena_make_default();
        unsigned char *p = rc_arena_alloc(&a, 8);
        ASSERT(p != NULL);
        memset(p, 0xAB, 8);

        rc_arena_free(&a, p, 8);               /* reclaim as last item      */
        unsigned char *p2 = rc_arena_alloc(&a, 8);
        ASSERT(p2 == p);                    /* same address              */
        /* rc_arena_alloc makes no zeroing promise: old bytes are intact    */
        int all_dirty = 1;
        for (int i = 0; i < 8; i++) if (p2[i] != 0xAB) {all_dirty = 0; break;}
        ASSERT(all_dirty);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_alloc_zero: zeroed even on reused memory");
    {
        rc_arena         a = rc_arena_make_default();
        unsigned char *p = rc_arena_alloc(&a, 8);
        ASSERT(p != NULL);
        memset(p, 0xAB, 8);

        rc_arena_free(&a, p, 8);               /* reclaim as last item      */
        unsigned char *p2 = rc_arena_alloc_zero(&a, 8);
        ASSERT(p2 == p);                    /* same address              */
        int all_zero = 1;
        for (int i = 0; i < 8; i++) if (p2[i] != 0) {all_zero = 0; break;}
        ASSERT(all_zero);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc_zero: new bytes zeroed (last item)");
    {
        rc_arena a  = rc_arena_make_default();
        int  *p  = rc_arena_alloc_type(&a, int, 4);
        p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4;

        int *p2 = rc_arena_realloc_zero(&a, p, sizeof(int)*4, sizeof(int)*8);
        ASSERT(p2 == p);                    /* grew in place             */
        ASSERT(p2[0] == 1 && p2[3] == 4);  /* old data preserved        */
        ASSERT(p2[4] == 0 && p2[5] == 0 && p2[6] == 0 && p2[7] == 0);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_arena_realloc_zero: new bytes zeroed (non-last)");
    {
        rc_arena a  = rc_arena_make_default();
        int  *p1 = rc_arena_alloc_type(&a, int, 4);
        p1[0] = 10; p1[1] = 20; p1[2] = 30; p1[3] = 40;
        int  *p2 = rc_arena_alloc_type(&a, int, 4); /* block in-place grow  */
        (void)p2;

        int *p3 = rc_arena_realloc_zero(&a, p1, sizeof(int)*4, sizeof(int)*8);
        ASSERT(p3 != p1);                   /* forced copy               */
        ASSERT(p3[0] == 10 && p3[3] == 40);/* old data copied            */
        ASSERT(p3[4] == 0 && p3[5] == 0 && p3[6] == 0 && p3[7] == 0);

        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- array ---- */

static void test_array(void)
{
    printf("array\n");

    BEGIN_GROUP("push: returned index and read back");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        size_t idx = rc_array_int_push(&arr, 42, &a);
        ASSERT(idx == 0);
        ASSERT(arr.num == 1);
        ASSERT(arr.data[0] == 42);
        idx = rc_array_int_push(&arr, 99, &a);
        ASSERT(idx == 1);
        ASSERT(arr.num == 2);
        ASSERT(arr.data[1] == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("push: cap doubles 8->16->32 (32 pushes)");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 32; i++) rc_array_int_push(&arr, i, &a);
        ASSERT(arr.num == 32);
        ASSERT(arr.cap == 32);
        for (int i = 0; i < 32; i++) ASSERT(arr.data[i] == i);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: pre-allocates, cap stable on push");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_reserve(&arr, 50, &a);
        ASSERT(arr.cap >= 50);
        size_t cap_before = arr.cap;
        rc_array_int_push(&arr, 1, &a);
        ASSERT(arr.cap == cap_before);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("in-place growth: pointer stable (array last in rc_arena)");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 0, &a);       /* initial alloc: 8 slots    */
        int *data_before = arr.data;
        for (int i = 1; i <= 8; i++)       /* 9th push triggers 8->16   */
            rc_array_int_push(&arr, i, &a);
        ASSERT(arr.data == data_before);   /* grew in place              */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("blocked growth: pointer changes");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 0, &a);       /* cap = 8                   */
        int *data_before = arr.data;
        rc_arena_alloc(&a, 1);                /* block in-place growth     */
        for (int i = 1; i <= 8; i++)       /* 9th push triggers 8->16   */
            rc_array_int_push(&arr, i, &a);
        ASSERT(arr.data != data_before);   /* had to copy               */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pop: correct value, num decrements");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 10, &a);
        rc_array_int_push(&arr, 20, &a);
        rc_array_int_push(&arr, 30, &a);
        ASSERT(rc_array_int_pop(&arr) == 30);
        ASSERT(arr.num == 2);
        ASSERT(rc_array_int_pop(&arr) == 20);
        ASSERT(arr.num == 1);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("push_n: appends C array, returns first index");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 0, &a);
        int src[] = {10, 20, 30};
        size_t idx = rc_array_int_push_n(&arr, src, 3, &a);
        ASSERT(idx == 1);
        ASSERT(arr.num == 4);
        ASSERT(arr.data[1] == 10 && arr.data[2] == 20 && arr.data[3] == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append_view: appends typed view");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        int src[] = {1, 2, 3, 4, 5};
        rc_view_int v = RC_VIEW(src);
        size_t idx = rc_array_int_append_view(&arr, v, &a);
        ASSERT(idx == 0);
        ASSERT(arr.num == 5);
        for (int i = 0; i < 5; i++) ASSERT(arr.data[i] == i + 1);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("resize: grows and shrinks");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_resize(&arr, 5, &a);
        ASSERT(arr.num == 5);
        ASSERT(arr.cap >= 5);
        arr.data[0] = 99;
        rc_array_int_resize(&arr, 3, &a);
        ASSERT(arr.num == 3);
        ASSERT(arr.data[0] == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("insert: beginning, middle, end");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 10, &a);
        rc_array_int_push(&arr, 30, &a);
        rc_array_int_insert(&arr, 1, 20, &a);      /* middle */
        ASSERT(arr.num == 3);
        ASSERT(arr.data[0] == 10 && arr.data[1] == 20 && arr.data[2] == 30);
        rc_array_int_insert(&arr, 0, 0, &a);       /* beginning */
        ASSERT(arr.data[0] == 0 && arr.data[1] == 10);
        rc_array_int_insert(&arr, arr.num, 40, &a);/* end */
        ASSERT(arr.data[arr.num - 1] == 40);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("insert_n_zero: zeroed gap, writable pointer");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 1, &a);
        rc_array_int_push(&arr, 4, &a);
        int *gap = rc_array_int_insert_n_zero(&arr, 1, 2, &a);
        ASSERT(arr.num == 4);
        ASSERT(arr.data[0] == 1);
        ASSERT(gap[0] == 0 && gap[1] == 0);
        ASSERT(arr.data[3] == 4);
        gap[0] = 2; gap[1] = 3;
        ASSERT(arr.data[1] == 2 && arr.data[2] == 3);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("insert_view: inserts view elements at position");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 1, &a);
        rc_array_int_push(&arr, 5, &a);
        int src[] = {2, 3, 4};
        rc_view_int v = RC_VIEW(src);
        rc_array_int_insert_view(&arr, 1, v, &a);
        ASSERT(arr.num == 5);
        for (int i = 0; i < 5; i++) ASSERT(arr.data[i] == i + 1);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove: value returned, elements shifted");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        int val = rc_array_int_remove(&arr, 2);    /* remove 20 */
        ASSERT(val == 20);
        ASSERT(arr.num == 4);
        ASSERT(arr.data[0] == 0  && arr.data[1] == 10);
        ASSERT(arr.data[2] == 30 && arr.data[3] == 40);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove_n: multi-element stable removal");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 6; i++) rc_array_int_push(&arr, i, &a);
        rc_array_int_remove_n(&arr, 1, 3);         /* remove [1,2,3] */
        ASSERT(arr.num == 3);
        ASSERT(arr.data[0] == 0 && arr.data[1] == 4 && arr.data[2] == 5);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove_swap: O(1), last element fills gap");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        int val = rc_array_int_remove_swap(&arr, 1); /* remove 10; 40 fills */
        ASSERT(val == 10);
        ASSERT(arr.num == 4);
        ASSERT(arr.data[1] == 40);
        ASSERT(arr.data[0] == 0 && arr.data[2] == 20 && arr.data[3] == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("NULL rc_arena: succeeds when cap sufficient");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_reserve(&arr, 16, &a);        /* allocate with rc_arena   */
        size_t cap = arr.cap;
        for (int i = 0; i < 16; i++)
            rc_array_int_push(&arr, i, NULL);      /* no realloc needed     */
        ASSERT(arr.num == 16);
        ASSERT(arr.cap == cap);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("view/span: sort via span, lower_bound via view");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        int src[] = {5, 3, 1, 4, 2};
        rc_array_int_push_n(&arr, src, 5, &a);
        rc_sort_int(rc_array_int_as_span(&arr));
        for (int i = 0; i < 5; i++) ASSERT(arr.data[i] == i + 1);
        rc_view_int v = rc_array_int_as_view(&arr);
        ASSERT(rc_lower_bound_int(v, 3) == 2);
        ASSERT(rc_lower_bound_int(v, 6) == 5);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rc_array_Record: push, sort, lower_bound");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_Record arr = {0};
        rc_array_Record_push(&arr, (Record) {3, 30}, &a);
        rc_array_Record_push(&arr, (Record) {1, 10}, &a);
        rc_array_Record_push(&arr, (Record) {2, 20}, &a);
        ASSERT(arr.num == 3);
        rc_sort_Record(rc_array_Record_as_span(&arr));
        ASSERT(arr.data[0].key == 1 && arr.data[0].payload == 10);
        ASSERT(arr.data[1].key == 2 && arr.data[1].payload == 20);
        ASSERT(arr.data[2].key == 3 && arr.data[2].payload == 30);
        Record key = {2, 0};
        ASSERT(rc_lower_bound_Record(rc_array_Record_as_view(&arr), key) == 1);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("span_as_view: adds const, usable by lower_bound");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 2, &a); /* [0,2,4,6,8] */
        rc_span_int s = rc_array_int_as_span(&arr);
        rc_view_int v = rc_span_int_as_view(s);
        ASSERT(v.num == 5);
        ASSERT(rc_lower_bound_int(v, 4) == 2);
        ASSERT(rc_lower_bound_int(v, 5) == 3);   /* gap: between 4 and 6 */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP(".view and .span field access on Array");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i + 1, &a); /* [1..5] */

        /* arr.view: field access gives T_View directly, no function call */
        ASSERT(rc_lower_bound_int(arr.view, 3) == 2);
        ASSERT(arr.view.data[0] == 1);          /* const pointer readable */
        ASSERT(arr.view.num == 5);

        /* arr.span: field access gives T_Span directly */
        rc_sort_int(arr.span);                     /* sort is no-op: already sorted */
        ASSERT(arr.data[0] == 1 && arr.data[4] == 5);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP(".view field access on Span");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        int src[] = {5, 3, 1, 4, 2};
        rc_array_int_push_n(&arr, src, 5, &a);
        rc_span_int s = arr.span;
        rc_sort_int(s);                            /* sorts arr.data in place */
        /* s.view: span → view via field, no conversion call */
        ASSERT(rc_lower_bound_int(s.view, 4) == 3);
        ASSERT(s.view.data[0] == 1 && s.view.data[4] == 5);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* ---- element access: get / at / set ---- */

    BEGIN_GROUP("view_get: returns by value, does not alias");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        ASSERT(rc_view_int_get(v, 0) == 10);
        ASSERT(rc_view_int_get(v, 2) == 30);
        /* Modifying data does not affect a previously fetched value. */
        int val = rc_view_int_get(v, 1);
        data[1] = 99;
        ASSERT(val == 20);
    }
    END_GROUP();

    BEGIN_GROUP("view_at: returns const pointer into original data");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        const int *p = rc_view_int_at(v, 1);
        ASSERT(p == &data[1]);
        ASSERT(*p == 20);
    }
    END_GROUP();

    BEGIN_GROUP("span_get: returns by value");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 4; i++) rc_array_int_push(&arr, i * 10, &a);
        rc_span_int s = arr.span;
        ASSERT(rc_span_int_get(s, 0) == 0);
        ASSERT(rc_span_int_get(s, 3) == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("span_at: returns mutable pointer into original data");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 3; i++) rc_array_int_push(&arr, i, &a);
        rc_span_int s = arr.span;
        int *p = rc_span_int_at(s, 1);
        ASSERT(p == &arr.data[1]);
        *p = 99;
        ASSERT(arr.data[1] == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("span_set: writes through to original data");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 3; i++) rc_array_int_push(&arr, 0, &a);
        rc_span_int_set(arr.span, 0, 11);
        rc_span_int_set(arr.span, 2, 33);
        ASSERT(arr.data[0] == 11 && arr.data[1] == 0 && arr.data[2] == 33);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_get: returns by value");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 3, &a);
        ASSERT(rc_array_int_get(&arr, 0) == 0);
        ASSERT(rc_array_int_get(&arr, 4) == 12);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_at: returns mutable pointer");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 3; i++) rc_array_int_push(&arr, i, &a);
        int *p = rc_array_int_at(&arr, 2);
        ASSERT(p == &arr.data[2]);
        *p = 77;
        ASSERT(arr.data[2] == 77);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_set: writes element at index");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 4; i++) rc_array_int_push(&arr, 0, &a);
        rc_array_int_set(&arr, 1, 55);
        rc_array_int_set(&arr, 3, 88);
        ASSERT(arr.data[0] == 0 && arr.data[1] == 55);
        ASSERT(arr.data[2] == 0 && arr.data[3] == 88);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get/at/set: single-element container (boundary)");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 42, &a);
        ASSERT(rc_array_int_get(&arr, 0) == 42);
        ASSERT(*rc_array_int_at(&arr, 0) == 42);
        rc_array_int_set(&arr, 0, 99);
        ASSERT(rc_array_int_get(&arr, 0) == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* ---- sub-slice functions ---- */

    /* rc_view_int_subview */

    BEGIN_GROUP("view_subview: normal range");
    {
        int data[] = {10, 20, 30, 40, 50};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 1, 3);
        ASSERT(s.num == 3);
        ASSERT(s.data[0] == 20 && s.data[1] == 30 && s.data[2] == 40);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: full range");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 0, 3);
        ASSERT(s.num == 3 && s.data == data);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: start=0, count=0");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 0, 0);
        ASSERT(s.num == 0 && s.data == data);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: start clamped beyond end");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 100, 1);
        ASSERT(s.num == 0 && s.data == data + 3);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: start == num");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 3, 0);
        ASSERT(s.num == 0 && s.data == data + 3);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: count clamped");
    {
        int data[] = {10, 20, 30, 40, 50};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 3, 100);
        ASSERT(s.num == 2);
        ASSERT(s.data[0] == 40 && s.data[1] == 50);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: count=UINT32_MAX clamped to remaining");
    {
        int data[] = {10, 20, 30, 40, 50};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 0, UINT32_MAX);
        ASSERT(s.num == 5 && s.data == data);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: start=UINT32_MAX clamped");
    {
        int data[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, UINT32_MAX, 1);
        ASSERT(s.num == 0);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: empty source");
    {
        rc_view_int v = {0};
        rc_view_int s = rc_view_int_subview(v, 0, 5);
        ASSERT(s.num == 0);
    }
    END_GROUP();

    BEGIN_GROUP("view_subview: exactly to end");
    {
        int data[] = {10, 20, 30, 40, 50};
        rc_view_int v = RC_VIEW(data);
        rc_view_int s = rc_view_int_subview(v, 3, 2);
        ASSERT(s.num == 2);
        ASSERT(s.data[0] == 40 && s.data[1] == 50);
    }
    END_GROUP();

    /* rc_span_int_subspan */

    BEGIN_GROUP("span_subspan: normal range, mutation visible in original");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        rc_span_int s = arr.span;
        rc_span_int sub = rc_span_int_subspan(s, 1, 3);
        ASSERT(sub.num == 3);
        ASSERT(sub.data[0] == 10 && sub.data[1] == 20 && sub.data[2] == 30);
        sub.data[1] = 99;
        ASSERT(arr.data[2] == 99);   /* mutation visible in original */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("span_subspan: start clamped");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 3; i++) rc_array_int_push(&arr, i, &a);
        rc_span_int sub = rc_span_int_subspan(arr.span, 100, 5);
        ASSERT(sub.num == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("span_subspan: count clamped");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        rc_span_int sub = rc_span_int_subspan(arr.span, 3, 100);
        ASSERT(sub.num == 2);
        ASSERT(sub.data[0] == 30 && sub.data[1] == 40);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_span_int_subview */

    BEGIN_GROUP("span_subview: normal range");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        rc_view_int v = rc_span_int_subview(arr.span, 2, 2);
        ASSERT(v.num == 2);
        ASSERT(v.data[0] == 20 && v.data[1] == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("span_subview: count clamped");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 4; i++) rc_array_int_push(&arr, i, &a);
        rc_view_int v = rc_span_int_subview(arr.span, 2, UINT32_MAX);
        ASSERT(v.num == 2);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_array_int_subspan */

    BEGIN_GROUP("array_subspan: normal range, mutation visible in array");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        rc_span_int sub = rc_array_int_subspan(&arr, 1, 3);
        ASSERT(sub.num == 3);
        ASSERT(sub.data[0] == 10 && sub.data[1] == 20 && sub.data[2] == 30);
        sub.data[0] = 55;
        ASSERT(arr.data[1] == 55);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_subspan: start clamped beyond end");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        rc_array_int_push(&arr, 1, &a);
        rc_span_int sub = rc_array_int_subspan(&arr, 999, 5);
        ASSERT(sub.num == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_subspan: count clamped");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 4; i++) rc_array_int_push(&arr, i, &a);
        rc_span_int sub = rc_array_int_subspan(&arr, 2, 10);
        ASSERT(sub.num == 2 && sub.data[0] == 2 && sub.data[1] == 3);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_subspan: empty array");
    {
        rc_array_int arr = {0};
        rc_span_int sub = rc_array_int_subspan(&arr, 0, 5);
        ASSERT(sub.num == 0);
    }
    END_GROUP();

    /* rc_array_int_subview */

    BEGIN_GROUP("array_subview: normal range");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 5; i++) rc_array_int_push(&arr, i * 10, &a);
        rc_view_int v = rc_array_int_subview(&arr, 2, 2);
        ASSERT(v.num == 2);
        ASSERT(v.data[0] == 20 && v.data[1] == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_subview: start=0 full range");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 3; i++) rc_array_int_push(&arr, i, &a);
        rc_view_int v = rc_array_int_subview(&arr, 0, 3);
        ASSERT(v.num == 3 && v.data == arr.data);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_subview: count=UINT32_MAX clamped");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int arr = {0};
        for (int i = 0; i < 4; i++) rc_array_int_push(&arr, i, &a);
        rc_view_int v = rc_array_int_subview(&arr, 1, UINT32_MAX);
        ASSERT(v.num == 3);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("array_subview: empty array");
    {
        rc_array_int arr = {0};
        rc_view_int v = rc_array_int_subview(&arr, 0, 99);
        ASSERT(v.num == 0);
    }
    END_GROUP();
}

/* ---- context comparator ---- */

static void test_ctx(void)
{
    printf("context comparator\n");

    BEGIN_GROUP("sort ascending via ctx");
    {
        int arr[] = {5, 3, 1, 4, 2};
        rc_span_int s = RC_SPAN(arr);
        IntCmpCtx ctx = {.ascending = 1};
        int_sort_ctx(s, &ctx);
        ASSERT(ints_sorted(arr, 5));
        for (int i = 0; i < 5; i++) ASSERT(arr[i] == i + 1);
    }
    END_GROUP();

    BEGIN_GROUP("sort descending via ctx");
    {
        int arr[] = {1, 2, 3, 4, 5};
        rc_span_int s = RC_SPAN(arr);
        IntCmpCtx ctx = {.ascending = 0};
        int_sort_ctx(s, &ctx);
        for (int i = 0; i < 5; i++) ASSERT(arr[i] == 5 - i);
    }
    END_GROUP();

    BEGIN_GROUP("lower_bound ascending via ctx");
    {
        int arr[] = {1, 3, 5, 7, 9};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {.ascending = 1};
        ASSERT(int_lower_bound_ctx(v, &ctx,  5) == 2);
        ASSERT(int_lower_bound_ctx(v, &ctx,  6) == 3); /* gap between 5 and 7 */
        ASSERT(int_lower_bound_ctx(v, &ctx,  0) == 0); /* below all           */
        ASSERT(int_lower_bound_ctx(v, &ctx, 10) == 5); /* above all: past end */
    }
    END_GROUP();

    BEGIN_GROUP("upper_bound ascending via ctx");
    {
        int arr[] = {1, 3, 5, 7, 9};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {.ascending = 1};
        ASSERT(int_upper_bound_ctx(v, &ctx, 5) == 3); /* one past last 5   */
        ASSERT(int_upper_bound_ctx(v, &ctx, 9) == 5); /* one past last 9   */
        ASSERT(int_upper_bound_ctx(v, &ctx, 0) == 0); /* below all         */
    }
    END_GROUP();

    BEGIN_GROUP("lower_bound descending via ctx");
    {
        /* Array sorted descending: [9, 7, 5, 3, 1].
         * Under the descending comparator, lower_bound finds the first
         * index where !(element > value), i.e. element <= value. */
        int arr[] = {9, 7, 5, 3, 1};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {.ascending = 0};
        ASSERT(int_lower_bound_ctx(v, &ctx,  5) == 2); /* 5 is at index 2   */
        ASSERT(int_lower_bound_ctx(v, &ctx,  7) == 1); /* 7 is at index 1   */
        ASSERT(int_lower_bound_ctx(v, &ctx, 10) == 0); /* above all: index 0 */
        ASSERT(int_lower_bound_ctx(v, &ctx,  0) == 5); /* below all: past end */
    }
    END_GROUP();

    BEGIN_GROUP("lower/upper equal range, descending");
    {
        /* [7, 7, 5, 3, 3] sorted descending. */
        int arr[] = {7, 7, 5, 3, 3};
        rc_view_int v = RC_VIEW(arr);
        IntCmpCtx ctx = {.ascending = 0};
        size_t lo, hi;

        lo = int_lower_bound_ctx(v, &ctx, 7);
        hi = int_upper_bound_ctx(v, &ctx, 7);
        ASSERT(lo == 0 && hi == 2);  /* two 7s at indices 0, 1 */

        lo = int_lower_bound_ctx(v, &ctx, 3);
        hi = int_upper_bound_ctx(v, &ctx, 3);
        ASSERT(lo == 3 && hi == 5);  /* two 3s at indices 3, 4 */

        lo = int_lower_bound_ctx(v, &ctx, 4);
        hi = int_upper_bound_ctx(v, &ctx, 4);
        ASSERT(lo == hi);            /* 4 absent: empty range  */
    }
    END_GROUP();

    BEGIN_GROUP("default CMP with context (no explicit CMP)");
    {
        /* int_sort_default_ctx was instantiated with CTX but no CMP.
         * It should behave identically to rc_sort_int (ascending). */
        int arr[] = {5, 3, 1, 4, 2};
        rc_span_int s = RC_SPAN(arr);
        IntCmpCtx ctx = {.ascending = 1};
        int_sort_default_ctx(s, &ctx);
        ASSERT(ints_sorted(arr, 5));
    }
    END_GROUP();
}

/* ---- transform ---- */

static void test_transform(void)
{
    printf("transform\n");

    BEGIN_GROUP("square transform: values correct");
    {
        rc_arena a = rc_arena_make_default();
        int src[] = {1, 2, 3, 4, 5};
        rc_view_int  v   = RC_VIEW(src);
        rc_array_int dst = {0};
        int_square(v, &dst, &a);
        ASSERT(dst.num == 5);
        ASSERT(dst.data[0] == 1);
        ASSERT(dst.data[1] == 4);
        ASSERT(dst.data[2] == 9);
        ASSERT(dst.data[3] == 16);
        ASSERT(dst.data[4] == 25);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("int to Record transform");
    {
        rc_arena a = rc_arena_make_default();
        int src[] = {1, 2, 3};
        rc_view_int     v   = RC_VIEW(src);
        rc_array_Record dst = {0};
        int_to_record(v, &dst, &a);
        ASSERT(dst.num == 3);
        ASSERT(dst.data[0].key == 1 && dst.data[0].payload == 10);
        ASSERT(dst.data[1].key == 2 && dst.data[1].payload == 20);
        ASSERT(dst.data[2].key == 3 && dst.data[2].payload == 30);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("appends to existing elements");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int dst = {0};

        int src1[] = {1, 2, 3};
        int_square((rc_view_int) {src1, 3}, &dst, &a);   /* [1, 4, 9]      */

        int src2[] = {4, 5};
        int_square((rc_view_int) {src2, 2}, &dst, &a);   /* append [16, 25] */

        ASSERT(dst.num == 5);
        ASSERT(dst.data[0] == 1 && dst.data[1] == 4  && dst.data[2] == 9);
        ASSERT(dst.data[3] == 16 && dst.data[4] == 25);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("returns index of first appended element");
    {
        rc_arena a = rc_arena_make_default();
        int src[] = {10, 20, 30};
        rc_view_int  v   = RC_VIEW(src);
        rc_array_int dst = {0};

        rc_array_int_push(&dst, 99, &a);            /* existing element at 0 */
        size_t first = int_square(v, &dst, &a);  /* appends at [1, 2, 3]  */
        ASSERT(first == 1);
        ASSERT(dst.num == 4);
        ASSERT(dst.data[0] == 99);
        ASSERT(dst.data[1] == 100 && dst.data[2] == 400 && dst.data[3] == 900);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("empty source: no-op, dst unchanged");
    {
        rc_arena a = rc_arena_make_default();
        rc_array_int dst = {0};
        rc_array_int_push(&dst, 42, &a);
        size_t num_before = dst.num;

        int_square((rc_view_int) {NULL, 0}, &dst, &a);
        ASSERT(dst.num == num_before);

        /* return value == dst->num on entry */
        size_t ret = int_square((rc_view_int) {NULL, 0}, &dst, &a);
        ASSERT(ret == num_before);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("context transform: scale by factor");
    {
        rc_arena a = rc_arena_make_default();
        int src[] = {1, 2, 3, 4};
        rc_view_int  v   = RC_VIEW(src);
        rc_array_int dst = {0};
        ScaleCtx  ctx = {.factor = 3};
        int_scale(v, &dst, &ctx, &a);
        ASSERT(dst.num == 4);
        ASSERT(dst.data[0] == 3 && dst.data[1] == 6);
        ASSERT(dst.data[2] == 9 && dst.data[3] == 12);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("default func with context (no explicit func)");
    {
        rc_arena a = rc_arena_make_default();
        int src[] = {5, 6, 7};
        rc_view_int  v   = RC_VIEW(src);
        rc_array_int dst = {0};
        ScaleCtx  ctx = {.factor = 0};   /* ctx present but ignored */
        int_transform_default_ctx(v, &dst, &ctx, &a);
        ASSERT(dst.num == 3);
        ASSERT(dst.data[0] == 5 && dst.data[1] == 6 && dst.data[2] == 7);
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- find ---- */

static void test_find(void)
{
    printf("find\n");

    BEGIN_GROUP("empty view returns RC_INDEX_NONE");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(rc_find_int(v, 5) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("found at beginning, middle, end");
    {
        int arr[] = {10, 20, 30, 40, 50};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_find_int(v, 10) == 0);
        ASSERT(rc_find_int(v, 30) == 2);
        ASSERT(rc_find_int(v, 50) == 4);
    }
    END_GROUP();

    BEGIN_GROUP("absent element returns RC_INDEX_NONE");
    {
        int arr[] = {10, 20, 30};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_find_int(v, 99) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("returns first occurrence of duplicates");
    {
        int arr[] = {1, 3, 3, 3, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_find_int(v, 3) == 1);
    }
    END_GROUP();

    BEGIN_GROUP("Record find by key (custom predicate)");
    {
        Record arr[] = {{1, 10}, {2, 20}, {3, 30}, {2, 40}};
        rc_view_Record v = RC_VIEW(arr);
        Record key;

        key = (Record) {2, 0};
        ASSERT(rc_find_Record(v, key) == 1);   /* first element with key==2 */

        key = (Record) {3, 0};
        ASSERT(rc_find_Record(v, key) == 2);

        key = (Record) {9, 0};
        ASSERT(rc_find_Record(v, key) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("find with context: tolerance matching");
    {
        int arr[] = {1, 10, 20, 30, 40};
        rc_view_int v = RC_VIEW(arr);
        TolCtx ctx = {.tolerance = 3};

        /* 10 ± 3 → [7,13]: element 10 at index 1 matches */
        ASSERT(int_find_tol(v, &ctx, 10) == 1);

        /* 18 ± 3 → [15,21]: element 20 at index 2 matches first */
        ASSERT(int_find_tol(v, &ctx, 18) == 2);

        /* 1 ± 3 → [-2,4]: element 1 at index 0 matches */
        ASSERT(int_find_tol(v, &ctx, 1) == 0);

        /* 50 ± 3 → [47,53]: no element matches */
        ASSERT(int_find_tol(v, &ctx, 50) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("default pred with context (no explicit pred)");
    {
        int arr[] = {7, 8, 9};
        rc_view_int v = RC_VIEW(arr);
        TolCtx ctx = {.tolerance = 0};   /* ctx present but ignored */
        ASSERT(int_find_default_ctx(v, &ctx, 8) == 1);
        ASSERT(int_find_default_ctx(v, &ctx, 5) == RC_INDEX_NONE);
    }
    END_GROUP();
}

/* ---- remove ---- */

static void test_remove(void)
{
    printf("remove\n");

    BEGIN_GROUP("empty span: no-op, returns 0");
    {
        rc_span_int s = {0};
        ASSERT(int_remove_negative(&s) == 0);
        ASSERT(s.num == 0);
    }
    END_GROUP();

    BEGIN_GROUP("no elements match: span unchanged, returns 0");
    {
        int data[] = {1, 2, 3, 4, 5};
        rc_span_int s = RC_SPAN(data);
        ASSERT(int_remove_negative(&s) == 0);
        ASSERT(s.num == 5);
        for (int i = 0; i < 5; i++)
            ASSERT(data[i] == i + 1);
    }
    END_GROUP();

    BEGIN_GROUP("all elements match: num becomes 0, returns n");
    {
        int data[] = {-1, -2, -3};
        rc_span_int s = RC_SPAN(data);
        ASSERT(int_remove_negative(&s) == 3);
        ASSERT(s.num == 0);
    }
    END_GROUP();

    BEGIN_GROUP("mixed: negatives removed, positives kept in order");
    {
        int data[] = {1, -2, 3, -4, 5};
        rc_span_int s = RC_SPAN(data);
        ASSERT(int_remove_negative(&s) == 2);
        ASSERT(s.num == 3);
        ASSERT(data[0] == 1);
        ASSERT(data[1] == 3);
        ASSERT(data[2] == 5);
    }
    END_GROUP();

    BEGIN_GROUP("remove head and tail, keep middle");
    {
        int data[] = {-1, 2, 3, -4};
        rc_span_int s = RC_SPAN(data);
        ASSERT(int_remove_negative(&s) == 2);
        ASSERT(s.num == 2);
        ASSERT(data[0] == 2);
        ASSERT(data[1] == 3);
    }
    END_GROUP();

    BEGIN_GROUP("single element kept");
    {
        int data[] = {-1, -2, 7, -3};
        rc_span_int s = RC_SPAN(data);
        ASSERT(int_remove_negative(&s) == 3);
        ASSERT(s.num == 1);
        ASSERT(data[0] == 7);
    }
    END_GROUP();

    BEGIN_GROUP("custom type: Record, remove even keys");
    {
        Record data[] = {{1,0},{2,0},{3,0},{4,0},{5,0}};
        rc_span_Record s = RC_SPAN(data);
        ASSERT(record_remove_even_key(&s) == 2);
        ASSERT(s.num == 3);
        ASSERT(data[0].key == 1);
        ASSERT(data[1].key == 3);
        ASSERT(data[2].key == 5);
    }
    END_GROUP();

    BEGIN_GROUP("context: remove ints below threshold");
    {
        int data[] = {5, 1, 8, 2, 9, 3};
        rc_span_int s = RC_SPAN(data);
        ThreshCtx ctx = {4};
        ASSERT(int_remove_below(&s, &ctx) == 3);
        ASSERT(s.num == 3);
        ASSERT(data[0] == 5);
        ASSERT(data[1] == 8);
        ASSERT(data[2] == 9);
    }
    END_GROUP();

    BEGIN_GROUP("context: threshold above all elements removes all");
    {
        int data[] = {1, 2, 3};
        rc_span_int s = RC_SPAN(data);
        ThreshCtx ctx = {100};
        ASSERT(int_remove_below(&s, &ctx) == 3);
        ASSERT(s.num == 0);
    }
    END_GROUP();

    BEGIN_GROUP("context: threshold below all elements removes none");
    {
        int data[] = {10, 20, 30};
        rc_span_int s = RC_SPAN(data);
        ThreshCtx ctx = {0};
        ASSERT(int_remove_below(&s, &ctx) == 0);
        ASSERT(s.num == 3);
    }
    END_GROUP();
}

/* ---- rotate ---- */

static void test_rotate(void)
{
    printf("rotate\n");

    BEGIN_GROUP("empty span: no-op");
    {
        rc_span_int s = {0};
        rc_rotate_int(s, 0);   /* k==0 on empty */
        ASSERT(s.num == 0);
    }
    END_GROUP();

    BEGIN_GROUP("single element: no-op");
    {
        int data[] = {42};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 0);
        ASSERT(data[0] == 42);
        rc_rotate_int(s, 1);   /* k >= num: no-op */
        ASSERT(data[0] == 42);
    }
    END_GROUP();

    BEGIN_GROUP("k == 0: no-op");
    {
        int data[] = {1, 2, 3, 4, 5};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 0);
        for (int i = 0; i < 5; i++) ASSERT(data[i] == i + 1);
    }
    END_GROUP();

    BEGIN_GROUP("k >= num: no-op");
    {
        int data[] = {1, 2, 3};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 3);
        ASSERT(data[0] == 1 && data[1] == 2 && data[2] == 3);
        rc_rotate_int(s, 99);
        ASSERT(data[0] == 1 && data[1] == 2 && data[2] == 3);
    }
    END_GROUP();

    BEGIN_GROUP("k == 1: shift all left by one");
    {
        int data[] = {1, 2, 3, 4, 5};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 1);
        ASSERT(data[0] == 2);
        ASSERT(data[1] == 3);
        ASSERT(data[2] == 4);
        ASSERT(data[3] == 5);
        ASSERT(data[4] == 1);
    }
    END_GROUP();

    BEGIN_GROUP("k == num-1: only last element moves to front");
    {
        int data[] = {1, 2, 3, 4, 5};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 4);
        ASSERT(data[0] == 5);
        ASSERT(data[1] == 1);
        ASSERT(data[2] == 2);
        ASSERT(data[3] == 3);
        ASSERT(data[4] == 4);
    }
    END_GROUP();

    BEGIN_GROUP("k == num/2: even-length split");
    {
        int data[] = {1, 2, 3, 4, 5, 6};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 3);
        ASSERT(data[0] == 4);
        ASSERT(data[1] == 5);
        ASSERT(data[2] == 6);
        ASSERT(data[3] == 1);
        ASSERT(data[4] == 2);
        ASSERT(data[5] == 3);
    }
    END_GROUP();

    BEGIN_GROUP("two elements");
    {
        int data[] = {7, 9};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 1);
        ASSERT(data[0] == 9 && data[1] == 7);
    }
    END_GROUP();

    BEGIN_GROUP("rotate twice gives original when k1+k2==num");
    {
        int data[] = {1, 2, 3, 4, 5};
        rc_span_int s = RC_SPAN(data);
        rc_rotate_int(s, 2);
        rc_rotate_int(s, 3);
        for (int i = 0; i < 5; i++) ASSERT(data[i] == i + 1);
    }
    END_GROUP();

    BEGIN_GROUP("custom type: Record elements");
    {
        Record data[] = {{10,0},{20,0},{30,0},{40,0}};
        rc_span_Record s = RC_SPAN(data);
        record_rotate(s, 2);
        ASSERT(data[0].key == 30);
        ASSERT(data[1].key == 40);
        ASSERT(data[2].key == 10);
        ASSERT(data[3].key == 20);
    }
    END_GROUP();
}

/* ---- all_of / any_of / none_of ---- */

static void test_all_any_none_of(void)
{
    printf("all_of / any_of / none_of\n");

    BEGIN_GROUP("empty view: all=true, any=false, none=true");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(int_all_positive(v) == true);
        ASSERT(int_any_positive(v) == false);
        ASSERT(int_none_positive(v) == true);
    }
    END_GROUP();

    BEGIN_GROUP("all match: all=true, any=true, none=false");
    {
        int data[] = {1, 2, 3, 4};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == true);
        ASSERT(int_any_positive(v) == true);
        ASSERT(int_none_positive(v) == false);
    }
    END_GROUP();

    BEGIN_GROUP("none match: all=false, any=false, none=true");
    {
        int data[] = {-1, -2, -3};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == false);
        ASSERT(int_any_positive(v) == false);
        ASSERT(int_none_positive(v) == true);
    }
    END_GROUP();

    BEGIN_GROUP("some match (not all): all=false, any=true, none=false");
    {
        int data[] = {-1, 2, -3, 4};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == false);
        ASSERT(int_any_positive(v) == true);
        ASSERT(int_none_positive(v) == false);
    }
    END_GROUP();

    BEGIN_GROUP("single element matching");
    {
        int data[] = {5};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == true);
        ASSERT(int_any_positive(v) == true);
        ASSERT(int_none_positive(v) == false);
    }
    END_GROUP();

    BEGIN_GROUP("single element not matching");
    {
        int data[] = {-5};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == false);
        ASSERT(int_any_positive(v) == false);
        ASSERT(int_none_positive(v) == true);
    }
    END_GROUP();

    BEGIN_GROUP("match at first element only");
    {
        int data[] = {3, -1, -2};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == false);
        ASSERT(int_any_positive(v) == true);
        ASSERT(int_none_positive(v) == false);
    }
    END_GROUP();

    BEGIN_GROUP("match at last element only");
    {
        int data[] = {-1, -2, 3};
        rc_view_int v = RC_VIEW(data);
        ASSERT(int_all_positive(v) == false);
        ASSERT(int_any_positive(v) == true);
        ASSERT(int_none_positive(v) == false);
    }
    END_GROUP();

    BEGIN_GROUP("context: all at least threshold");
    {
        int data[] = {5, 6, 7};
        rc_view_int v = RC_VIEW(data);
        ThreshCtx ctx = {5};
        ASSERT(int_all_at_least(v, &ctx) == true);
        ASSERT(int_any_at_least(v, &ctx) == true);
        ASSERT(int_none_at_least(v, &ctx) == false);
    }
    END_GROUP();

    BEGIN_GROUP("context: none at least threshold");
    {
        int data[] = {1, 2, 3};
        rc_view_int v = RC_VIEW(data);
        ThreshCtx ctx = {10};
        ASSERT(int_all_at_least(v, &ctx) == false);
        ASSERT(int_any_at_least(v, &ctx) == false);
        ASSERT(int_none_at_least(v, &ctx) == true);
    }
    END_GROUP();

    BEGIN_GROUP("context: some at least threshold");
    {
        int data[] = {1, 10, 2, 20};
        rc_view_int v = RC_VIEW(data);
        ThreshCtx ctx = {5};
        ASSERT(int_all_at_least(v, &ctx) == false);
        ASSERT(int_any_at_least(v, &ctx) == true);
        ASSERT(int_none_at_least(v, &ctx) == false);
    }
    END_GROUP();
}

/* ---- mismatch ---- */

static void test_mismatch(void)
{
    printf("mismatch\n");

    BEGIN_GROUP("both empty: no overlap, returns 0");
    {
        rc_view_int v1 = {NULL, 0};
        rc_view_int v2 = {NULL, 0};
        ASSERT(rc_mismatch_int(v1, v2) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("one empty: no overlap, returns 0");
    {
        int data[] = {1, 2, 3};
        rc_view_int v1 = RC_VIEW(data);
        rc_view_int v2 = {NULL, 0};
        ASSERT(rc_mismatch_int(v1, v2) == 0);
        ASSERT(rc_mismatch_int(v2, v1) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("identical views: returns n");
    {
        int data[] = {1, 2, 3, 4};
        rc_view_int v1 = RC_VIEW(data);
        rc_view_int v2 = RC_VIEW(data);
        ASSERT(rc_mismatch_int(v1, v2) == 4);
    }
    END_GROUP();

    BEGIN_GROUP("mismatch at index 0");
    {
        int a[] = {9, 2, 3};
        int b[] = {1, 2, 3};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("mismatch at middle index");
    {
        int a[] = {1, 2, 9, 4};
        int b[] = {1, 2, 3, 4};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("mismatch at last index");
    {
        int a[] = {1, 2, 3, 9};
        int b[] = {1, 2, 3, 4};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("equal overlap, view1 longer: returns min length");
    {
        int a[] = {1, 2, 3, 4, 5};
        int b[] = {1, 2, 3};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("equal overlap, view2 longer: returns min length");
    {
        int a[] = {1, 2};
        int b[] = {1, 2, 3, 4};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("mismatch within shorter view");
    {
        int a[] = {1, 9, 3, 4, 5};
        int b[] = {1, 2, 3};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 1);
    }
    END_GROUP();

    BEGIN_GROUP("single element: match");
    {
        int a[] = {7};
        int b[] = {7};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 1);
    }
    END_GROUP();

    BEGIN_GROUP("single element: mismatch");
    {
        int a[] = {7};
        int b[] = {8};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        ASSERT(rc_mismatch_int(v1, v2) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("custom predicate: Record match by key");
    {
        Record a[] = {{1,0},{2,0},{3,0}};
        Record b[] = {{1,9},{2,9},{4,9}};   /* key 3 vs 4 at index 2 */
        rc_view_Record v1 = RC_VIEW(a);
        rc_view_Record v2 = RC_VIEW(b);
        ASSERT(record_mismatch_by_key(v1, v2) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("custom predicate: Record all keys match");
    {
        Record a[] = {{1,0},{2,0},{3,0}};
        Record b[] = {{1,9},{2,9},{3,9}};
        rc_view_Record v1 = RC_VIEW(a);
        rc_view_Record v2 = RC_VIEW(b);
        ASSERT(record_mismatch_by_key(v1, v2) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("context: tolerance=1, values within tolerance match");
    {
        int a[] = {10, 20, 30};
        int b[] = {10, 21, 32};   /* 21-20=1 ok; 32-30=2 mismatch at index 2 */
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        TolCtx ctx = {1};
        ASSERT(int_mismatch_tol(v1, v2, &ctx) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("context: tolerance=0 is exact equality");
    {
        int a[] = {1, 2, 3};
        int b[] = {1, 2, 3};
        rc_view_int v1 = RC_VIEW(a);
        rc_view_int v2 = RC_VIEW(b);
        TolCtx ctx = {0};
        ASSERT(int_mismatch_tol(v1, v2, &ctx) == 3);
    }
    END_GROUP();
}

/* ---- accumulate ---- */

static void test_accumulate(void)
{
    printf("accumulate\n");

    /* --- default addition (int) --- */

    BEGIN_GROUP("empty view: returns init unchanged");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(rc_accumulate_int(v, 42) == 42);
    }
    END_GROUP();

    BEGIN_GROUP("single element");
    {
        int arr[] = {7};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_accumulate_int(v, 3) == 10);
    }
    END_GROUP();

    BEGIN_GROUP("sum 1..5, zero init");
    {
        int arr[] = {1, 2, 3, 4, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_accumulate_int(v, 0) == 15);
    }
    END_GROUP();

    BEGIN_GROUP("sum with non-zero init");
    {
        int arr[] = {1, 2, 3};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_accumulate_int(v, 10) == 16);
    }
    END_GROUP();

    BEGIN_GROUP("negative elements");
    {
        int arr[] = {-1, -2, -3};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_accumulate_int(v, 0) == -6);
    }
    END_GROUP();

    BEGIN_GROUP("mixed positive and negative");
    {
        int arr[] = {10, -3, 5, -2};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_accumulate_int(v, 0) == 10);
    }
    END_GROUP();

    BEGIN_GROUP("all elements zero: result equals init");
    {
        int arr[] = {0, 0, 0};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(rc_accumulate_int(v, 5) == 5);
    }
    END_GROUP();

    /* --- custom func: product --- */

    BEGIN_GROUP("product: empty view returns init");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(int_product(v, 1) == 1);
        ASSERT(int_product(v, 99) == 99); /* init unchanged regardless of value */
    }
    END_GROUP();

    BEGIN_GROUP("product: single element");
    {
        int arr[] = {7};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(int_product(v, 1) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("product: 1..5");
    {
        int arr[] = {1, 2, 3, 4, 5};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(int_product(v, 1) == 120);
    }
    END_GROUP();

    BEGIN_GROUP("product: init=0 annihilator");
    {
        int arr[] = {1, 2, 3};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(int_product(v, 0) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("product: zero element in sequence");
    {
        int arr[] = {1, 2, 0, 3, 4};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(int_product(v, 1) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("product: negative elements");
    {
        int arr[] = {-1, -2, -3};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(int_product(v, 1) == -6);
        /* even number of negatives */
        int arr2[] = {-2, -3};
        rc_view_int v2 = RC_VIEW(arr2);
        ASSERT(int_product(v2, 1) == 6);
    }
    END_GROUP();

    /* --- heterogeneous types: int elements → double result --- */

    BEGIN_GROUP("heterogeneous: empty view preserves init");
    {
        rc_view_int v = {NULL, 0};
        ASSERT(int_sum_to_double(v, 3.14) == 3.14);
    }
    END_GROUP();

    BEGIN_GROUP("heterogeneous: int sum into double accumulator");
    {
        int arr[] = {1, 2, 3};
        rc_view_int v = RC_VIEW(arr);
        ASSERT(int_sum_to_double(v, 0.5) == 6.5);
    }
    END_GROUP();

    BEGIN_GROUP("heterogeneous: fractional init preserved through empty");
    {
        int arr[] = {0};
        rc_view_int v = RC_VIEW(arr);
        /* 0.25 + 0 = 0.25 — integer zero does not corrupt fractional init */
        ASSERT(int_sum_to_double(v, 0.25) == 0.25);
    }
    END_GROUP();

    /* --- struct element type: Record key sum --- */

    BEGIN_GROUP("Record key sum: empty view");
    {
        rc_view_Record v = {NULL, 0};
        ASSERT(Record_key_sum(v, 99) == 99);
    }
    END_GROUP();

    BEGIN_GROUP("Record key sum: sums keys, ignores payload");
    {
        Record arr[] = {{1, 100}, {2, 200}, {3, 300}};
        rc_view_Record v = RC_VIEW(arr);
        ASSERT(Record_key_sum(v, 0) == 6);
    }
    END_GROUP();

    BEGIN_GROUP("Record key sum: non-zero init");
    {
        Record arr[] = {{1, 0}, {2, 0}, {3, 0}};
        rc_view_Record v = RC_VIEW(arr);
        ASSERT(Record_key_sum(v, 100) == 106);
    }
    END_GROUP();

    /* --- context: weighted sum --- */

    BEGIN_GROUP("context: empty view returns init");
    {
        rc_view_int  v   = {NULL, 0};
        WeightCtx ctx = {.weight = 5};
        ASSERT(int_weighted_sum(v, &ctx, 10) == 10);
    }
    END_GROUP();

    BEGIN_GROUP("context: weight=3, sum(1..3)*3");
    {
        int arr[] = {1, 2, 3};
        rc_view_int  v   = RC_VIEW(arr);
        WeightCtx ctx = {.weight = 3};
        ASSERT(int_weighted_sum(v, &ctx, 0) == 18);
    }
    END_GROUP();

    BEGIN_GROUP("context: weight=0 — all contributions vanish");
    {
        int arr[] = {1, 2, 3, 4, 5};
        rc_view_int  v   = RC_VIEW(arr);
        WeightCtx ctx = {.weight = 0};
        ASSERT(int_weighted_sum(v, &ctx, 100) == 100);
    }
    END_GROUP();

    BEGIN_GROUP("context: weight=1 equivalent to plain sum");
    {
        int arr[] = {1, 2, 3, 4, 5};
        rc_view_int  v   = RC_VIEW(arr);
        WeightCtx ctx = {.weight = 1};
        ASSERT(int_weighted_sum(v, &ctx, 0) == 15);
    }
    END_GROUP();

    BEGIN_GROUP("context: non-zero init with weight");
    {
        int arr[] = {2, 4};
        rc_view_int  v   = RC_VIEW(arr);
        WeightCtx ctx = {.weight = 10};
        /* 7 + 2*10 + 4*10 = 7 + 60 = 67 */
        ASSERT(int_weighted_sum(v, &ctx, 7) == 67);
    }
    END_GROUP();

    /* --- default func with context: verifies the (void)ctx path --- */

    BEGIN_GROUP("default func with context: (void)ctx path");
    {
        int arr[] = {1, 2, 3};
        rc_view_int  v   = RC_VIEW(arr);
        WeightCtx ctx = {.weight = 999}; /* ignored by default func */
        ASSERT(int_accumulate_default_ctx(v, &ctx, 0) == 6);
    }
    END_GROUP();
}

/* ---- hash ---- */

static void test_hash(void)
{
    printf("hash\n");

    BEGIN_GROUP("u32: same input same output");
    {
        ASSERT(rc_hash_u32(0)    == rc_hash_u32(0));
        ASSERT(rc_hash_u32(1)    == rc_hash_u32(1));
        ASSERT(rc_hash_u32(UINT32_MAX) == rc_hash_u32(UINT32_MAX));
    }
    END_GROUP();

    BEGIN_GROUP("u32: different inputs produce different hashes");
    {
        ASSERT(rc_hash_u32(0) != rc_hash_u32(1));
        ASSERT(rc_hash_u32(1) != rc_hash_u32(2));
        ASSERT(rc_hash_u32(0) != rc_hash_u32(UINT32_MAX));
    }
    END_GROUP();

    BEGIN_GROUP("i32: negative values hash consistently");
    {
        ASSERT(rc_hash_i32(-1)  == rc_hash_i32(-1));
        ASSERT(rc_hash_i32(-1)  != rc_hash_i32(1));
        ASSERT(rc_hash_i32(INT32_MIN) == rc_hash_i32(INT32_MIN));
    }
    END_GROUP();

    BEGIN_GROUP("u64: same input same output");
    {
        ASSERT(rc_hash_u64(0)              == rc_hash_u64(0));
        ASSERT(rc_hash_u64(UINT64_MAX)     == rc_hash_u64(UINT64_MAX));
        ASSERT(rc_hash_u64(0) != rc_hash_u64(1));
    }
    END_GROUP();

    BEGIN_GROUP("u64: high bits affect output");
    {
        /* Values that differ only in high 32 bits must hash differently. */
        ASSERT(rc_hash_u64(UINT64_C(1) << 32) != rc_hash_u64(1));
        ASSERT(rc_hash_u64(UINT64_C(1) << 32) != rc_hash_u64(0));
    }
    END_GROUP();

    BEGIN_GROUP("f32: -0.0f and +0.0f hash identically");
    {
        ASSERT(rc_hash_f32(-0.0f) == rc_hash_f32(+0.0f));
    }
    END_GROUP();

    BEGIN_GROUP("f32: distinct values hash differently");
    {
        ASSERT(rc_hash_f32(1.0f)  != rc_hash_f32(2.0f));
        ASSERT(rc_hash_f32(-1.0f) != rc_hash_f32(1.0f));
    }
    END_GROUP();

    BEGIN_GROUP("f64: -0.0 and +0.0 hash identically");
    {
        ASSERT(rc_hash_f64(-0.0) == rc_hash_f64(+0.0));
    }
    END_GROUP();

    BEGIN_GROUP("bytes: empty input is consistent");
    {
        ASSERT(rc_hash_bytes(NULL, 0) == rc_hash_bytes(NULL, 0));
        uint8_t buf[] = {0};
        ASSERT(rc_hash_bytes(buf, 0)  == rc_hash_bytes(NULL, 0));
    }
    END_GROUP();

    BEGIN_GROUP("bytes: single byte differs from empty and from other bytes");
    {
        uint8_t a = 'A', b = 'B';
        ASSERT(rc_hash_bytes(&a, 1) != rc_hash_bytes(NULL, 0));
        ASSERT(rc_hash_bytes(&a, 1) != rc_hash_bytes(&b, 1));
        ASSERT(rc_hash_bytes(&a, 1) == rc_hash_bytes(&a, 1));
    }
    END_GROUP();

    BEGIN_GROUP("bytes: byte order matters");
    {
        uint8_t ab[] = {'A', 'B'};
        uint8_t ba[] = {'B', 'A'};
        ASSERT(rc_hash_bytes(ab, 2) != rc_hash_bytes(ba, 2));
    }
    END_GROUP();

    BEGIN_GROUP("str: consistent for same string");
    {
        ASSERT(rc_hash_str(RC_STR("hello")) == rc_hash_str(RC_STR("hello")));
        ASSERT(rc_hash_str(RC_STR(""))      == rc_hash_str(RC_STR("")));
    }
    END_GROUP();

    BEGIN_GROUP("str: different strings hash differently");
    {
        ASSERT(rc_hash_str(RC_STR("hello")) != rc_hash_str(RC_STR("world")));
        ASSERT(rc_hash_str(RC_STR("ab"))    != rc_hash_str(RC_STR("ba")));
    }
    END_GROUP();

    BEGIN_GROUP("str: empty string same as invalid (NULL, 0)");
    {
        rc_str invalid = {0};
        ASSERT(rc_hash_str(RC_STR("")) == rc_hash_str(invalid));
    }
    END_GROUP();

    BEGIN_GROUP("combine: same inputs same output");
    {
        ASSERT(rc_hash_combine(0, 0)             == rc_hash_combine(0, 0));
        ASSERT(rc_hash_combine(42, 17)           == rc_hash_combine(42, 17));
    }
    END_GROUP();

    BEGIN_GROUP("combine: order matters");
    {
        ASSERT(rc_hash_combine(1, 2) != rc_hash_combine(2, 1));
    }
    END_GROUP();

    BEGIN_GROUP("combine: composing two fields differs from either alone");
    {
        uint32_t hx = rc_hash_i32(3);
        uint32_t hy = rc_hash_i32(7);
        uint32_t hxy = rc_hash_combine(hx, hy);
        ASSERT(hxy != hx);
        ASSERT(hxy != hy);
    }
    END_GROUP();

    BEGIN_GROUP("vec2i: consistent and field-sensitive");
    {
        rc_vec2i a = {1, 2}, b = {2, 1}, c = {1, 2};
        ASSERT(rc_hash_vec2i(a) == rc_hash_vec2i(c));
        ASSERT(rc_hash_vec2i(a) != rc_hash_vec2i(b));
    }
    END_GROUP();

    BEGIN_GROUP("vec3i: consistent and field-sensitive");
    {
        rc_vec3i a = {1, 2, 3}, b = {1, 3, 2}, c = {1, 2, 3};
        ASSERT(rc_hash_vec3i(a) == rc_hash_vec3i(c));
        ASSERT(rc_hash_vec3i(a) != rc_hash_vec3i(b));
    }
    END_GROUP();

    BEGIN_GROUP("vec2f: consistent; -0/+0 components treated equally");
    {
        rc_vec2f a = {-0.0f, 1.0f}, b = {+0.0f, 1.0f};
        ASSERT(rc_hash_vec2f(a) == rc_hash_vec2f(b));
        rc_vec2f c = {1.0f, 2.0f}, d = {2.0f, 1.0f};
        ASSERT(rc_hash_vec2f(c) != rc_hash_vec2f(d));
    }
    END_GROUP();

    BEGIN_GROUP("vec4f: consistent and field-sensitive");
    {
        rc_vec4f a = {1,2,3,4}, b = {1,2,3,5}, c = {1,2,3,4};
        ASSERT(rc_hash_vec4f(a) == rc_hash_vec4f(c));
        ASSERT(rc_hash_vec4f(a) != rc_hash_vec4f(b));
    }
    END_GROUP();

    BEGIN_GROUP("rational: consistent with canonical equality");
    {
        rc_rational a = rc_rational_make(1, 2);
        rc_rational b = rc_rational_make(2, 4);  /* same value; canonical: 1/2 */
        rc_rational c = rc_rational_make(1, 3);
        rc_rational d = rc_rational_make(-1, 2);
        rc_rational e = rc_rational_from_i64(3); /* 3/1 */
        rc_rational f = rc_rational_from_i64(3);
        /* Equal rationals must hash equally. */
        ASSERT(rc_hash_rational(a) == rc_hash_rational(b));
        ASSERT(rc_hash_rational(e) == rc_hash_rational(f));
        /* Different values should (almost certainly) differ. */
        ASSERT(rc_hash_rational(a) != rc_hash_rational(c));
        ASSERT(rc_hash_rational(a) != rc_hash_rational(d));
        ASSERT(rc_hash_rational(a) != rc_hash_rational(e));
        /* Zero: 0/1. */
        rc_rational z = rc_rational_from_i64(0);
        ASSERT(rc_hash_rational(z) == rc_hash_rational(rc_rational_make(0, 7)));
    }
    END_GROUP();
}

/* ---- hash multimap ---- */

static void test_multimap(void)
{
    printf("hash multimap\n");

    BEGIN_GROUP("pool_make / make: initial state");
    {
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        ASSERT(pool.len == 0 && pool.cap == 0 && pool.nodes == NULL);
        ASSERT(pool.free_head == 0);
        rc_mmap_int m = rc_mmap_int_make(&pool);
        ASSERT(m.count == 0 && m.cap == 0 && m.data == NULL);
        ASSERT(m.pool == &pool);
    }
    END_GROUP();

    BEGIN_GROUP("empty map: find/contains/remove safe");
    {
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        ASSERT(!rc_mmap_int_contains(&m, 42));
        ASSERT(rc_mmap_int_find_head(&m, 42) == RC_INDEX_NONE);
        ASSERT(rc_mmap_int_next(&m, 0) == m.cap);
        ASSERT(!rc_mmap_int_remove_all(&m, 42));
    }
    END_GROUP();

    BEGIN_GROUP("add new key: returns 1, chain has one node");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        ASSERT(rc_mmap_int_add(&m, 1, 100, &a) == 1);
        ASSERT(m.count == 1);
        ASSERT(pool.len == 1);
        ASSERT(rc_mmap_int_contains(&m, 1));
        uint32_t h = rc_mmap_int_find_head(&m, 1);
        ASSERT(h != RC_INDEX_NONE);
        ASSERT(*rc_mmap_int_node_val(&m, h) == 100);
        ASSERT(rc_mmap_int_node_next(&m, h) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("add existing key: returns 0, value prepended");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 100, &a);
        ASSERT(rc_mmap_int_add(&m, 1, 200, &a) == 0);
        ASSERT(m.count == 1);
        ASSERT(pool.len == 2);
        uint32_t h  = rc_mmap_int_find_head(&m, 1);
        uint32_t h2 = rc_mmap_int_node_next(&m, h);
        ASSERT(*rc_mmap_int_node_val(&m, h)  == 200);   /* newest first */
        ASSERT(*rc_mmap_int_node_val(&m, h2) == 100);
        ASSERT(rc_mmap_int_node_next(&m, h2) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("three values for one key: chain order correct");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 100, &a);
        rc_mmap_int_add(&m, 1, 200, &a);
        ASSERT(rc_mmap_int_add(&m, 1, 300, &a) == 0);
        uint32_t h  = rc_mmap_int_find_head(&m, 1);
        uint32_t h2 = rc_mmap_int_node_next(&m, h);
        uint32_t h3 = rc_mmap_int_node_next(&m, h2);
        ASSERT(*rc_mmap_int_node_val(&m, h)  == 300);
        ASSERT(*rc_mmap_int_node_val(&m, h2) == 200);
        ASSERT(*rc_mmap_int_node_val(&m, h3) == 100);
        ASSERT(rc_mmap_int_node_next(&m, h3) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("multiple keys: count and contains correct");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        ASSERT(rc_mmap_int_add(&m, 1, 10, &a) == 1);
        ASSERT(rc_mmap_int_add(&m, 2, 20, &a) == 1);
        ASSERT(rc_mmap_int_add(&m, 3, 30, &a) == 1);
        ASSERT(m.count == 3);
        ASSERT(rc_mmap_int_contains(&m, 1));
        ASSERT(rc_mmap_int_contains(&m, 2));
        ASSERT(rc_mmap_int_contains(&m, 3));
        ASSERT(!rc_mmap_int_contains(&m, 4));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at/head_at: all keys visited exactly once");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 100, &a);
        rc_mmap_int_add(&m, 1, 200, &a);
        rc_mmap_int_add(&m, 2, 300, &a);
        uint32_t key_count = 0;
        int val_sum = 0;
        for (uint32_t i = rc_mmap_int_next(&m, 0); i < m.cap; i = rc_mmap_int_next(&m, i + 1)) {
            key_count++;
            for (uint32_t j = rc_mmap_int_head_at(&m, i); j != RC_INDEX_NONE; j = rc_mmap_int_node_next(&m, j))
                val_sum += *rc_mmap_int_node_val(&m, j);
        }
        ASSERT(key_count == 2);
        ASSERT(val_sum == 100 + 200 + 300);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove_all: key gone, nodes freed, absent returns 0");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 100, &a);
        rc_mmap_int_add(&m, 1, 200, &a);
        rc_mmap_int_add(&m, 1, 300, &a);
        rc_mmap_int_add(&m, 2, 400, &a);
        ASSERT(rc_mmap_int_remove_all(&m, 1) == 1);
        ASSERT(m.count == 1);
        ASSERT(!rc_mmap_int_contains(&m, 1));
        ASSERT(pool.free_head != 0);   /* three nodes on free-list */
        ASSERT(!rc_mmap_int_remove_all(&m, 1)); /* already absent */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("free-list reuse: pool.len stable after remove+add");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 100, &a);
        rc_mmap_int_add(&m, 1, 200, &a);
        rc_mmap_int_remove_all(&m, 1);
        uint32_t len_before = pool.len;
        rc_mmap_int_add(&m, 1, 999, &a);   /* new key; should reuse free-list */
        ASSERT(pool.len == len_before);
        uint32_t h = rc_mmap_int_find_head(&m, 1);
        ASSERT(*rc_mmap_int_node_val(&m, h) == 999);
        ASSERT(rc_mmap_int_node_next(&m, h) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("tombstone reuse: remove_all then re-add same key");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 10, &a);
        rc_mmap_int_add(&m, 2, 20, &a);
        uint32_t used_before = m.used;
        rc_mmap_int_remove_all(&m, 1);
        ASSERT(m.count == 1);
        ASSERT(m.used == used_before);       /* tombstone: used unchanged */
        rc_mmap_int_add(&m, 1, 777, &a);    /* reuses tombstone slot */
        ASSERT(m.count == 2);
        ASSERT(m.used == used_before);       /* slot recycled, used still same */
        ASSERT(rc_mmap_int_contains(&m, 1));
        ASSERT(*rc_mmap_int_node_val(&m, rc_mmap_int_find_head(&m, 1)) == 777);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: pre-allocates hash table slots");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_reserve(&m, 100, &a);
        ASSERT(m.cap >= 128);   /* smallest power-of-two s.t. floor(cap*3/4)>=100 */
        ASSERT(m.count == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pool_reserve: pre-allocates pool nodes");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int_pool_reserve(&pool, 64, &a);
        ASSERT(pool.cap >= 64);
        ASSERT(pool.len == 0);   /* no nodes allocated, just capacity reserved */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("shared pool: two maps draw from and return to same pool");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool shared = rc_mmap_int_pool_make();
        rc_mmap_int ma = rc_mmap_int_make(&shared);
        rc_mmap_int mb = rc_mmap_int_make(&shared);
        rc_mmap_int_add(&ma, 10, 1, &a);
        rc_mmap_int_add(&mb, 20, 2, &a);
        ASSERT(shared.len == 2);
        ASSERT( rc_mmap_int_contains(&ma, 10) && !rc_mmap_int_contains(&ma, 20));
        ASSERT( rc_mmap_int_contains(&mb, 20) && !rc_mmap_int_contains(&mb, 10));
        rc_mmap_int_remove_all(&ma, 10);
        uint32_t len_before = shared.len;
        rc_mmap_int_add(&mb, 30, 3, &a);
        ASSERT(shared.len == len_before);   /* mb recycled the node freed by ma */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rehash: 200 keys, all data survives growth");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        for (int k = 0; k < 200; k++)
            rc_mmap_int_add(&m, k, k * 10, &a);
        ASSERT(m.count == 200);
        int ok = 1;
        for (int k = 0; k < 200; k++) {
            if (!rc_mmap_int_contains(&m, k)) { ok = 0; break; }
            uint32_t h = rc_mmap_int_find_head(&m, k);
            if (*rc_mmap_int_node_val(&m, h) != k * 10) { ok = 0; break; }
        }
        ASSERT(ok);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("multiple values per key after rehash: chains intact");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        for (int k = 0; k < 200; k++)
            rc_mmap_int_add(&m, k, k * 10, &a);
        for (int k = 0; k < 200; k++)
            rc_mmap_int_add(&m, k, k * 20, &a);
        ASSERT(m.count == 200);
        int ok = 1;
        for (int k = 0; k < 200; k++) {
            uint32_t h  = rc_mmap_int_find_head(&m, k);
            uint32_t h2 = rc_mmap_int_node_next(&m, h);
            if (*rc_mmap_int_node_val(&m, h)  != k * 20) { ok = 0; break; }
            if (*rc_mmap_int_node_val(&m, h2) != k * 10) { ok = 0; break; }
            if (rc_mmap_int_node_next(&m, h2) != RC_INDEX_NONE) { ok = 0; break; }
        }
        ASSERT(ok);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove_all half of keys: count and iteration correct");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        for (int k = 0; k < 200; k++)
            rc_mmap_int_add(&m, k, k * 10, &a);
        for (int k = 0; k < 100; k++)
            ASSERT(rc_mmap_int_remove_all(&m, k) == 1);
        ASSERT(m.count == 100);
        int ok = 1;
        for (int k = 0; k < 100; k++)
            if (rc_mmap_int_contains(&m, k)) { ok = 0; break; }
        for (int k = 100; k < 200; k++)
            if (!rc_mmap_int_contains(&m, k)) { ok = 0; break; }
        ASSERT(ok);
        uint32_t cnt = 0;
        for (uint32_t i = rc_mmap_int_next(&m, 0); i < m.cap; i = rc_mmap_int_next(&m, i + 1))
            cnt++;
        ASSERT(cnt == 100);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("find_head / contains / remove_all on absent key");
    {
        rc_arena a = rc_arena_make_default();
        rc_mmap_int_pool pool = rc_mmap_int_pool_make();
        rc_mmap_int m = rc_mmap_int_make(&pool);
        rc_mmap_int_add(&m, 1, 10, &a);
        ASSERT(rc_mmap_int_find_head(&m, 9999) == RC_INDEX_NONE);
        ASSERT(!rc_mmap_int_contains(&m, 9999));
        ASSERT(!rc_mmap_int_remove_all(&m, 9999));
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- hash map ---- */

static void test_hash_map(void)
{
    printf("hash map\n");

    BEGIN_GROUP("zero-init: find/contains/remove safe");
    {
        rc_map_int m = {0};
        ASSERT(rc_map_int_find(&m, 42)     == NULL);
        ASSERT(rc_map_int_contains(&m, 42) == 0);
        ASSERT(rc_map_int_remove(&m, 42)   == 0);
        ASSERT(m.count == 0);
    }
    END_GROUP();

    BEGIN_GROUP("add new key: returns 1, find/contains work");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        ASSERT(rc_map_int_add(&m, 7, 42, &a) == 1);    /* new key */
        ASSERT(m.count == 1);
        ASSERT(rc_map_int_contains(&m, 7));
        int *v = rc_map_int_find(&m, 7);
        ASSERT(v != NULL && *v == 42);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("add existing key: returns 0, value updated");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        rc_map_int_add(&m, 5, 10, &a);
        ASSERT(rc_map_int_add(&m, 5, 99, &a) == 0);    /* existing key */
        ASSERT(m.count == 1);
        int *v = rc_map_int_find(&m, 5);
        ASSERT(v != NULL && *v == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove: key gone; absent key returns 0");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        rc_map_int_add(&m, 3, 30, &a);
        rc_map_int_add(&m, 4, 40, &a);
        ASSERT(rc_map_int_remove(&m, 3) == 1);         /* present */
        ASSERT(m.count == 1);
        ASSERT(rc_map_int_find(&m, 3)     == NULL);
        ASSERT(rc_map_int_contains(&m, 3) == 0);
        ASSERT(rc_map_int_find(&m, 4)     != NULL);    /* other key intact */
        ASSERT(rc_map_int_remove(&m, 3)   == 0);       /* already gone */
        ASSERT(rc_map_int_remove(&m, 99)  == 0);       /* never existed */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("multiple entries: 20 pairs, all findable");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        for (int i = 0; i < 20; i++)
            rc_map_int_add(&m, i, i * 10, &a);
        ASSERT(m.count == 20);
        for (int i = 0; i < 20; i++) {
            int *v = rc_map_int_find(&m, i);
            ASSERT(v != NULL && *v == i * 10);
        }
        ASSERT(rc_map_int_find(&m, 20) == NULL);       /* not inserted */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rehash: all entries survive growth");
    {
        /* Initial cap = 8.  At the 7th insertion, used+1 = 7, and
         * 7*4=28 > 8*3=24, so the map rehashes to cap=16.
         * Keep inserting to trigger a second rehash (cap=16 → 32). */
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        for (int i = 0; i < 25; i++)
            rc_map_int_add(&m, i * 7, i, &a);   /* spread keys via *7 */
        ASSERT(m.count == 25);
        ASSERT(m.cap   >= 32);
        for (int i = 0; i < 25; i++) {
            int *v = rc_map_int_find(&m, i * 7);
            ASSERT(v != NULL && *v == i);
        }
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("tombstone reuse: remove then re-add same key");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        rc_map_int_add(&m, 1, 100, &a);
        rc_map_int_add(&m, 2, 200, &a);
        size_t used_before = m.used;
        rc_map_int_remove(&m, 1);              /* leaves tombstone */
        ASSERT(m.count == 1);
        ASSERT(m.used  == used_before);     /* used unchanged by remove */
        rc_map_int_add(&m, 1, 111, &a);        /* re-add: reuses tombstone */
        ASSERT(m.count == 2);
        ASSERT(m.used  == used_before);     /* tombstone slot reused */
        int *v = rc_map_int_find(&m, 1);
        ASSERT(v != NULL && *v == 111);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: NULL rc_arena ok when cap sufficient");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        rc_map_int_reserve(&m, 10, &a);        /* allocate with rc_arena   */
        ASSERT(m.cap >= 16);                /* need cap s.t. 3/4*cap >= 10 */
        /* All subsequent operations that don't exceed reserved cap
         * work with NULL rc_arena. */
        for (int i = 0; i < 10; i++)
            rc_map_int_add(&m, i, i, NULL);    /* no rehash needed      */
        ASSERT(m.count == 10);
        for (int i = 0; i < 10; i++)
            ASSERT(rc_map_int_remove(&m, i));  /* remove needs no rc_arena */
        ASSERT(m.count == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("int_Record_Map: int key, Record value");
    {
        rc_arena a = rc_arena_make_default();
        int_Record_Map m = {0};
        int_Record_Map_add(&m, 10, (Record) {10, 100}, &a);
        int_Record_Map_add(&m, 20, (Record) {20, 200}, &a);
        ASSERT(m.count == 2);
        Record *r = int_Record_Map_find(&m, 10);
        ASSERT(r != NULL && r->key == 10 && r->payload == 100);
        r = int_Record_Map_find(&m, 20);
        ASSERT(r != NULL && r->key == 20 && r->payload == 200);
        ASSERT(int_Record_Map_find(&m, 30) == NULL);
        /* Update via returned pointer. */
        r = int_Record_Map_find(&m, 10);
        r->payload = 999;
        r = int_Record_Map_find(&m, 10);
        ASSERT(r->payload == 999);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("Record_int_Map: struct key with custom equality");
    {
        rc_arena a = rc_arena_make_default();
        Record_int_Map m = {0};
        Record k1 = {1, 0};    /* payload ignored by MAP_EQUAL */
        Record k2 = {2, 0};
        Record k3 = {1, 99};   /* same key as k1 despite different payload */
        Record_int_Map_add(&m, k1, 10, &a);
        Record_int_Map_add(&m, k2, 20, &a);
        ASSERT(m.count == 2);
        ASSERT(Record_int_Map_contains(&m, k1));
        ASSERT(Record_int_Map_contains(&m, k2));
        /* k3 has the same .key as k1: should hit the same slot */
        ASSERT(Record_int_Map_add(&m, k3, 30, &a) == 0); /* update */
        ASSERT(m.count == 2);
        int *v = Record_int_Map_find(&m, k1);
        ASSERT(v != NULL && *v == 30);              /* updated via k3 */
        ASSERT(Record_int_Map_remove(&m, k2) == 1);
        ASSERT(!Record_int_Map_contains(&m, k2));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at/val_at: iterate empty map");
    {
        rc_map_int m = {0};
        uint32_t i = rc_map_int_next(&m, 0);
        ASSERT(i == m.cap);   /* immediately at end (cap == 0) */
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at/val_at: all entries visited exactly once");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        for (int k = 0; k < 10; k++)
            rc_map_int_add(&m, k, k * 10, &a);

        /* Collect keys and values via iteration. */
        int seen_keys[10] = {0};
        int count = 0;
        for (uint32_t i = rc_map_int_next(&m, 0); i < m.cap;
             i = rc_map_int_next(&m, i + 1))
        {
            int k = rc_map_int_key_at(&m, i);
            int *v = rc_map_int_val_at(&m, i);
            ASSERT(k >= 0 && k < 10);
            ASSERT(*v == k * 10);
            seen_keys[k]++;
            count++;
        }
        ASSERT(count == 10);
        for (int k = 0; k < 10; k++)
            ASSERT(seen_keys[k] == 1);   /* each key seen exactly once */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at/val_at: val_at pointer is mutable");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        rc_map_int_add(&m, 1, 100, &a);
        rc_map_int_add(&m, 2, 200, &a);

        /* Double each value via val_at. */
        for (uint32_t i = rc_map_int_next(&m, 0); i < m.cap;
             i = rc_map_int_next(&m, i + 1))
            *rc_map_int_val_at(&m, i) *= 2;

        int *v1 = rc_map_int_find(&m, 1);
        int *v2 = rc_map_int_find(&m, 2);
        ASSERT(v1 != NULL && *v1 == 200);
        ASSERT(v2 != NULL && *v2 == 400);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at/val_at: tombstones skipped");
    {
        rc_arena a = rc_arena_make_default();
        rc_map_int m = {0};
        for (int k = 0; k < 5; k++)
            rc_map_int_add(&m, k, k, &a);
        rc_map_int_remove(&m, 1);
        rc_map_int_remove(&m, 3);

        int count = 0;
        for (uint32_t i = rc_map_int_next(&m, 0); i < m.cap;
             i = rc_map_int_next(&m, i + 1))
        {
            int k = rc_map_int_key_at(&m, i);
            ASSERT(k != 1 && k != 3);   /* removed keys must not appear */
            count++;
        }
        ASSERT(count == 3);
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- hash set ---- */

static void test_hash_set(void)
{
    printf("hash set\n");

    BEGIN_GROUP("zero-init: contains/remove safe");
    {
        rc_set_int s = {0};
        ASSERT(rc_set_int_contains(&s, 42) == 0);
        ASSERT(rc_set_int_remove(&s, 42)   == 0);
        ASSERT(s.count == 0);
    }
    END_GROUP();

    BEGIN_GROUP("add new key: returns 1, contains works");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        ASSERT(rc_set_int_add(&s, 7, &a) == 1);    /* new key */
        ASSERT(s.count == 1);
        ASSERT(rc_set_int_contains(&s, 7)  == 1);
        ASSERT(rc_set_int_contains(&s, 99) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("add existing key: returns 0, count unchanged");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        ASSERT(rc_set_int_add(&s, 5, &a) == 1);
        ASSERT(rc_set_int_add(&s, 5, &a) == 0);    /* already present */
        ASSERT(s.count == 1);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove: key gone; absent key returns 0");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        rc_set_int_add(&s, 3, &a);
        rc_set_int_add(&s, 4, &a);
        ASSERT(rc_set_int_remove(&s, 3)   == 1);   /* present */
        ASSERT(s.count == 1);
        ASSERT(rc_set_int_contains(&s, 3) == 0);
        ASSERT(rc_set_int_contains(&s, 4) == 1);   /* other key intact */
        ASSERT(rc_set_int_remove(&s, 3)   == 0);   /* already gone */
        ASSERT(rc_set_int_remove(&s, 99)  == 0);   /* never existed */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("multiple entries: 20 keys, all present");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        for (int i = 0; i < 20; i++)
            rc_set_int_add(&s, i, &a);
        ASSERT(s.count == 20);
        for (int i = 0; i < 20; i++)
            ASSERT(rc_set_int_contains(&s, i) == 1);
        ASSERT(rc_set_int_contains(&s, 20) == 0);  /* not inserted */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("rehash: all entries survive growth");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        for (int i = 0; i < 100; i++)
            rc_set_int_add(&s, i, &a);
        ASSERT(s.count == 100);
        for (int i = 0; i < 100; i++)
            ASSERT(rc_set_int_contains(&s, i) == 1);
        for (int i = 100; i < 110; i++)
            ASSERT(rc_set_int_contains(&s, i) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("remove then re-add: tombstone reuse");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        rc_set_int_add(&s, 1, &a);
        rc_set_int_add(&s, 2, &a);
        rc_set_int_remove(&s, 1);
        ASSERT(s.count == 1);
        ASSERT(rc_set_int_add(&s, 1, &a) == 1);    /* re-insert into tombstone */
        ASSERT(s.count == 2);
        ASSERT(rc_set_int_contains(&s, 1) == 1);
        ASSERT(rc_set_int_contains(&s, 2) == 1);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: pre-alloc avoids rehash");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        rc_set_int_reserve(&s, 50, &a);
        ASSERT(s.cap >= 64);
        uint32_t cap_before = s.cap;
        for (int i = 0; i < 40; i++)
            rc_set_int_add(&s, i, &a);
        ASSERT(s.cap == cap_before);   /* no rehash needed */
        ASSERT(s.count == 40);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("custom key type: Record_set with .key equality");
    {
        rc_arena a = rc_arena_make_default();
        Record_set s = {0};
        Record r1 = {10, 0};
        Record r2 = {20, 0};
        Record r3 = {10, 99};  /* same .key as r1, different .val */
        ASSERT(Record_set_add(&s, r1, &a) == 1);
        ASSERT(Record_set_add(&s, r2, &a) == 1);
        ASSERT(s.count == 2);
        ASSERT(Record_set_contains(&s, r1) == 1);
        ASSERT(Record_set_contains(&s, r2) == 1);
        ASSERT(Record_set_add(&s, r3, &a) == 0);  /* same .key as r1 */
        ASSERT(s.count == 2);
        ASSERT(Record_set_remove(&s, r2) == 1);
        ASSERT(Record_set_contains(&s, r2) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at: iterate empty set");
    {
        rc_set_int s = {0};
        uint32_t i = rc_set_int_next(&s, 0);
        ASSERT(i == s.cap);   /* immediately at end (cap == 0) */
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at: all entries visited exactly once");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        for (int k = 0; k < 10; k++)
            rc_set_int_add(&s, k, &a);

        int seen[10] = {0};
        int count = 0;
        for (uint32_t i = rc_set_int_next(&s, 0); i < s.cap;
             i = rc_set_int_next(&s, i + 1))
        {
            int k = rc_set_int_key_at(&s, i);
            ASSERT(k >= 0 && k < 10);
            seen[k]++;
            count++;
        }
        ASSERT(count == 10);
        for (int k = 0; k < 10; k++)
            ASSERT(seen[k] == 1);   /* each key seen exactly once */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("next/key_at: tombstones skipped");
    {
        rc_arena a = rc_arena_make_default();
        rc_set_int s = {0};
        for (int k = 0; k < 5; k++)
            rc_set_int_add(&s, k, &a);
        rc_set_int_remove(&s, 1);
        rc_set_int_remove(&s, 3);

        int count = 0;
        for (uint32_t i = rc_set_int_next(&s, 0); i < s.cap;
             i = rc_set_int_next(&s, i + 1))
        {
            int k = rc_set_int_key_at(&s, i);
            ASSERT(k != 1 && k != 3);   /* removed keys must not appear */
            count++;
        }
        ASSERT(count == 3);
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- hash trie ---- */

static void test_hash_trie(void)
{
    printf("hash trie\n");

    BEGIN_GROUP("empty trie: find/delete safe");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        ASSERT(U64IntTrie_find(&t, 0)   == NULL);
        ASSERT(U64IntTrie_find(&t, 42)  == NULL);
        ASSERT(U64IntTrie_delete(&t, 0) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("add new key: returns 1, findable");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        ASSERT(U64IntTrie_add(&t, 7, 42, &a) == 1);
        int *v = U64IntTrie_find(&t, 7);
        ASSERT(v != NULL && *v == 42);
        ASSERT(U64IntTrie_find(&t, 8) == NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("add existing key: returns 0, value updated");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        U64IntTrie_add(&t, 5, 10, &a);
        ASSERT(U64IntTrie_add(&t, 5, 99, &a) == 0);
        int *v = U64IntTrie_find(&t, 5);
        ASSERT(v != NULL && *v == 99);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("delete leaf: key gone, sibling intact");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        U64IntTrie_add(&t, 1, 100, &a);
        U64IntTrie_add(&t, 2, 200, &a);
        ASSERT(U64IntTrie_delete(&t, 1) == 1);
        ASSERT(U64IntTrie_find(&t, 1)   == NULL);
        int *v = U64IntTrie_find(&t, 2);
        ASSERT(v != NULL && *v == 200);
        ASSERT(U64IntTrie_delete(&t, 1) == 0);   /* already gone */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("delete absent key returns 0");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        U64IntTrie_add(&t, 10, 1, &a);
        ASSERT(U64IntTrie_delete(&t, 99) == 0);
        ASSERT(U64IntTrie_find(&t, 10)   != NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("multiple entries: 20 distinct keys, all findable");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        for (int i = 0; i < 20; i++)
            U64IntTrie_add(&t, (uint64_t)i, i * 10, &a);
        for (int i = 0; i < 20; i++) {
            int *v = U64IntTrie_find(&t, (uint64_t)i);
            ASSERT(v != NULL && *v == i * 10);
        }
        ASSERT(U64IntTrie_find(&t, 20) == NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /*
     * Collision at level 0: keys 0x10 and 0x20 share low nibble 0.
     * hash(0x10) & 0xF == 0 == hash(0x20) & 0xF, so 0x20 is pushed into
     * 0x10's child block.  Deleting 0x10 (an interior node) must bubble
     * up 0x20 without losing it.
     */
    BEGIN_GROUP("level-0 collision: both keys findable");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        U64IntTrie_add(&t, 0x10u, 111, &a);
        U64IntTrie_add(&t, 0x20u, 222, &a);
        int *v1 = U64IntTrie_find(&t, 0x10u);
        int *v2 = U64IntTrie_find(&t, 0x20u);
        ASSERT(v1 != NULL && *v1 == 111);
        ASSERT(v2 != NULL && *v2 == 222);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("bubble-up: delete interior node, child survives");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        /* 0x10 owns slot 0 at root; 0x20 is pushed to 0x10's children. */
        U64IntTrie_add(&t, 0x10u, 111, &a);
        U64IntTrie_add(&t, 0x20u, 222, &a);
        ASSERT(U64IntTrie_delete(&t, 0x10u) == 1);
        ASSERT(U64IntTrie_find(&t, 0x10u)   == NULL);
        int *v = U64IntTrie_find(&t, 0x20u);
        ASSERT(v != NULL && *v == 222);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("bubble-up: delete promoted key, original child gone");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        U64IntTrie_add(&t, 0x10u, 111, &a);
        U64IntTrie_add(&t, 0x20u, 222, &a);
        U64IntTrie_delete(&t, 0x10u);           /* 0x20 bubbles up      */
        ASSERT(U64IntTrie_delete(&t, 0x20u) == 1); /* can still delete it  */
        ASSERT(U64IntTrie_find(&t, 0x20u)   == NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("three-way collision: all findable after each deletion");
    {
        /*
         * 0x10, 0x20, 0x30 all share low nibble 0.
         * 0x10 owns root slot 0; 0x20 and 0x30 are in the child block.
         * At level 1 (after rotating hash right 4): 0x20 >> 4 = 0x2 → slot 2;
         * 0x30 >> 4 = 0x3 → slot 3.  No further collision.
         */
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        U64IntTrie_add(&t, 0x10u, 1, &a);
        U64IntTrie_add(&t, 0x20u, 2, &a);
        U64IntTrie_add(&t, 0x30u, 3, &a);

        ASSERT(*U64IntTrie_find(&t, 0x10u) == 1);
        ASSERT(*U64IntTrie_find(&t, 0x20u) == 2);
        ASSERT(*U64IntTrie_find(&t, 0x30u) == 3);

        /* Delete the interior node; one child bubbles up. */
        ASSERT(U64IntTrie_delete(&t, 0x10u) == 1);
        ASSERT(U64IntTrie_find(&t, 0x10u) == NULL);
        /* The other two must still be accessible. */
        ASSERT(U64IntTrie_find(&t, 0x20u) != NULL);
        ASSERT(U64IntTrie_find(&t, 0x30u) != NULL);
        ASSERT(*U64IntTrie_find(&t, 0x20u) == 2);
        ASSERT(*U64IntTrie_find(&t, 0x30u) == 3);

        /* Delete both remaining keys. */
        ASSERT(U64IntTrie_delete(&t, 0x20u) == 1);
        ASSERT(U64IntTrie_delete(&t, 0x30u) == 1);
        ASSERT(U64IntTrie_find(&t, 0x20u) == NULL);
        ASSERT(U64IntTrie_find(&t, 0x30u) == NULL);

        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pool growth: 100 entries trigger realloc");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        for (int i = 0; i < 100; i++)
            U64IntTrie_add(&t, (uint64_t)(i * 17 + 3), i, &a);
        int all_ok = 1;
        for (int i = 0; i < 100; i++) {
            int *v = U64IntTrie_find(&t, (uint64_t)(i * 17 + 3));
            if (v == NULL || *v != i) {all_ok = 0; break;}
        }
        ASSERT(all_ok);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("shared pool: two tries share nodes, no cross-contamination");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie ta = U64IntTrie_create(&pool, &a);
        U64IntTrie tb = U64IntTrie_create(&pool, &a);

        U64IntTrie_add(&ta, 1, 10, &a);
        U64IntTrie_add(&ta, 2, 20, &a);
        U64IntTrie_add(&tb, 1, 99, &a);   /* same key, different trie */
        U64IntTrie_add(&tb, 3, 30, &a);

        /* Each trie sees its own values. */
        ASSERT(*U64IntTrie_find(&ta, 1) == 10);
        ASSERT(*U64IntTrie_find(&ta, 2) == 20);
        ASSERT( U64IntTrie_find(&ta, 3) == NULL);
        ASSERT(*U64IntTrie_find(&tb, 1) == 99);
        ASSERT(*U64IntTrie_find(&tb, 3) == 30);
        ASSERT( U64IntTrie_find(&tb, 2) == NULL);

        /* Deleting from one trie doesn't affect the other. */
        ASSERT(U64IntTrie_delete(&ta, 1) == 1);
        ASSERT(U64IntTrie_find(&ta, 1)   == NULL);
        ASSERT(*U64IntTrie_find(&tb, 1)  == 99);   /* tb's key 1 intact */

        rc_arena_destroy(&a);
    }
    END_GROUP();

    /*
     * Identical-hash keys: use TRIE_EQUAL to distinguish them.
     * With identity hash, keys 0 and 0 are the same key (EQUAL returns 1),
     * but we can test the hash-chain behaviour by instantiating a trie
     * where the hash ignores part of the key.  Instead, use a separate
     * approach: keys that produce identical low-16-bit hash values, meaning
     * after 4 rotations they've exhausted the distinguishing bits.
     *
     * Simpler: just insert many keys that all have the same low 4 bits
     * (level-0 slot) AND the same bits 4-7 (level-1 slot).  Keys like
     * 0x000, 0x100, 0x200 share both nibbles 0 and 1, diverging at nibble 2.
     * This forces a 2-level chain for each.  Full hash-equality would require
     * a custom hash; we instead verify the rotation works over many levels.
     */
    BEGIN_GROUP("deeply nested chain: 16 keys sharing all but top nibble");
    {
        /*
         * Keys 0x0000_0000_0000_0X00 for X in 0..15 all have
         * low nibble 0 and nibble-1 = 0.  They diverge at nibble 2 (bits 8-11).
         * So all 16 share depths 0 and 1, but each lands at a unique slot at
         * depth 2.  This exercises a 2-level common prefix for 16 keys.
         */
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        for (int x = 0; x < 16; x++)
            U64IntTrie_add(&t, (uint64_t)(x << 8), x, &a);
        int all_found = 1;
        for (int x = 0; x < 16; x++) {
            int *v = U64IntTrie_find(&t, (uint64_t)(x << 8));
            if (!v || *v != x) {all_found = 0; break;}
        }
        ASSERT(all_found);
        /* Delete all and verify empty. */
        for (int x = 0; x < 16; x++)
            ASSERT(U64IntTrie_delete(&t, (uint64_t)(x << 8)) == 1);
        for (int x = 0; x < 16; x++)
            ASSERT(U64IntTrie_find(&t, (uint64_t)(x << 8)) == NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("delete all entries: trie becomes empty");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        for (int i = 0; i < 10; i++)
            U64IntTrie_add(&t, (uint64_t)i, i, &a);
        for (int i = 0; i < 10; i++)
            U64IntTrie_delete(&t, (uint64_t)i);
        for (int i = 0; i < 10; i++)
            ASSERT(U64IntTrie_find(&t, (uint64_t)i) == NULL);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pool_reserve: no-op when min_blocks == 0");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie_pool_reserve(&pool, 0, &a);
        ASSERT(pool.cap == 0);
        ASSERT(pool.num == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pool_reserve: pre-allocates backing array");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie_pool_reserve(&pool, 10, &a);
        ASSERT(pool.cap >= 160);   /* 10 blocks * 16 nodes */
        ASSERT(pool.num == 0);     /* no blocks allocated yet */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pool_reserve: no-op when already sufficient");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        U64IntTrie_pool_reserve(&pool, 10, &a);
        uint32_t cap_after_first = pool.cap;
        U64IntTrie_pool_reserve(&pool, 5, &a);    /* smaller request */
        ASSERT(pool.cap == cap_after_first);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("pool_reserve: add() does not realloc within reserved capacity");
    {
        rc_arena a = rc_arena_make_default();
        U64IntTrie_pool pool = {0};
        /* Reserve enough for 20 blocks (320 nodes); create() uses 1 block. */
        U64IntTrie_pool_reserve(&pool, 20, &a);
        uint32_t cap_before = pool.cap;
        U64IntTrie t = U64IntTrie_create(&pool, &a);
        /* Each add() may allocate at most one new block per level of descent.
         * 10 distinct keys with no hash collisions consume at most 11 blocks. */
        for (int i = 0; i < 10; i++)
            U64IntTrie_add(&t, (uint64_t)i, i, &a);
        ASSERT(pool.cap == cap_before);   /* backing array was not reallocated */
        for (int i = 0; i < 10; i++) {
            int *v = U64IntTrie_find(&t, (uint64_t)i);
            ASSERT(v != NULL && *v == i);
        }
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

static void test_bitset(void);
static void test_str(void);
static void test_mstr(void);
static void test_math(void);
static void test_bigint(void);
static void test_file(void);

/* ---- main ---- */

int main(void)
{
    test_lower_bound();
    putchar('\n');
    test_upper_bound();
    putchar('\n');
    test_min_max_element();
    putchar('\n');
    test_sort_int();
    putchar('\n');
    test_record();
    putchar('\n');
    test_access();
    putchar('\n');
    test_platform();
    putchar('\n');
    test_arena();
    putchar('\n');
    test_array();
    putchar('\n');
    test_ctx();
    putchar('\n');
    test_transform();
    putchar('\n');
    test_find();
    putchar('\n');
    test_remove();
    putchar('\n');
    test_rotate();
    putchar('\n');
    test_all_any_none_of();
    putchar('\n');
    test_mismatch();
    putchar('\n');
    test_accumulate();
    putchar('\n');
    test_hash();
    putchar('\n');
    test_multimap();
    putchar('\n');
    test_hash_map();
    putchar('\n');
    test_hash_set();
    putchar('\n');
    test_hash_trie();
    putchar('\n');
    test_bitset();
    putchar('\n');
    test_str();
    putchar('\n');
    test_mstr();
    putchar('\n');
    test_math();
    putchar('\n');
    test_bigint();
    putchar('\n');
    test_file();

    putchar('\n');
    if (g_failures == 0)
        printf("All %d assertions passed.\n", g_total);
    else
        printf("%d / %d assertions FAILED.\n", g_failures, g_total);

    return g_failures > 0 ? 1 : 0;
}

/* ---- bitset ---- */

static void test_bitset(void)
{
    printf("bitset\n");

    /* ---- reserve ---- */

    BEGIN_GROUP("reserve: no-op when cap already sufficient");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_reserve(&bs, 32, &a);
        ASSERT(bs.cap >= 32);
        uint32_t cap_before = bs.cap;
        rc_bitset_reserve(&bs, 16, &a);
        ASSERT(bs.cap == cap_before);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: grows by doubling from 32");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_reserve(&bs, 1, &a);
        ASSERT(bs.cap == 32);
        rc_bitset_reserve(&bs, 33, &a);
        ASSERT(bs.cap == 64);
        rc_bitset_reserve(&bs, 128, &a);
        ASSERT(bs.cap == 128);
        rc_bitset_reserve(&bs, 129, &a);
        ASSERT(bs.cap == 256);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: new words zeroed");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_reserve(&bs, 64, &a);
        ASSERT(bs.data[0] == 0 && bs.data[1] == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* ---- resize ---- */

    BEGIN_GROUP("resize: grow sets num, new bits are zero");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 50, &a);
        ASSERT(bs.num == 50 && bs.cap >= 50);
        for (uint32_t i = 0; i < 50; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("resize: shrink clears vacated bits (invariant)");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        /* Set bits across both words. */
        for (uint32_t i = 0; i < 64; i++) rc_bitset_set(&bs, i);
        /* Shrink to 10: bits 10..63 must be cleared. */
        rc_bitset_resize(&bs, 10, &a);
        ASSERT(bs.num == 10);
        /* Grow back without setting anything: old bits beyond 10 must be 0. */
        rc_bitset_resize(&bs, 64, &a);
        for (uint32_t i = 0; i < 10; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 1);
        for (uint32_t i = 10; i < 64; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("resize: shrink to 0 clears all bits");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 32, &a);
        for (uint32_t i = 0; i < 32; i++) rc_bitset_set(&bs, i);
        rc_bitset_resize(&bs, 0, &a);
        ASSERT(bs.num == 0);
        rc_bitset_resize(&bs, 32, &a);
        for (uint32_t i = 0; i < 32; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("resize: shrink at exact word boundary");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        for (uint32_t i = 0; i < 64; i++) rc_bitset_set(&bs, i);
        rc_bitset_resize(&bs, 32, &a);
        ASSERT(bs.num == 32);
        rc_bitset_resize(&bs, 64, &a);
        for (uint32_t i = 0; i < 32; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 1);
        for (uint32_t i = 32; i < 64; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* ---- reset ---- */

    BEGIN_GROUP("reset: clears all bits, num and cap unchanged");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 96, &a);
        for (uint32_t i = 0; i < 96; i++) rc_bitset_set(&bs, i);
        rc_bitset_reset(&bs);
        ASSERT(bs.num == 96);
        for (uint32_t i = 0; i < 96; i++)
            ASSERT(rc_bitset_is_set(&bs, i) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reset: empty bitset (num=0) is a no-op");
    {
        rc_bitset bs = {0};
        rc_bitset_reset(&bs);   /* must not crash */
        ASSERT(bs.num == 0);
    }
    END_GROUP();

    /* ---- set / clear / is_set ---- */

    BEGIN_GROUP("set/is_set: individual bits in a multi-word bitset");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 128, &a);
        /* Boundary bits and interior bits. */
        uint32_t positions[] = {0, 1, 31, 32, 33, 63, 64, 65, 127};
        for (uint32_t k = 0; k < 9; k++)
            rc_bitset_set(&bs, positions[k]);
        for (uint32_t k = 0; k < 9; k++)
            ASSERT(rc_bitset_is_set(&bs, positions[k]) == 1);
        /* All other bits must remain 0. */
        for (uint32_t i = 0; i < 128; i++) {
            int expected = 0;
            for (uint32_t k = 0; k < 9; k++)
                if (positions[k] == i) { expected = 1; break; }
            ASSERT(rc_bitset_is_set(&bs, i) == expected);
        }
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("clear: clears only the target bit");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        for (uint32_t i = 0; i < 64; i++) rc_bitset_set(&bs, i);
        rc_bitset_clear(&bs, 31);
        rc_bitset_clear(&bs, 32);
        for (uint32_t i = 0; i < 64; i++) {
            int expected = (i == 31 || i == 32) ? 0 : 1;
            ASSERT(rc_bitset_is_set(&bs, i) == expected);
        }
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("set is idempotent; clear on already-clear is safe");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 32, &a);
        rc_bitset_set(&bs, 5);
        rc_bitset_set(&bs, 5);
        ASSERT(rc_bitset_is_set(&bs, 5) == 1);
        rc_bitset_clear(&bs, 10);   /* was already 0 */
        ASSERT(rc_bitset_is_set(&bs, 10) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* ---- get_first_set / get_next_set ---- */

    BEGIN_GROUP("get_first_set: empty bitset returns RC_INDEX_NONE");
    {
        rc_bitset bs = {0};
        ASSERT(rc_bitset_get_first_set(&bs) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("get_first_set: no bits set returns RC_INDEX_NONE");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        ASSERT(rc_bitset_get_first_set(&bs) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_first_set: finds bit 0");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 32, &a);
        rc_bitset_set(&bs, 0);
        ASSERT(rc_bitset_get_first_set(&bs) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_first_set: finds first set bit in second word");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        rc_bitset_set(&bs, 33);
        ASSERT(rc_bitset_get_first_set(&bs) == 33);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_first_set: last bit only");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        rc_bitset_set(&bs, 63);
        ASSERT(rc_bitset_get_first_set(&bs) == 63);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_next_set: pos beyond num returns RC_INDEX_NONE");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 32, &a);
        for (uint32_t i = 0; i < 32; i++) rc_bitset_set(&bs, i);
        ASSERT(rc_bitset_get_next_set(&bs, 32) == RC_INDEX_NONE);
        ASSERT(rc_bitset_get_next_set(&bs, UINT32_MAX) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_next_set: advances past cleared bit");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 32, &a);
        rc_bitset_set(&bs, 4);
        rc_bitset_set(&bs, 8);
        rc_bitset_set(&bs, 31);
        ASSERT(rc_bitset_get_next_set(&bs, 5)  == 8);
        ASSERT(rc_bitset_get_next_set(&bs, 9)  == 31);
        ASSERT(rc_bitset_get_next_set(&bs, 31) == 31);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_next_set: iteration visits every set bit exactly once");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 96, &a);
        /* Set every third bit. */
        for (uint32_t i = 0; i < 96; i += 3) rc_bitset_set(&bs, i);

        uint32_t count = 0;
        uint32_t prev  = RC_INDEX_NONE;
        for (uint32_t i = rc_bitset_get_first_set(&bs);
             i != RC_INDEX_NONE;
             i = rc_bitset_get_next_set(&bs, i + 1)) {
            ASSERT(i % 3 == 0);
            ASSERT(prev == RC_INDEX_NONE || i > prev);
            prev = i;
            count++;
        }
        ASSERT(count == 32);   /* 0, 3, 6, ..., 93 */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_next_set: iteration across word boundaries");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 128, &a);
        rc_bitset_set(&bs, 31);
        rc_bitset_set(&bs, 32);
        rc_bitset_set(&bs, 63);
        rc_bitset_set(&bs, 64);
        rc_bitset_set(&bs, 127);
        uint32_t expected[] = {31, 32, 63, 64, 127};
        uint32_t idx = 0;
        for (uint32_t i = rc_bitset_get_first_set(&bs);
             i != RC_INDEX_NONE;
             i = rc_bitset_get_next_set(&bs, i + 1)) {
            ASSERT(idx < 5 && i == expected[idx++]);
        }
        ASSERT(idx == 5);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("get_next_set: num not a multiple of 32 (no out-of-bounds)");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 37, &a);
        rc_bitset_set(&bs, 36);
        ASSERT(rc_bitset_get_first_set(&bs) == 36);
        ASSERT(rc_bitset_get_next_set(&bs, 37) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("iteration: after reset, no bits found");
    {
        rc_arena a = rc_arena_make_default();
        rc_bitset bs = {0};
        rc_bitset_resize(&bs, 64, &a);
        for (uint32_t i = 0; i < 64; i++) rc_bitset_set(&bs, i);
        rc_bitset_reset(&bs);
        ASSERT(rc_bitset_get_first_set(&bs) == RC_INDEX_NONE);
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- str ---- */

static void test_str(void)
{
    printf("str\n");

    /* rc_str_make */

    BEGIN_GROUP("make: NULL -> invalid");
    {
        rc_str s = rc_str_make(NULL);
        ASSERT(!rc_str_is_valid(s));
        ASSERT(s.len == 0);
    }
    END_GROUP();

    BEGIN_GROUP("make: empty string");
    {
        rc_str s = rc_str_make("");
        ASSERT(rc_str_is_valid(s));
        ASSERT(s.len == 0);
    }
    END_GROUP();

    BEGIN_GROUP("make: non-empty string");
    {
        rc_str s = rc_str_make("hello");
        ASSERT(rc_str_is_valid(s));
        ASSERT(s.len == 5);
        ASSERT(memcmp(s.data, "hello", 5) == 0);
    }
    END_GROUP();

    /* RC_STR macro */

    BEGIN_GROUP("RC_STR: compile-time length");
    {
        rc_str s = RC_STR("hello");
        ASSERT(s.len == 5);
        ASSERT(rc_str_is_valid(s));
        ASSERT(rc_str_is_equal(s, rc_str_make("hello")));
    }
    END_GROUP();

    BEGIN_GROUP("RC_STR: empty literal");
    {
        rc_str s = RC_STR("");
        ASSERT(rc_str_is_valid(s));
        ASSERT(s.len == 0);
    }
    END_GROUP();

    /* rc_str_is_valid */

    BEGIN_GROUP("is_valid");
    {
        ASSERT(!rc_str_is_valid((rc_str) {NULL, 0}));
        ASSERT( rc_str_is_valid(rc_str_make("")));
        ASSERT( rc_str_is_valid(RC_STR("hi")));
    }
    END_GROUP();

    /* rc_str_is_empty */

    BEGIN_GROUP("is_empty");
    {
        ASSERT( rc_str_is_empty((rc_str) {NULL, 0}));   /* invalid */
        ASSERT( rc_str_is_empty(rc_str_make("")));        /* valid empty */
        ASSERT(!rc_str_is_empty(RC_STR("x")));
    }
    END_GROUP();

    /* rc_str_is_equal */

    BEGIN_GROUP("is_equal: equal content");
    {
        ASSERT(rc_str_is_equal(RC_STR("abc"), rc_str_make("abc")));
    }
    END_GROUP();

    BEGIN_GROUP("is_equal: different lengths");
    {
        ASSERT(!rc_str_is_equal(RC_STR("ab"), RC_STR("abc")));
        ASSERT(!rc_str_is_equal(RC_STR("abc"), RC_STR("ab")));
    }
    END_GROUP();

    BEGIN_GROUP("is_equal: same length, different content");
    {
        ASSERT(!rc_str_is_equal(RC_STR("abc"), RC_STR("abd")));
    }
    END_GROUP();

    BEGIN_GROUP("is_equal: both zero-length");
    {
        ASSERT(rc_str_is_equal(rc_str_make(""), RC_STR("")));
        ASSERT(rc_str_is_equal((rc_str) {NULL, 0}, (rc_str) {NULL, 0}));
    }
    END_GROUP();

    /* rc_str_is_equal_insensitive / rc_str_compare_insensitive */

    BEGIN_GROUP("is_equal_insensitive: mixed case");
    {
        ASSERT( rc_str_is_equal_insensitive(RC_STR("Hello"), RC_STR("hello")));
        ASSERT( rc_str_is_equal_insensitive(RC_STR("HELLO"), RC_STR("hello")));
        ASSERT( rc_str_is_equal_insensitive(RC_STR("HeLLo"), RC_STR("hElLO")));
    }
    END_GROUP();

    BEGIN_GROUP("is_equal_insensitive: different strings");
    {
        ASSERT(!rc_str_is_equal_insensitive(RC_STR("Hello"), RC_STR("World")));
        ASSERT(!rc_str_is_equal_insensitive(RC_STR("abc"), RC_STR("abd")));
    }
    END_GROUP();

    /* rc_str_compare */

    BEGIN_GROUP("compare: equal, less, greater");
    {
        ASSERT(rc_str_compare(RC_STR("abc"), RC_STR("abc")) == 0);
        ASSERT(rc_str_compare(RC_STR("abc"), RC_STR("abd")) < 0);
        ASSERT(rc_str_compare(RC_STR("abd"), RC_STR("abc")) > 0);
    }
    END_GROUP();

    BEGIN_GROUP("compare: different lengths");
    {
        ASSERT(rc_str_compare(RC_STR("ab"),  RC_STR("abc")) < 0);
        ASSERT(rc_str_compare(RC_STR("abc"), RC_STR("ab"))  > 0);
        ASSERT(rc_str_compare(RC_STR(""),    RC_STR("a"))   < 0);
        ASSERT(rc_str_compare(RC_STR("a"),   RC_STR(""))    > 0);
        ASSERT(rc_str_compare(RC_STR(""),    RC_STR(""))   == 0);
    }
    END_GROUP();

    BEGIN_GROUP("compare_insensitive: case-folded ordering");
    {
        ASSERT(rc_str_compare_insensitive(RC_STR("ABC"), RC_STR("abc")) == 0);
        ASSERT(rc_str_compare_insensitive(RC_STR("abc"), RC_STR("ABD")) < 0);
        ASSERT(rc_str_compare_insensitive(RC_STR("ABD"), RC_STR("abc")) > 0);
        ASSERT(rc_str_compare_insensitive(RC_STR("AB"),  RC_STR("ABC")) < 0);
    }
    END_GROUP();

    /* rc_str_left */

    BEGIN_GROUP("left: count variants");
    {
        rc_str s = RC_STR("hello");
        ASSERT(rc_str_is_equal(rc_str_left(s, 0), RC_STR("")));
        ASSERT(rc_str_is_equal(rc_str_left(s, 3), RC_STR("hel")));
        ASSERT(rc_str_is_equal(rc_str_left(s, 5), RC_STR("hello")));
        ASSERT(rc_str_is_equal(rc_str_left(s, 9), RC_STR("hello"))); /* clamped */
    }
    END_GROUP();

    /* rc_str_right */

    BEGIN_GROUP("right: count variants");
    {
        rc_str s = RC_STR("hello");
        ASSERT(rc_str_is_equal(rc_str_right(s, 0), RC_STR("")));
        ASSERT(rc_str_is_equal(rc_str_right(s, 3), RC_STR("llo")));
        ASSERT(rc_str_is_equal(rc_str_right(s, 5), RC_STR("hello")));
        ASSERT(rc_str_is_equal(rc_str_right(s, 9), RC_STR("hello"))); /* clamped */
    }
    END_GROUP();

    /* rc_str_substr */

    BEGIN_GROUP("substr: start and count variants");
    {
        rc_str s = RC_STR("hello world");
        ASSERT(rc_str_is_equal(rc_str_substr(s, 0,  5),  RC_STR("hello")));
        ASSERT(rc_str_is_equal(rc_str_substr(s, 6,  5),  RC_STR("world")));
        ASSERT(rc_str_is_equal(rc_str_substr(s, 2,  3),  RC_STR("llo")));
        ASSERT(rc_str_is_equal(rc_str_substr(s, 0,  0),  RC_STR("")));
        ASSERT(rc_str_is_equal(rc_str_substr(s, 6,  99), RC_STR("world"))); /* clamped */
        ASSERT(rc_str_is_equal(rc_str_substr(s, 99, 3),  RC_STR("")));      /* start clamped */
    }
    END_GROUP();

    /* rc_str_skip */

    BEGIN_GROUP("skip: start variants");
    {
        rc_str s = RC_STR("hello");
        ASSERT(rc_str_is_equal(rc_str_skip(s, 0),  RC_STR("hello")));
        ASSERT(rc_str_is_equal(rc_str_skip(s, 2),  RC_STR("llo")));
        ASSERT(rc_str_is_equal(rc_str_skip(s, 5),  RC_STR("")));
        ASSERT(rc_str_is_equal(rc_str_skip(s, 99), RC_STR(""))); /* clamped */
    }
    END_GROUP();

    /* rc_str_starts_with */

    BEGIN_GROUP("starts_with");
    {
        rc_str s = RC_STR("hello world");
        ASSERT( rc_str_starts_with(s, RC_STR("")));
        ASSERT( rc_str_starts_with(s, RC_STR("h")));
        ASSERT( rc_str_starts_with(s, RC_STR("hello")));
        ASSERT( rc_str_starts_with(s, s));
        ASSERT(!rc_str_starts_with(s, RC_STR("world")));
        ASSERT(!rc_str_starts_with(s, RC_STR("hello world!")));  /* longer */
        ASSERT(!rc_str_starts_with(RC_STR("hi"), RC_STR("hello")));
    }
    END_GROUP();

    /* rc_str_ends_with */

    BEGIN_GROUP("ends_with");
    {
        rc_str s = RC_STR("hello world");
        ASSERT( rc_str_ends_with(s, RC_STR("")));
        ASSERT( rc_str_ends_with(s, RC_STR("d")));
        ASSERT( rc_str_ends_with(s, RC_STR("world")));
        ASSERT( rc_str_ends_with(s, s));
        ASSERT(!rc_str_ends_with(s, RC_STR("hello")));
        ASSERT(!rc_str_ends_with(s, RC_STR("!hello world"))); /* longer */
        ASSERT(!rc_str_ends_with(RC_STR("hi"), RC_STR("world")));
    }
    END_GROUP();

    /* rc_str_find_first */

    BEGIN_GROUP("find_first: not found");
    {
        ASSERT(rc_str_find_first(RC_STR("hello"), RC_STR("xyz")) == RC_INDEX_NONE);
        ASSERT(rc_str_find_first(RC_STR(""),      RC_STR("a"))   == RC_INDEX_NONE);
        ASSERT(rc_str_find_first(RC_STR("hi"),    RC_STR("hello")) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("find_first: found at various positions");
    {
        ASSERT(rc_str_find_first(RC_STR("hello"), RC_STR("h"))   == 0);
        ASSERT(rc_str_find_first(RC_STR("hello"), RC_STR("ell")) == 1);
        ASSERT(rc_str_find_first(RC_STR("hello"), RC_STR("o"))   == 4);
        ASSERT(rc_str_find_first(RC_STR("hello"), RC_STR("hello")) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("find_first: empty needle -> 0");
    {
        ASSERT(rc_str_find_first(RC_STR("hello"), RC_STR("")) == 0);
        ASSERT(rc_str_find_first(RC_STR(""),      RC_STR("")) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("find_first: multiple occurrences -> first");
    {
        ASSERT(rc_str_find_first(RC_STR("abcabc"), RC_STR("bc")) == 1);
        ASSERT(rc_str_find_first(RC_STR("aaaa"),   RC_STR("aa")) == 0);
    }
    END_GROUP();

    /* rc_str_find_last */

    BEGIN_GROUP("find_last: not found");
    {
        ASSERT(rc_str_find_last(RC_STR("hello"), RC_STR("xyz"))   == RC_INDEX_NONE);
        ASSERT(rc_str_find_last(RC_STR(""),      RC_STR("a"))     == RC_INDEX_NONE);
        ASSERT(rc_str_find_last(RC_STR("hi"),    RC_STR("hello")) == RC_INDEX_NONE);
    }
    END_GROUP();

    BEGIN_GROUP("find_last: multiple occurrences -> last");
    {
        ASSERT(rc_str_find_last(RC_STR("abcabc"), RC_STR("bc")) == 4);
        ASSERT(rc_str_find_last(RC_STR("abcabc"), RC_STR("a"))  == 3);
        ASSERT(rc_str_find_last(RC_STR("aaaa"),   RC_STR("aa")) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("find_last: single occurrence");
    {
        ASSERT(rc_str_find_last(RC_STR("hello"), RC_STR("h"))   == 0);
        ASSERT(rc_str_find_last(RC_STR("hello"), RC_STR("llo")) == 2);
        ASSERT(rc_str_find_last(RC_STR("hello"), RC_STR("o"))   == 4);
    }
    END_GROUP();

    BEGIN_GROUP("find_last: empty needle -> haystack.len");
    {
        ASSERT(rc_str_find_last(RC_STR("hello"), RC_STR("")) == 5);
        ASSERT(rc_str_find_last(RC_STR(""),      RC_STR("")) == 0);
    }
    END_GROUP();

    /* rc_str_contains */

    BEGIN_GROUP("contains");
    {
        ASSERT( rc_str_contains(RC_STR("hello world"), RC_STR("world")));
        ASSERT( rc_str_contains(RC_STR("hello world"), RC_STR("hello")));
        ASSERT( rc_str_contains(RC_STR("hello world"), RC_STR(" ")));
        ASSERT(!rc_str_contains(RC_STR("hello world"), RC_STR("xyz")));
        ASSERT( rc_str_contains(RC_STR("hello"),       RC_STR("")));   /* empty always found */
        ASSERT(!rc_str_contains(RC_STR("hi"),          RC_STR("hello")));
    }
    END_GROUP();

    /* rc_str_remove_prefix */

    BEGIN_GROUP("remove_prefix");
    {
        rc_str s = RC_STR("hello world");
        ASSERT(rc_str_is_equal(rc_str_remove_prefix(s, RC_STR("hello ")), RC_STR("world")));
        ASSERT(rc_str_is_equal(rc_str_remove_prefix(s, RC_STR("")),       s));  /* empty prefix: unchanged */
        ASSERT(rc_str_is_equal(rc_str_remove_prefix(s, RC_STR("world")),  s));  /* absent: unchanged */
        ASSERT(rc_str_is_equal(rc_str_remove_prefix(s, s),                RC_STR("")));  /* whole string */
    }
    END_GROUP();

    /* rc_str_remove_suffix */

    BEGIN_GROUP("remove_suffix");
    {
        rc_str s = RC_STR("hello world");
        ASSERT(rc_str_is_equal(rc_str_remove_suffix(s, RC_STR(" world")), RC_STR("hello")));
        ASSERT(rc_str_is_equal(rc_str_remove_suffix(s, RC_STR("")),       s));  /* empty suffix: unchanged */
        ASSERT(rc_str_is_equal(rc_str_remove_suffix(s, RC_STR("hello")),  s));  /* absent: unchanged */
        ASSERT(rc_str_is_equal(rc_str_remove_suffix(s, s),                RC_STR("")));  /* whole string */
    }
    END_GROUP();

    /* rc_str_first_split */

    BEGIN_GROUP("first_split: delimiter found");
    {
        rc_str_pair p = rc_str_first_split(RC_STR("a:b:c"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("a")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("b:c")));
    }
    END_GROUP();

    BEGIN_GROUP("first_split: delimiter absent");
    {
        rc_str_pair p = rc_str_first_split(RC_STR("hello"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first, RC_STR("hello")));
        ASSERT(!rc_str_is_valid(p.second));
    }
    END_GROUP();

    BEGIN_GROUP("first_split: delimiter at start");
    {
        rc_str_pair p = rc_str_first_split(RC_STR(":rest"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("rest")));
    }
    END_GROUP();

    BEGIN_GROUP("first_split: delimiter at end");
    {
        rc_str_pair p = rc_str_first_split(RC_STR("hello:"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("hello")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("")));
        ASSERT(rc_str_is_valid(p.second));
    }
    END_GROUP();

    BEGIN_GROUP("first_split: multi-char delimiter");
    {
        rc_str_pair p = rc_str_first_split(RC_STR("one::two::three"), RC_STR("::"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("one")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("two::three")));
    }
    END_GROUP();

    /* rc_str_last_split */

    BEGIN_GROUP("last_split: delimiter found once");
    {
        rc_str_pair p = rc_str_last_split(RC_STR("a:b"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("a")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("b")));
    }
    END_GROUP();

    BEGIN_GROUP("last_split: multiple delimiters -> split at last");
    {
        rc_str_pair p = rc_str_last_split(RC_STR("a:b:c"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("a:b")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("c")));
    }
    END_GROUP();

    BEGIN_GROUP("last_split: delimiter absent");
    {
        rc_str_pair p = rc_str_last_split(RC_STR("hello"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first, RC_STR("hello")));
        ASSERT(!rc_str_is_valid(p.second));
    }
    END_GROUP();

    BEGIN_GROUP("last_split: delimiter at start");
    {
        rc_str_pair p = rc_str_last_split(RC_STR(":rest"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("rest")));
    }
    END_GROUP();

    BEGIN_GROUP("last_split: delimiter at end");
    {
        rc_str_pair p = rc_str_last_split(RC_STR("hello:"), RC_STR(":"));
        ASSERT(rc_str_is_equal(p.first,  RC_STR("hello")));
        ASSERT(rc_str_is_equal(p.second, RC_STR("")));
        ASSERT(rc_str_is_valid(p.second));
    }
    END_GROUP();

    /* rc_str_as_cstr */

    /* Helper: rc_str_left creates a view where data[len] != '\0' (the char
     * after the view is ' ', not '\0'), exercising the copy path. */

    BEGIN_GROUP("as_cstr: null-terminated -> returns data directly");
    {
        rc_str s = RC_STR("hello");
        char buf[16];
        const char *r = rc_str_as_cstr(s, buf, sizeof(buf));
        ASSERT(r == s.data);          /* no copy: same pointer */
        ASSERT(strcmp(r, "hello") == 0);
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: null-terminated ignores buf and buf_size");
    {
        /* buf is NULL and buf_size is 0 — must not matter for null-terminated input */
        rc_str s = RC_STR("hello");
        ASSERT(rc_str_as_cstr(s, NULL, 0) == s.data);
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: empty null-terminated -> returns data directly");
    {
        rc_str s = RC_STR("");
        char buf[4] = {'x', 'x', 'x', 'x'};
        const char *r = rc_str_as_cstr(s, buf, sizeof(buf));
        ASSERT(r == s.data);
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: non-null-terminated copies to buffer");
    {
        /* rc_str_left("hello world", 5) -> data[5] == ' ', not '\0' */
        rc_str s = rc_str_left(rc_str_make("hello world"), 5);
        char buf[16];
        const char *r = rc_str_as_cstr(s, buf, sizeof(buf));
        ASSERT(r == buf);
        ASSERT(strcmp(buf, "hello") == 0);
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: non-null-terminated exact-fit buffer");
    {
        rc_str s = rc_str_left(rc_str_make("hello world"), 5);
        char buf[6]; /* 5 chars + '\0' */
        const char *r = rc_str_as_cstr(s, buf, 6);
        ASSERT(r == buf);
        ASSERT(strcmp(buf, "hello") == 0);
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: non-null-terminated truncation");
    {
        rc_str s = rc_str_left(rc_str_make("hello world"), 5);
        char buf[4];
        const char *r = rc_str_as_cstr(s, buf, 4);
        ASSERT(r == buf);
        ASSERT(strcmp(buf, "hel") == 0); /* truncated, still null-terminated */
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: non-null-terminated, buf_size 0 -> NULL");
    {
        rc_str s = rc_str_left(rc_str_make("hello world"), 5);
        char buf[8];
        ASSERT(rc_str_as_cstr(s, buf, 0) == NULL);
    }
    END_GROUP();

    BEGIN_GROUP("as_cstr: non-null-terminated, NULL buf -> NULL");
    {
        rc_str s = rc_str_left(rc_str_make("hello world"), 5);
        ASSERT(rc_str_as_cstr(s, NULL, 16) == NULL);
    }
    END_GROUP();
}

/* ---- mstr ---- */

static void test_mstr(void)
{
    printf("mstr\n");

    /* rc_mstr_make */

    BEGIN_GROUP("make: valid empty string with given capacity");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(16, &a);
        ASSERT(rc_mstr_is_valid(&s));
        ASSERT(rc_mstr_is_empty(&s));
        ASSERT(s.len == 0);
        ASSERT(s.cap == 16);
        ASSERT(s.data[0] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("make: cap=0 yields valid empty string");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(0, &a);
        ASSERT(rc_mstr_is_valid(&s));
        ASSERT(rc_mstr_is_empty(&s));
        ASSERT(s.cap == 0);
        ASSERT(s.data[0] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("make: view is valid empty rc_str");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(8, &a);
        ASSERT(rc_str_is_valid(s.view));
        ASSERT(rc_str_is_empty(s.view));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_from_cstr */

    BEGIN_GROUP("from_cstr: NULL -> invalid");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr(NULL, 0, &a);
        ASSERT(!rc_mstr_is_valid(&s));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_cstr: empty string");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("", 0, &a);
        ASSERT(rc_mstr_is_valid(&s));
        ASSERT(rc_mstr_is_empty(&s));
        ASSERT(s.len == 0);
        ASSERT(s.data[0] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_cstr: copies content, null-terminated");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        ASSERT(rc_mstr_is_valid(&s));
        ASSERT(s.len == 5);
        ASSERT(s.cap == 5);
        ASSERT(memcmp(s.data, "hello", 5) == 0);
        ASSERT(s.data[5] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_cstr: max_cap larger than len");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hi", 20, &a);
        ASSERT(s.len == 2);
        ASSERT(s.cap == 20);
        ASSERT(memcmp(s.data, "hi", 2) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_cstr: max_cap smaller than len is ignored");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 2, &a);
        ASSERT(s.len == 5);
        ASSERT(s.cap == 5);   /* len wins */
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_from_str */

    BEGIN_GROUP("from_str: invalid rc_str -> invalid rc_mstr");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_str((rc_str) {NULL, 0}, 0, &a);
        ASSERT(!rc_mstr_is_valid(&s));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_str: empty rc_str");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_str(RC_STR(""), 0, &a);
        ASSERT(rc_mstr_is_valid(&s));
        ASSERT(s.len == 0);
        ASSERT(s.data[0] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_str: copies content");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_str(RC_STR("world"), 0, &a);
        ASSERT(s.len == 5);
        ASSERT(s.cap == 5);
        ASSERT(rc_str_is_equal(s.view, RC_STR("world")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_str: max_cap larger than source len");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_str(RC_STR("abc"), 16, &a);
        ASSERT(s.len == 3);
        ASSERT(s.cap == 16);
        ASSERT(rc_str_is_equal(s.view, RC_STR("abc")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("from_str: non-null-terminated rc_str view");
    {
        /* rc_str_left produces a view whose data[len] is not '\0'. */
        rc_arena a = rc_arena_make_default();
        rc_str src = rc_str_left(rc_str_make("hello world"), 5);
        rc_mstr s = rc_mstr_from_str(src, 0, &a);
        ASSERT(s.len == 5);
        ASSERT(s.data[5] == '\0');    /* copy appends null terminator */
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_is_valid / rc_mstr_is_empty */

    BEGIN_GROUP("is_valid / is_empty");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr inv = {.data = NULL, .len = 0, .cap = 0};
        rc_mstr empty = rc_mstr_make(4, &a);
        rc_mstr nonempty = rc_mstr_from_cstr("x", 0, &a);
        ASSERT(!rc_mstr_is_valid(&inv));
        ASSERT( rc_mstr_is_empty(&inv));
        ASSERT( rc_mstr_is_valid(&empty));
        ASSERT( rc_mstr_is_empty(&empty));
        ASSERT( rc_mstr_is_valid(&nonempty));
        ASSERT(!rc_mstr_is_empty(&nonempty));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_reset */

    BEGIN_GROUP("reset: clears len, retains cap and buffer");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        uint32_t old_cap = s.cap;
        const char *old_ptr = s.data;
        rc_mstr_reset(&s);
        ASSERT(s.len == 0);
        ASSERT(s.cap == old_cap);
        ASSERT(s.data == old_ptr);    /* same buffer */
        ASSERT(s.data[0] == '\0');
        ASSERT(rc_mstr_is_empty(&s));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reset: can append again after reset");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        rc_mstr_reset(&s);
        rc_mstr_append(&s, RC_STR("bye"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("bye")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_reserve */

    BEGIN_GROUP("reserve: no-op when already sufficient");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(20, &a);
        const char *ptr_before = s.data;
        rc_mstr_reserve(&s, 10, &a);
        ASSERT(s.cap == 20);
        ASSERT(s.data == ptr_before);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: grows capacity, preserves content");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        ASSERT(s.cap == 5);
        rc_mstr_reserve(&s, 32, &a);
        ASSERT(s.cap == 32);
        ASSERT(s.len == 5);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        ASSERT(s.data[5] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("reserve: exact new_cap == old_cap is no-op");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(8, &a);
        const char *ptr_before = s.data;
        rc_mstr_reserve(&s, 8, &a);
        ASSERT(s.data == ptr_before);
        ASSERT(s.cap == 8);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_append */

    BEGIN_GROUP("append: single append");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(16, &a);
        rc_mstr_append(&s, RC_STR("hello"), &a);
        ASSERT(s.len == 5);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        ASSERT(s.data[5] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append: multiple appends build string");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(32, &a);
        rc_mstr_append(&s, RC_STR("foo"), &a);
        rc_mstr_append(&s, RC_STR("bar"), &a);
        rc_mstr_append(&s, RC_STR("baz"), &a);
        ASSERT(s.len == 9);
        ASSERT(rc_str_is_equal(s.view, RC_STR("foobarbaz")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append: empty rc_str is no-op");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("abc", 0, &a);
        rc_mstr_append(&s, RC_STR(""), &a);
        ASSERT(s.len == 3);
        ASSERT(rc_str_is_equal(s.view, RC_STR("abc")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append: forces growth from zero capacity");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(0, &a);
        rc_mstr_append(&s, RC_STR("hello"), &a);
        ASSERT(s.len == 5);
        ASSERT(s.cap >= 5);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append: forces growth (doubling strategy)");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(4, &a);
        rc_mstr_append(&s, RC_STR("AAAA"), &a);   /* fills capacity */
        rc_mstr_append(&s, RC_STR("B"), &a);       /* triggers growth */
        ASSERT(s.len == 5);
        ASSERT(s.cap >= 8);   /* doubling: 4 -> 8 */
        ASSERT(rc_str_is_equal(s.view, RC_STR("AAAAB")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append: large append beyond double forces exact fit");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(4, &a);
        rc_mstr_append(&s, RC_STR("0123456789"), &a);   /* 10 > 4*2=8 */
        ASSERT(s.len == 10);
        ASSERT(s.cap >= 10);
        ASSERT(rc_str_is_equal(s.view, RC_STR("0123456789")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_append_char */

    BEGIN_GROUP("append_char: single character");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(4, &a);
        rc_mstr_append_char(&s, 'x', &a);
        ASSERT(s.len == 1);
        ASSERT(s.data[0] == 'x');
        ASSERT(s.data[1] == '\0');
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append_char: multiple characters build string");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(8, &a);
        rc_mstr_append_char(&s, 'a', &a);
        rc_mstr_append_char(&s, 'b', &a);
        rc_mstr_append_char(&s, 'c', &a);
        ASSERT(s.len == 3);
        ASSERT(rc_str_is_equal(s.view, RC_STR("abc")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append_char: forces growth");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(2, &a);
        rc_mstr_append_char(&s, 'x', &a);
        rc_mstr_append_char(&s, 'y', &a);
        rc_mstr_append_char(&s, 'z', &a);   /* triggers growth */
        ASSERT(s.len == 3);
        ASSERT(s.cap >= 3);
        ASSERT(rc_str_is_equal(s.view, RC_STR("xyz")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("append_char: null terminator always correct");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(0, &a);
        for (int i = 0; i < 10; i++)
            rc_mstr_append_char(&s, (char)('a' + i), &a);
        ASSERT(s.len == 10);
        ASSERT(s.data[10] == '\0');
        ASSERT(rc_str_is_equal(s.view, RC_STR("abcdefghij")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* rc_mstr_replace */

    BEGIN_GROUP("replace: empty find is no-op");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        rc_mstr_replace(&s, RC_STR(""), RC_STR("X"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: empty string is no-op");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_make(8, &a);
        rc_mstr_replace(&s, RC_STR("x"), RC_STR("y"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: find not present is no-op");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        rc_mstr_replace(&s, RC_STR("xyz"), RC_STR("!"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: single occurrence, same-length replacement");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("held", 0, &a);   /* one 'l' */
        rc_mstr_replace(&s, RC_STR("l"), RC_STR("r"), &a);
        ASSERT(s.len == 4);
        ASSERT(rc_str_is_equal(s.view, RC_STR("herd")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: multiple occurrences, same-length replacement");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("aXbXcX", 0, &a);
        rc_mstr_replace(&s, RC_STR("X"), RC_STR("Y"), &a);
        ASSERT(s.len == 6);
        ASSERT(rc_str_is_equal(s.view, RC_STR("aYbYcY")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: shorter replacement (string shrinks, in-place)");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("aabbcc", 0, &a);
        rc_mstr_replace(&s, RC_STR("bb"), RC_STR("b"), &a);
        ASSERT(s.len == 5);
        ASSERT(rc_str_is_equal(s.view, RC_STR("aabcc")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: replace with empty (deletion, in-place)");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello world", 0, &a);
        rc_mstr_replace(&s, RC_STR(" "), RC_STR(""), &a);
        ASSERT(s.len == 10);
        ASSERT(rc_str_is_equal(s.view, RC_STR("helloworld")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: longer replacement (right-to-left in-place)");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("aXb", 0, &a);
        rc_mstr_replace(&s, RC_STR("X"), RC_STR("YZ"), &a);
        ASSERT(s.len == 4);
        ASSERT(rc_str_is_equal(s.view, RC_STR("aYZb")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: multiple occurrences, longer replacement");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("a-b-c", 0, &a);
        rc_mstr_replace(&s, RC_STR("-"), RC_STR("::"), &a);
        ASSERT(s.len == 7);
        ASSERT(rc_str_is_equal(s.view, RC_STR("a::b::c")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: replace whole string");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("foo", 0, &a);
        rc_mstr_replace(&s, RC_STR("foo"), RC_STR("bar"), &a);
        ASSERT(s.len == 3);
        ASSERT(rc_str_is_equal(s.view, RC_STR("bar")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: replace whole string with longer");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hi", 0, &a);
        rc_mstr_replace(&s, RC_STR("hi"), RC_STR("hello"), &a);
        ASSERT(s.len == 5);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: non-overlapping matches only (left to right)");
    {
        /* "aaaa" with find="aa": non-overlapping -> [aa][aa], 2 matches */
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("aaaa", 0, &a);
        rc_mstr_replace(&s, RC_STR("aa"), RC_STR("b"), &a);
        ASSERT(s.len == 2);
        ASSERT(rc_str_is_equal(s.view, RC_STR("bb")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: match at start");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("foobar", 0, &a);
        rc_mstr_replace(&s, RC_STR("foo"), RC_STR("baz"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("bazbar")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: match at end");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("foobar", 0, &a);
        rc_mstr_replace(&s, RC_STR("bar"), RC_STR("baz"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("foobaz")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: multi-char find");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("one::two::three", 0, &a);
        rc_mstr_replace(&s, RC_STR("::"), RC_STR("-"), &a);
        ASSERT(rc_str_is_equal(s.view, RC_STR("one-two-three")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("replace: null terminator valid after all replacements");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("xAxBxCx", 0, &a);
        rc_mstr_replace(&s, RC_STR("x"), RC_STR(""), &a);
        ASSERT(s.len == 3);
        ASSERT(rc_str_is_equal(s.view, RC_STR("ABC")));
        ASSERT(s.data[3] == '\0');
        /* rc_str_as_cstr fast path works: data[len] == '\0' */
        ASSERT(rc_str_as_cstr(s.view, NULL, 0) == s.data);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    /* view consistency */

    BEGIN_GROUP("view: s.view and s.data/s.len alias the same memory");
    {
        rc_arena a = rc_arena_make_default();
        rc_mstr s = rc_mstr_from_cstr("hello", 0, &a);
        ASSERT(s.view.data == s.data);
        ASSERT(s.view.len  == s.len);
        rc_mstr_append(&s, RC_STR(" world"), &a);
        ASSERT(s.view.data == s.data);
        ASSERT(s.view.len  == s.len);
        ASSERT(rc_str_is_equal(s.view, RC_STR("hello world")));
        rc_arena_destroy(&a);
    }
    END_GROUP();
}

/* ---- math ---- */

/* Tolerance for float comparisons. */
#define NEARLY(a, b)   (fabsf((a) - (b)) < 1e-5f)
#define VNEARLY2(a, b) (rc_vec2f_is_nearly_equal((a), (b), 1e-4f))
#define VNEARLY3(a, b) (rc_vec3f_is_nearly_equal((a), (b), 1e-4f))
#define VNEARLY4(a, b) (rc_vec4f_is_nearly_equal((a), (b), 1e-4f))

static void test_math(void)
{
    printf("math\n");

    /* ---- math.h ---- */

    BEGIN_GROUP("deg_to_rad: 0, 90, 180, 360");
    {
        ASSERT(NEARLY(rc_deg_to_rad(0.0f),   0.0f));
        ASSERT(NEARLY(rc_deg_to_rad(90.0f),  (float)(3.14159265358979 / 2.0)));
        ASSERT(NEARLY(rc_deg_to_rad(180.0f), (float)3.14159265358979));
        ASSERT(NEARLY(rc_deg_to_rad(360.0f), (float)(3.14159265358979 * 2.0)));
    }
    END_GROUP();

    BEGIN_GROUP("min/max/sgn i32");
    {
        ASSERT(rc_min_i32(3, 5)   == 3);
        ASSERT(rc_min_i32(-1, 0)  == -1);
        ASSERT(rc_max_i32(3, 5)   == 5);
        ASSERT(rc_max_i32(-1, 0)  == 0);
        ASSERT(rc_sgn_i32(-7)     == -1);
        ASSERT(rc_sgn_i32(0)      == 0);
        ASSERT(rc_sgn_i32(42)     == 1);
    }
    END_GROUP();

    BEGIN_GROUP("gcd i32: basic cases");
    {
        ASSERT(rc_gcd_i32(12, 8)  == 4);
        ASSERT(rc_gcd_i32(0, 5)   == 5);
        ASSERT(rc_gcd_i32(7, 0)   == 7);
        ASSERT(rc_gcd_i32(-6, 4)  == 2);  /* abs applied */
        ASSERT(rc_gcd_i32(1, 1)   == 1);
    }
    END_GROUP();

    BEGIN_GROUP("gcd i64: basic cases");
    {
        ASSERT(rc_gcd_i64(100, 75) == 25);
        ASSERT(rc_gcd_i64(0, 13)   == 13);
        ASSERT(rc_gcd_i64(-12, 8)  == 4);
    }
    END_GROUP();

    BEGIN_GROUP("clz u32: known bit patterns");
    {
        ASSERT(rc_clz_u32(0)          == 32);
        ASSERT(rc_clz_u32(1)          == 31);
        ASSERT(rc_clz_u32(0x80000000u) == 0);
        ASSERT(rc_clz_u32(0x00010000u) == 15);
    }
    END_GROUP();

    BEGIN_GROUP("clz u64: known bit patterns");
    {
        ASSERT(rc_clz_u64(0)                   == 64);
        ASSERT(rc_clz_u64(1)                   == 63);
        ASSERT(rc_clz_u64(0x8000000000000000ull) == 0);
    }
    END_GROUP();

    BEGIN_GROUP("overflow checks");
    {
        ASSERT( rc_mul_overflows_u64(UINT64_MAX, 2));
        ASSERT(!rc_mul_overflows_u64(0, UINT64_MAX));
        ASSERT(!rc_mul_overflows_u64(1, UINT64_MAX));
        ASSERT( rc_add_overflows_u64(UINT64_MAX, 1));
        ASSERT(!rc_add_overflows_u64(0, UINT64_MAX));
        ASSERT(!rc_add_overflows_u64(1, 1));
    }
    END_GROUP();

    /* ---- vec2i ---- */

    BEGIN_GROUP("vec2i: construction and equality");
    {
        rc_vec2i a = rc_vec2i_make(3, -4);
        ASSERT(a.x == 3 && a.y == -4);
        ASSERT(rc_vec2i_is_equal(a, rc_vec2i_make(3, -4)));
        ASSERT(!rc_vec2i_is_equal(a, rc_vec2i_make_zero()));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_make_unitx(), rc_vec2i_make(1, 0)));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_make_unity(), rc_vec2i_make(0, 1)));
    }
    END_GROUP();

    BEGIN_GROUP("vec2i: arithmetic");
    {
        rc_vec2i a = rc_vec2i_make(1, 2);
        rc_vec2i b = rc_vec2i_make(3, 4);
        ASSERT(rc_vec2i_is_equal(rc_vec2i_add(a, b), rc_vec2i_make(4, 6)));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_sub(a, b), rc_vec2i_make(-2, -2)));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_scalar_mul(a, 3), rc_vec2i_make(3, 6)));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_scalar_div(rc_vec2i_make(6, 9), 3), rc_vec2i_make(2, 3)));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_scalar_div(rc_vec2i_make(7, 9), 3), rc_vec2i_make(2, 3))); /* truncates */
        ASSERT(rc_vec2i_is_equal(rc_vec2i_negate(a), rc_vec2i_make(-1, -2)));
        ASSERT(rc_vec2i_dot(a, b) == 11);
        ASSERT(rc_vec2i_wedge(a, b) == -2);  /* 1*4 - 3*2 = -2 */
        ASSERT(rc_vec2i_is_equal(rc_vec2i_perp(a), rc_vec2i_make(-2, 1)));
        ASSERT(rc_vec2i_lengthsqr(a) == 5);
    }
    END_GROUP();

    BEGIN_GROUP("vec2i: component min/max");
    {
        rc_vec2i a = rc_vec2i_make(1, 5);
        rc_vec2i b = rc_vec2i_make(3, 2);
        ASSERT(rc_vec2i_is_equal(rc_vec2i_component_min(a, b), rc_vec2i_make(1, 2)));
        ASSERT(rc_vec2i_is_equal(rc_vec2i_component_max(a, b), rc_vec2i_make(3, 5)));
    }
    END_GROUP();

    BEGIN_GROUP("vec2i: from/as arrays");
    {
        int32_t arr[] = {7, -3};
        rc_vec2i v = rc_vec2i_from_i32s(arr);
        ASSERT(v.x == 7 && v.y == -3);
        ASSERT(rc_vec2i_as_i32s(&v) == &v.x);
    }
    END_GROUP();

    /* ---- vec3i ---- */

    BEGIN_GROUP("vec3i: construction and equality");
    {
        rc_vec3i a = rc_vec3i_make(1, -2, 3);
        ASSERT(rc_vec3i_is_equal(a, rc_vec3i_make(1, -2, 3)));
        ASSERT(!rc_vec3i_is_equal(a, rc_vec3i_make_zero()));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_make_unitx(), rc_vec3i_make(1, 0, 0)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_make_unity(), rc_vec3i_make(0, 1, 0)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_make_unitz(), rc_vec3i_make(0, 0, 1)));
    }
    END_GROUP();

    BEGIN_GROUP("vec3i: arithmetic");
    {
        rc_vec3i a = rc_vec3i_make(1, 2, 3);
        rc_vec3i b = rc_vec3i_make(4, 5, 6);
        ASSERT(rc_vec3i_is_equal(rc_vec3i_add(a, b), rc_vec3i_make(5, 7, 9)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_sub(a, b), rc_vec3i_make(-3, -3, -3)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_scalar_mul(a, 3), rc_vec3i_make(3, 6, 9)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_scalar_div(rc_vec3i_make(6, 9, 12), 3), rc_vec3i_make(2, 3, 4)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_scalar_div(rc_vec3i_make(7, 9, 11), 3), rc_vec3i_make(2, 3, 3))); /* truncates */
        ASSERT(rc_vec3i_is_equal(rc_vec3i_negate(a), rc_vec3i_make(-1, -2, -3)));
        ASSERT(rc_vec3i_dot(a, b) == 32);       /* 4+10+18 */
        ASSERT(rc_vec3i_lengthsqr(a) == 14);    /* 1+4+9 */
    }
    END_GROUP();

    BEGIN_GROUP("vec3i: cross product");
    {
        rc_vec3i x = rc_vec3i_make_unitx();
        rc_vec3i y = rc_vec3i_make_unity();
        rc_vec3i z = rc_vec3i_make_unitz();
        ASSERT(rc_vec3i_is_equal(rc_vec3i_cross(x, y), z));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_cross(y, z), x));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_cross(z, x), y));
        /* anti-commutativity: a × b == -(b × a) */
        rc_vec3i a = rc_vec3i_make(1, 2, 3);
        rc_vec3i b = rc_vec3i_make(4, 5, 6);
        ASSERT(rc_vec3i_is_equal(rc_vec3i_cross(a, b),
                                 rc_vec3i_negate(rc_vec3i_cross(b, a))));
        /* self-cross is zero */
        ASSERT(rc_vec3i_is_equal(rc_vec3i_cross(a, a), rc_vec3i_make_zero()));
    }
    END_GROUP();

    BEGIN_GROUP("vec3i: component min/max");
    {
        rc_vec3i a = rc_vec3i_make(1, 5, 3);
        rc_vec3i b = rc_vec3i_make(4, 2, 6);
        ASSERT(rc_vec3i_is_equal(rc_vec3i_component_min(a, b), rc_vec3i_make(1, 2, 3)));
        ASSERT(rc_vec3i_is_equal(rc_vec3i_component_max(a, b), rc_vec3i_make(4, 5, 6)));
    }
    END_GROUP();

    BEGIN_GROUP("vec3i: from/as arrays");
    {
        int32_t arr[] = {7, -3, 5};
        rc_vec3i v = rc_vec3i_from_i32s(arr);
        ASSERT(v.x == 7 && v.y == -3 && v.z == 5);
        ASSERT(rc_vec3i_as_i32s(&v) == &v.x);
        /* from_vec2i */
        rc_vec3i e = rc_vec3i_from_vec2i(rc_vec2i_make(1, 2), 3);
        ASSERT(rc_vec3i_is_equal(e, rc_vec3i_make(1, 2, 3)));
    }
    END_GROUP();

    /* ---- vec2f ---- */

    BEGIN_GROUP("vec2f: construction");
    {
        rc_vec2f a = rc_vec2f_make(1.0f, 2.0f);
        ASSERT(rc_vec2f_is_equal(a, (rc_vec2f) {1.0f, 2.0f}));
        ASSERT(rc_vec2f_is_equal(rc_vec2f_make_zero(),  rc_vec2f_make(0.0f, 0.0f)));
        ASSERT(rc_vec2f_is_equal(rc_vec2f_make_unitx(), rc_vec2f_make(1.0f, 0.0f)));
        ASSERT(rc_vec2f_is_equal(rc_vec2f_make_unity(), rc_vec2f_make(0.0f, 1.0f)));
        /* from_vec2i */
        rc_vec2f fv = rc_vec2f_from_vec2i(rc_vec2i_make(3, -4));
        ASSERT(NEARLY(fv.x, 3.0f) && NEARLY(fv.y, -4.0f));
    }
    END_GROUP();

    BEGIN_GROUP("vec2f: arithmetic");
    {
        rc_vec2f a = rc_vec2f_make(1.0f, 2.0f);
        rc_vec2f b = rc_vec2f_make(3.0f, 4.0f);
        ASSERT(VNEARLY2(rc_vec2f_add(a, b), rc_vec2f_make(4.0f, 6.0f)));
        ASSERT(VNEARLY2(rc_vec2f_sub(a, b), rc_vec2f_make(-2.0f, -2.0f)));
        ASSERT(VNEARLY2(rc_vec2f_scalar_mul(a, 3.0f), rc_vec2f_make(3.0f, 6.0f)));
        ASSERT(VNEARLY2(rc_vec2f_scalar_div(rc_vec2f_make(6.0f, 9.0f), 3.0f), rc_vec2f_make(2.0f, 3.0f)));
        ASSERT(VNEARLY2(rc_vec2f_negate(a), rc_vec2f_make(-1.0f, -2.0f)));
        ASSERT(NEARLY(rc_vec2f_dot(a, b), 11.0f));
        ASSERT(NEARLY(rc_vec2f_wedge(a, b), -2.0f));
        ASSERT(VNEARLY2(rc_vec2f_perp(a), rc_vec2f_make(-2.0f, 1.0f)));
        ASSERT(NEARLY(rc_vec2f_lengthsqr(a), 5.0f));
        ASSERT(NEARLY(rc_vec2f_length(a), sqrtf(5.0f)));
    }
    END_GROUP();

    BEGIN_GROUP("vec2f: normalize");
    {
        rc_vec2f a = rc_vec2f_make(3.0f, 4.0f);
        rc_vec2f n = rc_vec2f_normalize(a);
        ASSERT(NEARLY(rc_vec2f_length(n), 1.0f));
        ASSERT(NEARLY(n.x, 0.6f) && NEARLY(n.y, 0.8f));
        /* normalize_safe: zero vector returns zero */
        ASSERT(rc_vec2f_is_equal(
            rc_vec2f_normalize_safe(rc_vec2f_make_zero(), 1e-6f),
            rc_vec2f_make_zero()));
    }
    END_GROUP();

    BEGIN_GROUP("vec2f: lerp");
    {
        rc_vec2f a = rc_vec2f_make(0.0f, 0.0f);
        rc_vec2f b = rc_vec2f_make(2.0f, 4.0f);
        ASSERT(VNEARLY2(rc_vec2f_lerp(a, b, 0.0f), a));
        ASSERT(VNEARLY2(rc_vec2f_lerp(a, b, 1.0f), b));
        ASSERT(VNEARLY2(rc_vec2f_lerp(a, b, 0.5f), rc_vec2f_make(1.0f, 2.0f)));
    }
    END_GROUP();

    BEGIN_GROUP("vec2f: make_sincos / make_cossin");
    {
        float angle = rc_deg_to_rad(30.0f);
        rc_vec2f sc = rc_vec2f_make_sincos(angle);
        rc_vec2f cs = rc_vec2f_make_cossin(angle);
        ASSERT(NEARLY(sc.x, sinf(angle)) && NEARLY(sc.y, cosf(angle)));
        ASSERT(NEARLY(cs.x, cosf(angle)) && NEARLY(cs.y, sinf(angle)));
    }
    END_GROUP();

    /* ---- vec3f ---- */

    BEGIN_GROUP("vec3f: from_vec3i");
    {
        rc_vec3f v = rc_vec3f_from_vec3i(rc_vec3i_make(1, -2, 3));
        ASSERT(NEARLY(v.x, 1.0f) && NEARLY(v.y, -2.0f) && NEARLY(v.z, 3.0f));
    }
    END_GROUP();

    BEGIN_GROUP("vec3f: arithmetic");
    {
        rc_vec3f a = rc_vec3f_make(1.0f, 2.0f, 3.0f);
        rc_vec3f b = rc_vec3f_make(4.0f, 5.0f, 6.0f);
        ASSERT(VNEARLY3(rc_vec3f_add(a, b), rc_vec3f_make(5.0f, 7.0f, 9.0f)));
        ASSERT(VNEARLY3(rc_vec3f_sub(a, b), rc_vec3f_make(-3.0f, -3.0f, -3.0f)));
        ASSERT(NEARLY(rc_vec3f_dot(a, b), 32.0f));
        ASSERT(NEARLY(rc_vec3f_lengthsqr(a), 14.0f));
        ASSERT(NEARLY(rc_vec3f_length(a), sqrtf(14.0f)));
        ASSERT(VNEARLY3(rc_vec3f_scalar_div(rc_vec3f_make(6.0f, 9.0f, 12.0f), 3.0f),
                        rc_vec3f_make(2.0f, 3.0f, 4.0f)));
    }
    END_GROUP();

    BEGIN_GROUP("vec3f: cross product");
    {
        rc_vec3f x = rc_vec3f_make_unitx();
        rc_vec3f y = rc_vec3f_make_unity();
        rc_vec3f z = rc_vec3f_make_unitz();
        ASSERT(VNEARLY3(rc_vec3f_cross(x, y), z));
        ASSERT(VNEARLY3(rc_vec3f_cross(y, z), x));
        ASSERT(VNEARLY3(rc_vec3f_cross(z, x), y));
        /* anti-commutativity */
        rc_vec3f a = rc_vec3f_make(1.0f, 2.0f, 3.0f);
        rc_vec3f b = rc_vec3f_make(4.0f, 5.0f, 6.0f);
        ASSERT(VNEARLY3(rc_vec3f_cross(a, b),
                        rc_vec3f_negate(rc_vec3f_cross(b, a))));
    }
    END_GROUP();

    BEGIN_GROUP("vec3f: normalize");
    {
        rc_vec3f a = rc_vec3f_make(1.0f, 2.0f, 2.0f);  /* length = 3 */
        rc_vec3f n = rc_vec3f_normalize(a);
        ASSERT(NEARLY(rc_vec3f_length(n), 1.0f));
        ASSERT(VNEARLY3(n, rc_vec3f_make(1.0f/3.0f, 2.0f/3.0f, 2.0f/3.0f)));
        /* normalize_safe with zero */
        ASSERT(rc_vec3f_is_equal(
            rc_vec3f_normalize_safe(rc_vec3f_make_zero(), 1e-6f),
            rc_vec3f_make_zero()));
    }
    END_GROUP();

    /* ---- vec4f ---- */

    BEGIN_GROUP("vec4f: arithmetic");
    {
        rc_vec4f a = rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f);
        rc_vec4f b = rc_vec4f_make(4.0f, 3.0f, 2.0f, 1.0f);
        ASSERT(VNEARLY4(rc_vec4f_add(a, b), rc_vec4f_make(5.0f, 5.0f, 5.0f, 5.0f)));
        ASSERT(NEARLY(rc_vec4f_dot(a, b), 20.0f));
        ASSERT(NEARLY(rc_vec4f_lengthsqr(rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f)), 1.0f));
        ASSERT(VNEARLY4(rc_vec4f_scalar_div(rc_vec4f_make(6.0f, 9.0f, 12.0f, 15.0f), 3.0f),
                        rc_vec4f_make(2.0f, 3.0f, 4.0f, 5.0f)));
    }
    END_GROUP();

    /* ---- box2f ---- */

    BEGIN_GROUP("box2f: make sorts corners");
    {
        rc_box2f box = rc_box2f_make(
            rc_vec2f_make(5.0f, 3.0f),
            rc_vec2f_make(1.0f, 7.0f));
        ASSERT(NEARLY(box.min.x, 1.0f) && NEARLY(box.min.y, 3.0f));
        ASSERT(NEARLY(box.max.x, 5.0f) && NEARLY(box.max.y, 7.0f));
    }
    END_GROUP();

    BEGIN_GROUP("box2f: contains / intersects / contains_point");
    {
        rc_box2f outer = rc_box2f_make(rc_vec2f_make(0.0f,0.0f), rc_vec2f_make(10.0f,10.0f));
        rc_box2f inner = rc_box2f_make(rc_vec2f_make(2.0f,2.0f), rc_vec2f_make(5.0f, 5.0f));
        rc_box2f other = rc_box2f_make(rc_vec2f_make(8.0f,8.0f), rc_vec2f_make(12.0f,12.0f));
        rc_box2f away  = rc_box2f_make(rc_vec2f_make(20.0f,20.0f), rc_vec2f_make(30.0f,30.0f));
        /* Half-open [min, max): adjacent box sharing an edge does NOT intersect */
        rc_box2f adjacent = rc_box2f_make(rc_vec2f_make(10.0f,0.0f), rc_vec2f_make(20.0f,10.0f));
        ASSERT( rc_box2f_contains(outer, inner));
        ASSERT(!rc_box2f_contains(inner, outer));
        ASSERT( rc_box2f_intersects(outer, other));
        ASSERT(!rc_box2f_intersects(outer, away));
        ASSERT(!rc_box2f_intersects(outer, adjacent));   /* touching edge, not overlapping */
        /* contains_point: min inclusive, max exclusive */
        ASSERT( rc_box2f_contains_point(outer, rc_vec2f_make(0.0f, 0.0f)));  /* min corner: inside */
        ASSERT( rc_box2f_contains_point(outer, rc_vec2f_make(5.0f, 5.0f)));  /* interior */
        ASSERT(!rc_box2f_contains_point(outer, rc_vec2f_make(10.0f, 10.0f)));/* max corner: outside */
        ASSERT(!rc_box2f_contains_point(outer, rc_vec2f_make(10.0f, 5.0f))); /* on max edge: outside */
        ASSERT(!rc_box2f_contains_point(outer, rc_vec2f_make(11.0f, 5.0f))); /* beyond max: outside */
    }
    END_GROUP();

    BEGIN_GROUP("box2f: union / expand");
    {
        rc_box2f a = rc_box2f_make(rc_vec2f_make(0.0f,0.0f), rc_vec2f_make(3.0f,3.0f));
        rc_box2f b = rc_box2f_make(rc_vec2f_make(2.0f,2.0f), rc_vec2f_make(6.0f,6.0f));
        rc_box2f u = rc_box2f_union(a, b);
        ASSERT(NEARLY(u.min.x, 0.0f) && NEARLY(u.min.y, 0.0f));
        ASSERT(NEARLY(u.max.x, 6.0f) && NEARLY(u.max.y, 6.0f));
        rc_box2f e = rc_box2f_expand(a, rc_vec2f_make(10.0f, -1.0f));
        ASSERT(NEARLY(e.min.y, -1.0f) && NEARLY(e.max.x, 10.0f));
    }
    END_GROUP();

    /* ---- box2i ---- */

    BEGIN_GROUP("box2i: make sorts corners");
    {
        rc_box2i box = rc_box2i_make(
            rc_vec2i_make(5, 3),
            rc_vec2i_make(1, 7));
        ASSERT(box.min.x == 1 && box.min.y == 3);
        ASSERT(box.max.x == 5 && box.max.y == 7);
    }
    END_GROUP();

    BEGIN_GROUP("box2i: make_with_margin");
    {
        rc_box2i box = rc_box2i_make_with_margin(
            rc_vec2i_make(2, 2),
            rc_vec2i_make(8, 8), 1);
        ASSERT(box.min.x == 1 && box.min.y == 1);
        ASSERT(box.max.x == 9 && box.max.y == 9);
        /* zero margin is identity */
        rc_box2i base = rc_box2i_make(rc_vec2i_make(3, 3), rc_vec2i_make(7, 7));
        rc_box2i same = rc_box2i_make_with_margin(rc_vec2i_make(3, 3), rc_vec2i_make(7, 7), 0);
        ASSERT(same.min.x == base.min.x && same.min.y == base.min.y);
        ASSERT(same.max.x == base.max.x && same.max.y == base.max.y);
    }
    END_GROUP();

    BEGIN_GROUP("box2i: contains / intersects / contains_point");
    {
        rc_box2i outer    = rc_box2i_make(rc_vec2i_make(0,0),   rc_vec2i_make(10,10));
        rc_box2i inner    = rc_box2i_make(rc_vec2i_make(2,2),   rc_vec2i_make(5,5));
        rc_box2i other    = rc_box2i_make(rc_vec2i_make(8,8),   rc_vec2i_make(12,12));
        rc_box2i away     = rc_box2i_make(rc_vec2i_make(20,20), rc_vec2i_make(30,30));
        /* Half-open [min, max): adjacent box sharing an edge does NOT intersect */
        rc_box2i adjacent = rc_box2i_make(rc_vec2i_make(10,0),  rc_vec2i_make(20,10));
        ASSERT( rc_box2i_contains(outer, inner));
        ASSERT(!rc_box2i_contains(inner, outer));
        ASSERT( rc_box2i_intersects(outer, other));
        ASSERT(!rc_box2i_intersects(outer, away));
        ASSERT(!rc_box2i_intersects(outer, adjacent));   /* touching edge, not overlapping */
        /* contains_point: min inclusive, max exclusive */
        ASSERT( rc_box2i_contains_point(outer, rc_vec2i_make(0, 0)));    /* min corner: inside */
        ASSERT( rc_box2i_contains_point(outer, rc_vec2i_make(5, 5)));    /* interior */
        ASSERT(!rc_box2i_contains_point(outer, rc_vec2i_make(10, 10)));  /* max corner: outside */
        ASSERT(!rc_box2i_contains_point(outer, rc_vec2i_make(10, 5)));   /* on max edge: outside */
        ASSERT(!rc_box2i_contains_point(outer, rc_vec2i_make(11, 5)));   /* beyond max: outside */
    }
    END_GROUP();

    BEGIN_GROUP("box2i: union / expand");
    {
        rc_box2i a = rc_box2i_make(rc_vec2i_make(0,0), rc_vec2i_make(3,3));
        rc_box2i b = rc_box2i_make(rc_vec2i_make(2,2), rc_vec2i_make(6,6));
        rc_box2i u = rc_box2i_union(a, b);
        ASSERT(u.min.x == 0 && u.min.y == 0);
        ASSERT(u.max.x == 6 && u.max.y == 6);
        rc_box2i e = rc_box2i_expand(a, rc_vec2i_make(10, -1));
        ASSERT(e.min.y == -1 && e.max.x == 10);
    }
    END_GROUP();

    /* ---- mat22f ---- */

    BEGIN_GROUP("mat22f: identity * v = v");
    {
        rc_mat22f id = rc_mat22f_make_identity();
        rc_vec2f v = rc_vec2f_make(2.0f, 3.0f);
        ASSERT(VNEARLY2(rc_mat22f_vec2f_mul(id, v), v));
    }
    END_GROUP();

    BEGIN_GROUP("mat22f: rotation 90 degrees");
    {
        rc_mat22f r = rc_mat22f_make_rotation(rc_deg_to_rad(90.0f));
        rc_vec2f x = rc_vec2f_make_unitx();
        /* Rotating x 90° counter-clockwise gives y. */
        ASSERT(VNEARLY2(rc_mat22f_vec2f_mul(r, x), rc_vec2f_make_unity()));
    }
    END_GROUP();

    BEGIN_GROUP("mat22f: determinant");
    {
        rc_mat22f m = rc_mat22f_make(
            rc_vec2f_make(3.0f, 1.0f),
            rc_vec2f_make(2.0f, 4.0f));
        ASSERT(NEARLY(rc_mat22f_determinant(m), 10.0f));   /* 3*4 - 1*2 */
    }
    END_GROUP();

    BEGIN_GROUP("mat22f: inverse: M * M^-1 = I");
    {
        rc_mat22f m = rc_mat22f_make(
            rc_vec2f_make(2.0f, 1.0f),
            rc_vec2f_make(5.0f, 3.0f));
        rc_mat22f mi = rc_mat22f_inverse(m);
        rc_mat22f id = rc_mat22f_mul(m, mi);
        ASSERT(NEARLY(id.cx.x, 1.0f) && NEARLY(id.cx.y, 0.0f));
        ASSERT(NEARLY(id.cy.x, 0.0f) && NEARLY(id.cy.y, 1.0f));
    }
    END_GROUP();

    BEGIN_GROUP("mat22f: transpose");
    {
        rc_mat22f m = rc_mat22f_make(
            rc_vec2f_make(1.0f, 2.0f),
            rc_vec2f_make(3.0f, 4.0f));
        rc_mat22f t = rc_mat22f_transpose(m);
        ASSERT(NEARLY(t.cx.x, 1.0f) && NEARLY(t.cx.y, 3.0f));
        ASSERT(NEARLY(t.cy.x, 2.0f) && NEARLY(t.cy.y, 4.0f));
    }
    END_GROUP();

    /* ---- mat23f ---- */

    BEGIN_GROUP("mat23f: identity transform");
    {
        rc_mat23f id = rc_mat23f_make_identity();
        rc_vec2f v = rc_vec2f_make(5.0f, -3.0f);
        ASSERT(VNEARLY2(rc_mat23f_vec2f_mul(id, v), v));
    }
    END_GROUP();

    BEGIN_GROUP("mat23f: pure translation");
    {
        rc_mat23f t = rc_mat23f_make_translation(rc_vec2f_make(10.0f, -5.0f));
        rc_vec2f v  = rc_vec2f_make(1.0f, 2.0f);
        ASSERT(VNEARLY2(rc_mat23f_vec2f_mul(t, v), rc_vec2f_make(11.0f, -3.0f)));
    }
    END_GROUP();

    BEGIN_GROUP("mat23f: inverse: T * T^-1 = identity transform");
    {
        rc_mat23f t = rc_mat23f_make(
            rc_mat22f_make_rotation(rc_deg_to_rad(30.0f)),
            rc_vec2f_make(2.0f, -1.0f));
        rc_mat23f ti = rc_mat23f_inverse(t);
        rc_vec2f v = rc_vec2f_make(5.0f, 3.0f);
        ASSERT(VNEARLY2(rc_mat23f_vec2f_mul(ti, rc_mat23f_vec2f_mul(t, v)), v));
    }
    END_GROUP();

    /* ---- mat33f ---- */

    BEGIN_GROUP("mat33f: identity * v = v");
    {
        rc_mat33f id = rc_mat33f_make_identity();
        rc_vec3f v = rc_vec3f_make(1.0f, 2.0f, 3.0f);
        ASSERT(VNEARLY3(rc_mat33f_vec3f_mul(id, v), v));
    }
    END_GROUP();

    BEGIN_GROUP("mat33f: rotation_z 90 degrees");
    {
        rc_mat33f r = rc_mat33f_make_rotation_z(rc_deg_to_rad(90.0f));
        rc_vec3f x = rc_vec3f_make_unitx();
        ASSERT(VNEARLY3(rc_mat33f_vec3f_mul(r, x), rc_vec3f_make_unity()));
    }
    END_GROUP();

    BEGIN_GROUP("mat33f: determinant and inverse");
    {
        /* Rotation matrices have det = 1 and M^T = M^-1. */
        rc_mat33f r = rc_mat33f_make_rotation_y(rc_deg_to_rad(45.0f));
        ASSERT(NEARLY(rc_mat33f_determinant(r), 1.0f));
        rc_mat33f ri = rc_mat33f_inverse(r);
        rc_mat33f id = rc_mat33f_mul(r, ri);
        ASSERT(NEARLY(id.cx.x, 1.0f) && NEARLY(id.cy.y, 1.0f) && NEARLY(id.cz.z, 1.0f));
        ASSERT(NEARLY(id.cx.y, 0.0f) && NEARLY(id.cx.z, 0.0f));
    }
    END_GROUP();

    BEGIN_GROUP("mat33f: transpose of rotation = inverse");
    {
        rc_mat33f r  = rc_mat33f_make_rotation_x(rc_deg_to_rad(60.0f));
        rc_mat33f rt = rc_mat33f_transpose(r);
        rc_mat33f ri = rc_mat33f_inverse(r);
        /* Each column of rt should equal the corresponding column of ri. */
        ASSERT(VNEARLY3(rt.cx, ri.cx) && VNEARLY3(rt.cy, ri.cy) && VNEARLY3(rt.cz, ri.cz));
    }
    END_GROUP();

    /* ---- mat34f ---- */

    BEGIN_GROUP("mat34f: identity transform");
    {
        rc_mat34f id = rc_mat34f_make_identity();
        rc_vec3f v = rc_vec3f_make(1.0f, 2.0f, 3.0f);
        ASSERT(VNEARLY3(rc_mat34f_vec3f_mul(id, v), v));
    }
    END_GROUP();

    BEGIN_GROUP("mat34f: translation");
    {
        rc_mat34f t = rc_mat34f_make_translation(rc_vec3f_make(1.0f, 2.0f, 3.0f));
        rc_vec3f  v = rc_vec3f_make_zero();
        ASSERT(VNEARLY3(rc_mat34f_vec3f_mul(t, v), rc_vec3f_make(1.0f, 2.0f, 3.0f)));
    }
    END_GROUP();

    BEGIN_GROUP("mat34f: inverse: T * T^-1 = identity");
    {
        rc_mat34f t = rc_mat34f_make(
            rc_mat33f_make_rotation_z(rc_deg_to_rad(45.0f)),
            rc_vec3f_make(1.0f, -2.0f, 3.0f));
        rc_mat34f ti = rc_mat34f_inverse(t);
        rc_vec3f v = rc_vec3f_make(5.0f, 0.0f, -1.0f);
        ASSERT(VNEARLY3(rc_mat34f_vec3f_mul(ti, rc_mat34f_vec3f_mul(t, v)), v));
    }
    END_GROUP();

    BEGIN_GROUP("mat34f: lookat origin points at focus");
    {
        rc_vec3f eye   = rc_vec3f_make(0.0f, 0.0f, 5.0f);
        rc_vec3f focus = rc_vec3f_make_zero();
        rc_vec3f up    = rc_vec3f_make_unity();
        rc_mat34f lk   = rc_mat34f_make_lookat(eye, focus, up);
        /* Eye transforms to origin in camera space. */
        rc_vec3f cam_eye = rc_mat34f_vec3f_mul(lk, eye);
        ASSERT(VNEARLY3(cam_eye, rc_vec3f_make_zero()));
        /* Forward is -Z in camera space, so focus point → (0,0,-5). */
        rc_vec3f cam_focus = rc_mat34f_vec3f_mul(lk, focus);
        ASSERT(NEARLY(cam_focus.z, -5.0f));
    }
    END_GROUP();

    /* ---- mat44f ---- */

    BEGIN_GROUP("mat44f: identity * v = v");
    {
        rc_mat44f id = rc_mat44f_make_identity();
        rc_vec4f v = rc_vec4f_make(1.0f, 2.0f, 3.0f, 1.0f);
        ASSERT(VNEARLY4(rc_mat44f_vec4f_mul(id, v), v));
    }
    END_GROUP();

    BEGIN_GROUP("mat44f: determinant of identity = 1");
    {
        ASSERT(NEARLY(rc_mat44f_determinant(rc_mat44f_make_identity()), 1.0f));
    }
    END_GROUP();

    BEGIN_GROUP("mat44f: inverse: M * M^-1 = I");
    {
        rc_mat44f m = rc_mat44f_make_perspective(rc_deg_to_rad(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
        rc_mat44f mi = rc_mat44f_inverse(m);
        rc_mat44f id = rc_mat44f_mul(m, mi);
        ASSERT(NEARLY(id.cx.x, 1.0f) && NEARLY(id.cy.y, 1.0f));
        ASSERT(NEARLY(id.cz.z, 1.0f) && NEARLY(id.cw.w, 1.0f));
        ASSERT(NEARLY(id.cx.y, 0.0f) && NEARLY(id.cx.z, 0.0f));
    }
    END_GROUP();

    BEGIN_GROUP("mat44f: from_mat34f then vec4f_mul (w=1) matches mat34f_vec3f_mul");
    {
        rc_mat34f t = rc_mat34f_make(
            rc_mat33f_make_rotation_z(rc_deg_to_rad(30.0f)),
            rc_vec3f_make(1.0f, 2.0f, 3.0f));
        rc_mat44f m44 = rc_mat44f_from_mat34f(t);
        rc_vec3f  v   = rc_vec3f_make(4.0f, 5.0f, 6.0f);
        rc_vec4f  r44 = rc_mat44f_vec4f_mul(m44, rc_vec4f_from_vec3f(v, 1.0f));
        rc_vec3f  r34 = rc_mat34f_vec3f_mul(t, v);
        ASSERT(NEARLY(r44.x, r34.x) && NEARLY(r44.y, r34.y) && NEARLY(r44.z, r34.z));
        ASSERT(NEARLY(r44.w, 1.0f));
    }
    END_GROUP();

    BEGIN_GROUP("mat44f: ortho maps corners to NDC ±1");
    {
        rc_mat44f o = rc_mat44f_make_ortho(-2.0f, 2.0f, 1.0f, -1.0f, 1.0f, 10.0f);
        /* right edge x=2 → NDC x=1 */
        rc_vec4f right = rc_mat44f_vec4f_mul(o, rc_vec4f_make(2.0f, 0.0f, -1.0f, 1.0f));
        ASSERT(NEARLY(right.x, 1.0f));
        /* top edge y=1 → NDC y=1 */
        rc_vec4f top = rc_mat44f_vec4f_mul(o, rc_vec4f_make(0.0f, 1.0f, -1.0f, 1.0f));
        ASSERT(NEARLY(top.y, 1.0f));
    }
    END_GROUP();

    /* ---- quatf ---- */

    BEGIN_GROUP("quatf: identity rotates nothing");
    {
        rc_quatf q = rc_quatf_make_identity();
        rc_vec3f v = rc_vec3f_make(1.0f, 2.0f, 3.0f);
        ASSERT(VNEARLY3(rc_quatf_vec3f_transform(q, v), v));
    }
    END_GROUP();

    BEGIN_GROUP("quatf: 90° rotation about Z");
    {
        rc_quatf q = rc_quatf_make_angle_axis(rc_deg_to_rad(90.0f), rc_vec3f_make_unitz());
        /* x → y */
        ASSERT(VNEARLY3(rc_quatf_vec3f_transform(q, rc_vec3f_make_unitx()),
                        rc_vec3f_make_unity()));
        /* y → -x */
        ASSERT(VNEARLY3(rc_quatf_vec3f_transform(q, rc_vec3f_make_unity()),
                        rc_vec3f_negate(rc_vec3f_make_unitx())));
    }
    END_GROUP();

    BEGIN_GROUP("quatf: mat33f roundtrip");
    {
        rc_quatf q  = rc_quatf_make_angle_axis(rc_deg_to_rad(37.0f),
                          rc_vec3f_normalize(rc_vec3f_make(1.0f, 2.0f, 3.0f)));
        rc_mat33f m = rc_mat33f_from_quatf(q);
        rc_quatf  q2 = rc_quatf_from_mat33f(m);
        /* q and q2 should represent the same rotation.
         * They may differ in sign (q and -q are the same rotation). */
        rc_vec3f v = rc_vec3f_make(5.0f, -1.0f, 2.0f);
        ASSERT(VNEARLY3(rc_quatf_vec3f_transform(q, v),
                        rc_quatf_vec3f_transform(q2, v)));
    }
    END_GROUP();

    BEGIN_GROUP("quatf: mul composes rotations");
    {
        /* Two 90° Z rotations compose to 180°. */
        rc_quatf q90  = rc_quatf_make_angle_axis(rc_deg_to_rad(90.0f),  rc_vec3f_make_unitz());
        rc_quatf q180 = rc_quatf_make_angle_axis(rc_deg_to_rad(180.0f), rc_vec3f_make_unitz());
        rc_quatf qc   = rc_quatf_mul(q90, q90);
        rc_vec3f v = rc_vec3f_make(1.0f, 0.0f, 0.0f);
        ASSERT(VNEARLY3(rc_quatf_vec3f_transform(qc,   v),
                        rc_quatf_vec3f_transform(q180, v)));
    }
    END_GROUP();

    BEGIN_GROUP("quatf: conjugate is inverse for unit quaternion");
    {
        rc_quatf q = rc_quatf_make_angle_axis(rc_deg_to_rad(60.0f),
                         rc_vec3f_normalize(rc_vec3f_make(1.0f, 1.0f, 0.0f)));
        rc_quatf qi = rc_quatf_conjugate(q);
        rc_vec3f v  = rc_vec3f_make(3.0f, -1.0f, 2.0f);
        /* q * qi should be identity: (q applied then qi) = no-op */
        rc_vec3f result = rc_quatf_vec3f_transform(qi, rc_quatf_vec3f_transform(q, v));
        ASSERT(VNEARLY3(result, v));
    }
    END_GROUP();

    /* ---- rational ---- */

    BEGIN_GROUP("rational: make reduces to lowest terms");
    {
        rc_rational r = rc_rational_make(6, 4);
        ASSERT(r.num == 3 && r.denom == 2);
        rc_rational r2 = rc_rational_make(-6, 4);
        ASSERT(r2.num == -3 && r2.denom == 2);
        /* Negative denominator gets normalised. */
        rc_rational r3 = rc_rational_make(3, -6);
        ASSERT(r3.num == -1 && r3.denom == 2);
    }
    END_GROUP();

    BEGIN_GROUP("rational: make zero denom = invalid");
    {
        rc_rational r = rc_rational_make(5, 0);
        ASSERT(r.denom == 0);
        ASSERT(!rc_rational_is_valid(r));
    }
    END_GROUP();

    BEGIN_GROUP("rational: from_i64");
    {
        rc_rational r = rc_rational_from_i64(-7);
        ASSERT(r.num == -7 && r.denom == 1);
        ASSERT(rc_rational_is_valid(r));
    }
    END_GROUP();

    BEGIN_GROUP("rational: from_double");
    {
        /* Zero and integers. */
        rc_rational r0 = rc_rational_from_double(0.0, 0.001);
        ASSERT(r0.num == 0 && r0.denom == 1);
        rc_rational r1 = rc_rational_from_double(3.0, 0.001);
        ASSERT(r1.num == 3 && r1.denom == 1);

        /* Simple fractions. */
        rc_rational r2 = rc_rational_from_double(0.5, 0.001);
        ASSERT(r2.num == 1 && r2.denom == 2);
        rc_rational r3 = rc_rational_from_double(1.0 / 3.0, 1e-6);
        ASSERT(r3.num == 1 && r3.denom == 3);

        /* Negative values. */
        rc_rational r4 = rc_rational_from_double(-0.5, 0.001);
        ASSERT(r4.num == -1 && r4.denom == 2);
        rc_rational r5 = rc_rational_from_double(-1.0 / 3.0, 1e-6);
        ASSERT(r5.num == -1 && r5.denom == 3);

        /*
         * π: first convergent inside each tolerance band.
         * |π - 22/7|   ≈ 0.00126 < 0.002  → {22, 7}
         * |π - 333/106| ≈ 0.0000832 < 0.001 → {333, 106}
         */
        double pi = 3.14159265358979323846;
        rc_rational rpi1 = rc_rational_from_double(pi, 0.002);
        ASSERT(rpi1.num == 22 && rpi1.denom == 7);
        rc_rational rpi2 = rc_rational_from_double(pi, 0.001);
        ASSERT(rpi2.num == 333 && rpi2.denom == 106);

        /* Wide threshold accepts the integer part alone. */
        rc_rational r6 = rc_rational_from_double(3.14, 0.5);
        ASSERT(r6.num == 3 && r6.denom == 1);

        /* threshold = 0 still finds a compact representation for exact
         * machine fractions (0.5 is exactly 1/2 in IEEE 754). */
        rc_rational r7 = rc_rational_from_double(0.5, 0.0);
        ASSERT(r7.num == 1 && r7.denom == 2);
    }
    END_GROUP();

    BEGIN_GROUP("rational: mul and int_mul");
    {
        rc_rational a = rc_rational_make(1, 2);
        rc_rational b = rc_rational_make(2, 3);
        rc_rational r = rc_rational_mul(a, b);
        ASSERT(r.num == 1 && r.denom == 3);
        rc_rational r2 = rc_rational_int_mul(a, 4);
        ASSERT(r2.num == 2 && r2.denom == 1);
    }
    END_GROUP();

    BEGIN_GROUP("rational: int_div — positive and negative b");
    {
        rc_rational a = rc_rational_make(1, 2);
        /* 1/2 / 3 = 1/6 */
        rc_rational r = rc_rational_int_div(a, 3);
        ASSERT(r.num == 1 && r.denom == 6);
        /* 1/2 / (-2) = -1/4  (sign must be in numerator) */
        rc_rational r2 = rc_rational_int_div(a, -2);
        ASSERT(r2.num == -1 && r2.denom == 4);
    }
    END_GROUP();

    BEGIN_GROUP("rational: div");
    {
        rc_rational a = rc_rational_make(3, 4);
        rc_rational b = rc_rational_make(9, 8);
        /* (3/4) / (9/8) = (3/4) * (8/9) = 24/36 = 2/3 */
        rc_rational r = rc_rational_div(a, b);
        ASSERT(r.num == 2 && r.denom == 3);
    }
    END_GROUP();

    BEGIN_GROUP("rational: add reduces to lowest terms");
    {
        rc_rational a = rc_rational_make(1, 4);
        rc_rational b = rc_rational_make(1, 4);
        rc_rational r = rc_rational_add(a, b);
        /* 1/4 + 1/4 = 2/4 → 1/2 */
        ASSERT(r.num == 1 && r.denom == 2);
        rc_rational c = rc_rational_make(1, 3);
        rc_rational d = rc_rational_make(1, 6);
        /* 1/3 + 1/6 = 2/6 + 1/6 = 3/6 → 1/2 */
        rc_rational r2 = rc_rational_add(c, d);
        ASSERT(r2.num == 1 && r2.denom == 2);
    }
    END_GROUP();

    BEGIN_GROUP("rational: sub reduces to lowest terms");
    {
        rc_rational a = rc_rational_make(3, 4);
        rc_rational b = rc_rational_make(1, 4);
        rc_rational r = rc_rational_sub(a, b);
        /* 3/4 - 1/4 = 2/4 → 1/2 */
        ASSERT(r.num == 1 && r.denom == 2);
        /* Result can be negative */
        rc_rational r2 = rc_rational_sub(b, a);
        ASSERT(r2.num == -1 && r2.denom == 2);
    }
    END_GROUP();

    BEGIN_GROUP("rational: int_add and int_sub");
    {
        rc_rational a = rc_rational_make(1, 3);
        rc_rational r = rc_rational_int_add(a, 2);
        /* 1/3 + 2 = 7/3 */
        ASSERT(r.num == 7 && r.denom == 3);
        rc_rational r2 = rc_rational_int_sub(a, 1);
        /* 1/3 - 1 = -2/3 */
        ASSERT(r2.num == -2 && r2.denom == 3);
    }
    END_GROUP();

    BEGIN_GROUP("rational: is_valid on canonical forms");
    {
        ASSERT( rc_rational_is_valid(rc_rational_make(0,  1)));
        ASSERT( rc_rational_is_valid(rc_rational_make(1,  1)));
        ASSERT( rc_rational_is_valid(rc_rational_make(-1, 2)));
        /* Raw non-canonical struct: GCD(2,4)=2 != 1 → invalid. */
        ASSERT(!rc_rational_is_valid((rc_rational) {2, 4}));
        ASSERT(!rc_rational_is_valid((rc_rational) {0, 0}));
    }
    END_GROUP();

    BEGIN_GROUP("rational: predicates");
    {
        rc_rational zero     = rc_rational_from_i64(0);
        rc_rational pos      = rc_rational_make(3, 4);
        rc_rational neg      = rc_rational_make(-3, 4);
        rc_rational integer  = rc_rational_from_i64(5);

        ASSERT( rc_rational_is_zero(zero));
        ASSERT(!rc_rational_is_zero(pos));

        ASSERT( rc_rational_is_integer(zero));
        ASSERT( rc_rational_is_integer(integer));
        ASSERT(!rc_rational_is_integer(pos));

        ASSERT( rc_rational_is_positive(pos));
        ASSERT(!rc_rational_is_positive(zero));
        ASSERT(!rc_rational_is_positive(neg));

        ASSERT( rc_rational_is_negative(neg));
        ASSERT(!rc_rational_is_negative(zero));
        ASSERT(!rc_rational_is_negative(pos));
    }
    END_GROUP();

    BEGIN_GROUP("rational: negate, abs, reciprocal");
    {
        rc_rational a = rc_rational_make(3, 4);

        rc_rational neg = rc_rational_negate(a);
        ASSERT(neg.num == -3 && neg.denom == 4);
        /* Negate twice → original. */
        rc_rational back = rc_rational_negate(neg);
        ASSERT(back.num == 3 && back.denom == 4);

        /* abs of positive is unchanged. */
        rc_rational abspos = rc_rational_abs(a);
        ASSERT(abspos.num == 3 && abspos.denom == 4);
        /* abs of negative flips sign. */
        rc_rational absneg = rc_rational_abs(rc_rational_make(-5, 7));
        ASSERT(absneg.num == 5 && absneg.denom == 7);

        /* reciprocal of positive: swap num and denom. */
        rc_rational rp = rc_rational_reciprocal(a);
        ASSERT(rp.num == 4 && rp.denom == 3);
        /* reciprocal of negative: sign stays in numerator. */
        rc_rational rn = rc_rational_reciprocal(rc_rational_make(-2, 5));
        ASSERT(rn.num == -5 && rn.denom == 2);
        /* reciprocal of integer: 1/n */
        rc_rational ri = rc_rational_reciprocal(rc_rational_from_i64(3));
        ASSERT(ri.num == 1 && ri.denom == 3);
    }
    END_GROUP();

    BEGIN_GROUP("rational: compare, is_equal, is_less_than, is_greater_than");
    {
        rc_rational a = rc_rational_make(1, 3);
        rc_rational b = rc_rational_make(1, 2);
        rc_rational c = rc_rational_make(1, 3);

        ASSERT(rc_rational_compare(a, b) == -1);
        ASSERT(rc_rational_compare(b, a) ==  1);
        ASSERT(rc_rational_compare(a, c) ==  0);

        ASSERT( rc_rational_is_equal(a, c));
        ASSERT(!rc_rational_is_equal(a, b));

        ASSERT( rc_rational_is_less_than(a, b));
        ASSERT(!rc_rational_is_less_than(b, a));
        ASSERT(!rc_rational_is_less_than(a, c));

        ASSERT( rc_rational_is_greater_than(b, a));
        ASSERT(!rc_rational_is_greater_than(a, b));
        ASSERT(!rc_rational_is_greater_than(a, c));

        /* Negative vs positive. */
        rc_rational neg = rc_rational_make(-1, 2);
        ASSERT(rc_rational_is_less_than(neg, a));
        ASSERT(rc_rational_compare(neg, a) == -1);
    }
    END_GROUP();

    BEGIN_GROUP("rational: min and max");
    {
        rc_rational a = rc_rational_make(1, 4);
        rc_rational b = rc_rational_make(3, 4);

        rc_rational lo = rc_rational_min(a, b);
        ASSERT(lo.num == 1 && lo.denom == 4);
        rc_rational hi = rc_rational_max(a, b);
        ASSERT(hi.num == 3 && hi.denom == 4);

        /* Equal inputs: min and max both return the same value. */
        rc_rational same = rc_rational_min(a, a);
        ASSERT(same.num == 1 && same.denom == 4);
    }
    END_GROUP();

    BEGIN_GROUP("rational: to_double");
    {
        double d1 = rc_rational_to_double(rc_rational_make(1, 4));
        ASSERT(d1 == 0.25);
        double d2 = rc_rational_to_double(rc_rational_make(-1, 2));
        ASSERT(d2 == -0.5);
        double d3 = rc_rational_to_double(rc_rational_from_i64(3));
        ASSERT(d3 == 3.0);
    }
    END_GROUP();

    BEGIN_GROUP("rational: int_mul pre-reduction exercises gcd(denom, b)");
    {
        /* g = gcd(6, 3) = 3: result = {1*1, 2} not {3*1, 6} */
        rc_rational r = rc_rational_int_mul(rc_rational_make(1, 6), 3);
        ASSERT(r.num == 1 && r.denom == 2);
        /* Negative b: {3,4} * (-2) = {-3, 2}; g = gcd(4,2) = 2 */
        rc_rational r2 = rc_rational_int_mul(rc_rational_make(3, 4), -2);
        ASSERT(r2.num == -3 && r2.denom == 2);
        /* Multiply by zero → {0, 1}. */
        rc_rational r3 = rc_rational_int_mul(rc_rational_make(5, 7), 0);
        ASSERT(r3.num == 0 && r3.denom == 1);
        /*
         * Large value that would overflow int64_t without pre-reduction:
         * {922337203685477581, 10} * 10.
         * Without reduction: 922337203685477581 * 10 > INT64_MAX.
         * With:  g = gcd(10, 10) = 10, result = {922337203685477581 * 1, 1}.
         */
        rc_rational big = rc_rational_make(922337203685477581LL, 10);
        rc_rational r4 = rc_rational_int_mul(big, 10);
        ASSERT(r4.num == 922337203685477581LL && r4.denom == 1);
    }
    END_GROUP();

    BEGIN_GROUP("rational: mul cross-GCD pre-reduction");
    {
        /* {2,9} * {3,4}: g1=gcd(2,4)=2, g2=gcd(3,9)=3 → {1,6} */
        rc_rational r = rc_rational_mul(rc_rational_make(2, 9), rc_rational_make(3, 4));
        ASSERT(r.num == 1 && r.denom == 6);
        /* Negative numerator: {-2,9} * {3,4} = {-1,6} */
        rc_rational r2 = rc_rational_mul(rc_rational_make(-2, 9), rc_rational_make(3, 4));
        ASSERT(r2.num == -1 && r2.denom == 6);
        /* Multiply by zero rational → {0, 1}. */
        rc_rational r3 = rc_rational_mul(rc_rational_make(5, 7),
                                         rc_rational_from_i64(0));
        ASSERT(r3.num == 0 && r3.denom == 1);
        /*
         * Large values that would overflow without cross-GCD reduction:
         * {7, 2000000000000000000} * {2000000000000000000, 7}.
         * Without reduction: 7 * 2e18 = 14e18 > INT64_MAX.
         * With: g1=gcd(7,7)=7, g2=gcd(2e18,2e18)=2e18 → {1, 1}.
         */
        rc_rational a = rc_rational_make(7,                 2000000000000000000LL);
        rc_rational b = rc_rational_make(2000000000000000000LL, 7);
        rc_rational r4 = rc_rational_mul(a, b);
        ASSERT(r4.num == 1 && r4.denom == 1);
    }
    END_GROUP();

    BEGIN_GROUP("rational: int_div pre-reduction exercises gcd(num, b)");
    {
        /* g = gcd(6, 3) = 3: {6,7} / 3 = {2, 7} */
        rc_rational r = rc_rational_int_div(rc_rational_make(6, 7), 3);
        ASSERT(r.num == 2 && r.denom == 7);
        /* Negative b: {6,7} / (-3) = {-2, 7} */
        rc_rational r2 = rc_rational_int_div(rc_rational_make(6, 7), -3);
        ASSERT(r2.num == -2 && r2.denom == 7);
    }
    END_GROUP();

    BEGIN_GROUP("rational: div cross-GCD pre-reduction");
    {
        /* {4,9} / {8,3}: g1=gcd(4,8)=4, g2=gcd(9,3)=3 → {1,6} */
        rc_rational r = rc_rational_div(rc_rational_make(4, 9), rc_rational_make(8, 3));
        ASSERT(r.num == 1 && r.denom == 6);
        /* Division by a negative-numerator rational: {2,3} / {-4,5} = {-5,6} */
        rc_rational r2 = rc_rational_div(rc_rational_make(2, 3),
                                          rc_rational_make(-4, 5));
        ASSERT(r2.num == -5 && r2.denom == 6);
    }
    END_GROUP();

    BEGIN_GROUP("rational: add AHU — second GCD step reduces result further");
    {
        /*
         * {5,6} + {1,10}: d=gcd(6,10)=2, t=5*5+1*3=28, g=gcd(28,2)=2
         * result = {14, 3 * (10/2)} = {14, 15}.
         * Without the second gcd(t,d) step we'd pass 28/30 to
         * rc_rational_make instead of directly returning {14,15}.
         */
        rc_rational r = rc_rational_add(rc_rational_make(5, 6), rc_rational_make(1, 10));
        ASSERT(r.num == 14 && r.denom == 15);
        /* Additive inverse → {0, 1}. */
        rc_rational r2 = rc_rational_add(rc_rational_make(1, 4),
                                          rc_rational_make(-1, 4));
        ASSERT(r2.num == 0 && r2.denom == 1);
    }
    END_GROUP();

    BEGIN_GROUP("rational: sub AHU — second GCD step reduces result further");
    {
        /* {5,6} - {1,10} = 22/30 = 11/15 */
        rc_rational r = rc_rational_sub(rc_rational_make(5, 6), rc_rational_make(1, 10));
        ASSERT(r.num == 11 && r.denom == 15);
        /* Result equals addend → {0, 1}. */
        rc_rational r2 = rc_rational_sub(rc_rational_make(3, 7),
                                          rc_rational_make(3, 7));
        ASSERT(r2.num == 0 && r2.denom == 1);
    }
    END_GROUP();
}

static void test_bigint(void)
{
    rc_arena a       = rc_arena_make_default();
    rc_arena scratch = rc_arena_make_default();

    /* ---- construction and predicates ---- */

    BEGIN_GROUP("bigint: make and predicates");
    {
        rc_bigint z = rc_bigint_make(0, &a);
        ASSERT(rc_bigint_is_valid(&z));
        ASSERT(rc_bigint_is_zero(&z));
        ASSERT(!rc_bigint_is_positive(&z));
        ASSERT(!rc_bigint_is_negative(&z));
        ASSERT(z.cap >= 8);

        /* make with explicit capacity */
        rc_bigint big = rc_bigint_make(32, &a);
        ASSERT(big.cap >= 32);
        ASSERT(rc_bigint_is_zero(&big));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: from_u64");
    {
        rc_bigint r0 = rc_bigint_from_u64(0, &a);
        ASSERT(rc_bigint_is_zero(&r0));

        rc_bigint r1 = rc_bigint_from_u64(1, &a);
        ASSERT(rc_bigint_to_u64(&r1) == 1);
        ASSERT(rc_bigint_is_positive(&r1));

        rc_bigint rmax = rc_bigint_from_u64(UINT64_MAX, &a);
        ASSERT(rc_bigint_to_u64(&rmax) == UINT64_MAX);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: from_i64");
    {
        rc_bigint r0 = rc_bigint_from_i64(0, &a);
        ASSERT(rc_bigint_is_zero(&r0));

        rc_bigint rp = rc_bigint_from_i64(42, &a);
        ASSERT(rc_bigint_to_i64(&rp) == 42);
        ASSERT(rc_bigint_is_positive(&rp));

        rc_bigint rn = rc_bigint_from_i64(-42, &a);
        ASSERT(rc_bigint_to_i64(&rn) == -42);
        ASSERT(rc_bigint_is_negative(&rn));

        rc_bigint rmax = rc_bigint_from_i64(INT64_MAX, &a);
        ASSERT(rc_bigint_to_i64(&rmax) == INT64_MAX);

        rc_bigint rmin = rc_bigint_from_i64(INT64_MIN, &a);
        ASSERT(rc_bigint_to_i64(&rmin) == INT64_MIN);
        ASSERT(rc_bigint_is_negative(&rmin));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: copy");
    {
        rc_bigint src = rc_bigint_from_i64(-99, &a);
        rc_bigint dst = rc_bigint_copy(&src, &a);
        ASSERT(rc_bigint_to_i64(&dst) == -99);
        /* Modifying dst must not affect src. */
        rc_bigint_negate(&dst);
        ASSERT(rc_bigint_to_i64(&src) == -99);
        ASSERT(rc_bigint_to_i64(&dst) ==  99);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: reset and negate");
    {
        rc_bigint r = rc_bigint_from_i64(7, &a);
        rc_bigint_negate(&r);
        ASSERT(rc_bigint_to_i64(&r) == -7);
        rc_bigint_negate(&r);
        ASSERT(rc_bigint_to_i64(&r) == 7);
        rc_bigint_reset(&r);
        ASSERT(rc_bigint_is_zero(&r));
        ASSERT(r.cap > 0);   /* buffer retained */
    }
    END_GROUP();

    BEGIN_GROUP("bigint: reserve");
    {
        rc_bigint r = rc_bigint_from_i64(1, &a);
        uint32_t old_cap = r.cap;
        rc_bigint_reserve(&r, old_cap, &a);     /* no-op */
        ASSERT(r.cap == old_cap);
        rc_bigint_reserve(&r, old_cap + 1, &a); /* grow */
        ASSERT(r.cap > old_cap);
        ASSERT(rc_bigint_to_i64(&r) == 1);      /* value preserved */
    }
    END_GROUP();

    /* ---- add: zero cases ---- */

    BEGIN_GROUP("bigint: add zero + zero");
    {
        rc_bigint a0 = rc_bigint_make(0, &a);
        rc_bigint b0 = rc_bigint_make(0, &a);
        rc_bigint_add(&a0, &a0, &b0, &a);
        ASSERT(rc_bigint_is_zero(&a0));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add zero + positive");
    {
        rc_bigint a0 = rc_bigint_make(0, &a);
        rc_bigint b  = rc_bigint_from_i64(5, &a);
        rc_bigint_add(&a0, &a0, &b, &a);
        ASSERT(rc_bigint_to_i64(&a0) == 5);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add positive + zero");
    {
        rc_bigint r = rc_bigint_from_i64(5, &a);
        rc_bigint z = rc_bigint_make(0, &a);
        rc_bigint_add(&r, &r, &z, &a);
        ASSERT(rc_bigint_to_i64(&r) == 5);
    }
    END_GROUP();

    /* ---- add: same sign ---- */

    BEGIN_GROUP("bigint: add positive + positive, no carry");
    {
        rc_bigint r = rc_bigint_from_i64(3, &a);
        rc_bigint b = rc_bigint_from_i64(4, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add negative + negative");
    {
        rc_bigint r = rc_bigint_from_i64(-3, &a);
        rc_bigint b = rc_bigint_from_i64(-4, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add carries across limb boundary");
    {
        /*
         * UINT32_MAX + 1 produces a carry into a new most-significant limb.
         * Result should be 2^32 == {0, 1} in little-endian two-limb form.
         */
        rc_bigint r = rc_bigint_from_u64(UINT32_MAX, &a);
        rc_bigint b = rc_bigint_from_u64(1, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == 0);
        ASSERT(r.digits[1] == 1);
        ASSERT(r.sign == 1);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add carry ripple through multiple limbs");
    {
        /*
         * Start with {UINT32_MAX, UINT32_MAX} (= 2^64 - 1), add 1.
         * Expected result: {0, 0, 1} (= 2^64).
         */
        rc_bigint r = rc_bigint_make(4, &a);
        r.digits[0] = UINT32_MAX;
        r.digits[1] = UINT32_MAX;
        r.len  = 2;
        r.sign = 1;
        rc_bigint b = rc_bigint_from_u64(1, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(r.len == 3);
        ASSERT(r.digits[0] == 0);
        ASSERT(r.digits[1] == 0);
        ASSERT(r.digits[2] == 1);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add operands of different lengths (b longer)");
    {
        /*
         * a = 1, b = 2^32 + 1 (two limbs: {1, 1}).
         * Result = 2^32 + 2 (two limbs: {2, 1}).
         */
        rc_bigint r = rc_bigint_from_u64(1, &a);
        rc_bigint b = rc_bigint_make(2, &a);
        b.digits[0] = 1;
        b.digits[1] = 1;
        b.len  = 2;
        b.sign = 1;
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == 2);
        ASSERT(r.digits[1] == 1);
    }
    END_GROUP();

    /* ---- add: different signs (subtraction of magnitudes) ---- */

    BEGIN_GROUP("bigint: add cancellation (result zero)");
    {
        rc_bigint r = rc_bigint_from_i64(7, &a);
        rc_bigint b = rc_bigint_from_i64(-7, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add positive > |negative|");
    {
        rc_bigint r = rc_bigint_from_i64(10, &a);
        rc_bigint b = rc_bigint_from_i64(-3, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add positive < |negative|");
    {
        rc_bigint r = rc_bigint_from_i64(3, &a);
        rc_bigint b = rc_bigint_from_i64(-10, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add negative > |positive|");
    {
        /* -10 + 3 = -7 */
        rc_bigint r = rc_bigint_from_i64(-10, &a);
        rc_bigint b = rc_bigint_from_i64(3, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add different signs spanning multiple limbs");
    {
        /*
         * a = 2^32 (= {0, 1}), b = -(2^32 - 1) (= {UINT32_MAX}).
         * Result = 1.
         */
        rc_bigint r = rc_bigint_make(2, &a);
        r.digits[0] = 0;
        r.digits[1] = 1;
        r.len  = 2;
        r.sign = 1;
        rc_bigint b = rc_bigint_from_u64(UINT32_MAX, &a);
        rc_bigint_negate(&b);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(r.len == 1 && r.digits[0] == 1 && r.sign == 1);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: add borrow ripple through multiple limbs");
    {
        /*
         * a = 2^64 (= {0, 0, 1} in 32-bit limbs), b = -1.
         * Result = 2^64 - 1 (= {UINT32_MAX, UINT32_MAX}).
         */
        rc_bigint r = rc_bigint_make(3, &a);
        r.digits[0] = 0;
        r.digits[1] = 0;
        r.digits[2] = 1;
        r.len  = 3;
        r.sign = 1;
        rc_bigint b = rc_bigint_from_i64(-1, &a);
        rc_bigint_add(&r, &r, &b, &a);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == UINT32_MAX);
        ASSERT(r.digits[1] == UINT32_MAX);
        ASSERT(r.sign == 1);
    }
    END_GROUP();

    /* ---- add: self-add ---- */

    BEGIN_GROUP("bigint: self-add (a += a, doubling)");
    {
        rc_bigint r = rc_bigint_from_u64(21, &a);
        rc_bigint_add(&r, &r, &r, &a);
        ASSERT(rc_bigint_to_u64(&r) == 42);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: self-add with carry into new limb");
    {
        /* UINT32_MAX doubled = 2^33 - 2 = {UINT32_MAX - 1, 1} */
        rc_bigint r = rc_bigint_from_u64(UINT32_MAX, &a);
        rc_bigint_add(&r, &r, &r, &a);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == UINT32_MAX - 1);
        ASSERT(r.digits[1] == 1);
    }
    END_GROUP();

    /* ---- subtraction ---- */

    BEGIN_GROUP("bigint: sub zero - zero");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_make(0, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub positive - zero");
    {
        rc_bigint r = rc_bigint_from_i64(5, &a);
        rc_bigint b = rc_bigint_make(0, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == 5);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub zero - positive");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(5, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -5);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub positive - positive (a > b)");
    {
        rc_bigint r = rc_bigint_from_i64(10, &a);
        rc_bigint b = rc_bigint_from_i64(3, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub positive - positive (a < b)");
    {
        rc_bigint r = rc_bigint_from_i64(3, &a);
        rc_bigint b = rc_bigint_from_i64(10, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub positive - positive (a == b)");
    {
        rc_bigint r = rc_bigint_from_i64(7, &a);
        rc_bigint b = rc_bigint_from_i64(7, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub negative - negative (a more negative)");
    {
        /* -10 - (-3) = -7 */
        rc_bigint r = rc_bigint_from_i64(-10, &a);
        rc_bigint b = rc_bigint_from_i64(-3, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub negative - negative (b more negative)");
    {
        /* -3 - (-10) = 7 */
        rc_bigint r = rc_bigint_from_i64(-3, &a);
        rc_bigint b = rc_bigint_from_i64(-10, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub positive - negative (becomes addition)");
    {
        /* 3 - (-4) = 7 */
        rc_bigint r = rc_bigint_from_i64(3, &a);
        rc_bigint b = rc_bigint_from_i64(-4, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub negative - positive (becomes more negative)");
    {
        /* -3 - 4 = -7 */
        rc_bigint r = rc_bigint_from_i64(-3, &a);
        rc_bigint b = rc_bigint_from_i64(4, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(rc_bigint_to_i64(&r) == -7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub with borrow across limb boundary");
    {
        /* 2^32 - 1 = UINT32_MAX */
        rc_bigint r = rc_bigint_make(2, &a);
        r.digits[0] = 0;
        r.digits[1] = 1;
        r.len  = 2;
        r.sign = 1;
        rc_bigint b = rc_bigint_from_u64(1, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(r.len == 1 && r.digits[0] == UINT32_MAX && r.sign == 1);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub borrow ripple through multiple limbs");
    {
        /* 2^64 - 1 = {UINT32_MAX, UINT32_MAX} */
        rc_bigint r = rc_bigint_make(3, &a);
        r.digits[0] = 0;
        r.digits[1] = 0;
        r.digits[2] = 1;
        r.len  = 3;
        r.sign = 1;
        rc_bigint b = rc_bigint_from_u64(1, &a);
        rc_bigint_sub(&r, &r, &b, &a);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == UINT32_MAX);
        ASSERT(r.digits[1] == UINT32_MAX);
        ASSERT(r.sign == 1);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: self-sub (a -= a, result zero)");
    {
        rc_bigint r = rc_bigint_from_i64(42, &a);
        rc_bigint_sub(&r, &r, &r, &a);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    /* ---- multiplication ---- */

    BEGIN_GROUP("bigint: mul zero * anything = zero");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(42, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul anything * zero = zero");
    {
        rc_bigint r = rc_bigint_from_i64(42, &a);
        rc_bigint b = rc_bigint_make(0, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul positive * positive");
    {
        rc_bigint r = rc_bigint_from_i64(6, &a);
        rc_bigint b = rc_bigint_from_i64(7, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 42);
        ASSERT(rc_bigint_is_positive(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul positive * negative (result negative)");
    {
        rc_bigint r = rc_bigint_from_i64(6, &a);
        rc_bigint b = rc_bigint_from_i64(-7, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == -42);
        ASSERT(rc_bigint_is_negative(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul negative * positive (result negative)");
    {
        rc_bigint r = rc_bigint_from_i64(-6, &a);
        rc_bigint b = rc_bigint_from_i64(7, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == -42);
        ASSERT(rc_bigint_is_negative(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul negative * negative (result positive)");
    {
        rc_bigint r = rc_bigint_from_i64(-6, &a);
        rc_bigint b = rc_bigint_from_i64(-7, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 42);
        ASSERT(rc_bigint_is_positive(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul by 1 is identity");
    {
        rc_bigint r = rc_bigint_from_i64(12345, &a);
        rc_bigint b = rc_bigint_from_i64(1, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 12345);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul self-multiply");
    {
        /* 9 * 9 = 81 */
        rc_bigint r = rc_bigint_from_i64(9, &a);
        rc_bigint_mul(&r, &r, &r, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 81);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul carry into second limb");
    {
        /*
         * UINT32_MAX * UINT32_MAX = (2^32-1)^2 = 2^64 - 2^33 + 1.
         * In two 32-bit limbs:
         *   digits[0] = 1
         *   digits[1] = UINT32_MAX - 1  (= 0xFFFFFFFE)
         */
        rc_bigint r = rc_bigint_from_u64(UINT32_MAX, &a);
        rc_bigint b = rc_bigint_from_u64(UINT32_MAX, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == 1);
        ASSERT(r.digits[1] == UINT32_MAX - 1);
        ASSERT(r.sign == 1);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul multi-limb * single-limb");
    {
        /*
         * {1, 1} = 2^32 + 1; * 2 = 2^33 + 2 = {2, 2}.
         */
        rc_bigint r = rc_bigint_make(2, &a);
        r.digits[0] = 1;
        r.digits[1] = 1;
        r.len  = 2;
        r.sign = 1;
        rc_bigint b = rc_bigint_from_u64(2, &a);
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(r.len == 2);
        ASSERT(r.digits[0] == 2);
        ASSERT(r.digits[1] == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul multi-limb * multi-limb");
    {
        /*
         * {1, 1} = 2^32 + 1; squared = (2^32+1)^2 = 2^64 + 2^33 + 1.
         * In three 32-bit limbs: {1, 2, 1}.
         */
        rc_bigint r = rc_bigint_make(2, &a);
        r.digits[0] = 1;
        r.digits[1] = 1;
        r.len  = 2;
        r.sign = 1;
        rc_bigint b = rc_bigint_make(2, &a);
        b.digits[0] = 1;
        b.digits[1] = 1;
        b.len  = 2;
        b.sign = 1;
        rc_bigint_mul(&r, &r, &b, &a, scratch);
        ASSERT(r.len == 3);
        ASSERT(r.digits[0] == 1);
        ASSERT(r.digits[1] == 2);
        ASSERT(r.digits[2] == 1);
    }
    END_GROUP();

    /* ---- 3-address non-aliased form ---- */

    BEGIN_GROUP("bigint: add 3-address non-aliased");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(5, &a);
        rc_bigint c = rc_bigint_from_i64(3, &a);
        rc_bigint_add(&r, &b, &c, &a);
        ASSERT(rc_bigint_to_i64(&r) == 8);
        ASSERT(rc_bigint_to_i64(&b) == 5);
        ASSERT(rc_bigint_to_i64(&c) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: sub 3-address non-aliased");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(10, &a);
        rc_bigint c = rc_bigint_from_i64(3, &a);
        rc_bigint_sub(&r, &b, &c, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
        ASSERT(rc_bigint_to_i64(&b) == 10);
        ASSERT(rc_bigint_to_i64(&c) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mul 3-address non-aliased");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(6, &a);
        rc_bigint c = rc_bigint_from_i64(7, &a);
        rc_bigint_mul(&r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 42);
        ASSERT(rc_bigint_to_i64(&b) == 6);
        ASSERT(rc_bigint_to_i64(&c) == 7);
    }
    END_GROUP();

    /* ---- divmod ---- */

    BEGIN_GROUP("bigint: divmod 0/b = 0 rem 0");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_make(0, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_is_zero(&q));
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod b/1 = b rem 0");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(42, &a);
        rc_bigint c = rc_bigint_from_i64(1, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 42);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod b/b = 1 rem 0");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint c = rc_bigint_from_i64(17, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 1);
        ASSERT(rc_bigint_is_zero(&r));
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod 17/5 = 3 rem 2 (pos/pos)");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod -17/5 = -3 rem -2 (neg/pos)");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(-17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == -3);
        ASSERT(rc_bigint_to_i64(&r) == -2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod 17/-5 = -3 rem 2 (pos/neg)");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint c = rc_bigint_from_i64(-5, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == -3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod -17/-5 = 3 rem -2 (neg/neg)");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(-17, &a);
        rc_bigint c = rc_bigint_from_i64(-5, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == -2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod |b|<|c| gives quot=0 rem=b");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(3, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_is_zero(&q));
        ASSERT(rc_bigint_to_i64(&r) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod single-limb UINT32_MAX/3");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_u64((uint64_t)UINT32_MAX, &a);
        rc_bigint c = rc_bigint_from_i64(3, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_u64(&q) == (uint64_t)UINT32_MAX / 3);
        ASSERT(rc_bigint_to_u64(&r) == (uint64_t)UINT32_MAX % 3);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod multi-limb/single-limb (2^64-1)/7");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_u64(UINT64_MAX, &a);
        rc_bigint c = rc_bigint_from_i64(7, &a);
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_u64(&q) == UINT64_MAX / 7);
        ASSERT(rc_bigint_to_u64(&r) == UINT64_MAX % 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod multi-limb/multi-limb (2^64+1)/(2^32+1)");
    {
        /*
         * 2^64 + 1 = (2^32+1)*(2^32-1) + 2.
         * quot = 2^32-1 = UINT32_MAX, rem = 2.
         */
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_make(3, &a);
        b.digits[0] = 1; b.digits[1] = 0; b.digits[2] = 1;
        b.len = 3; b.sign = 1;
        rc_bigint c = rc_bigint_make(2, &a);
        c.digits[0] = 1; c.digits[1] = 1;
        c.len = 2; c.sign = 1;
        rc_bigint_divmod(&q, &r, &b, &c, &a, scratch);
        ASSERT(q.len == 1 && q.digits[0] == UINT32_MAX && q.sign == 1);
        ASSERT(r.len == 1 && r.digits[0] == 2 && r.sign == 1);
    }
    END_GROUP();

    /* ---- divmod aliasing ---- */

    BEGIN_GROUP("bigint: divmod quot==b aliasing");
    {
        rc_bigint q = rc_bigint_from_i64(17, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &q, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod quot==c aliasing");
    {
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint q = rc_bigint_from_i64(5, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint_divmod(&q, &r, &b, &q, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod rem==b aliasing");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_from_i64(17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &r, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: divmod rem==c aliasing");
    {
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_from_i64(5, &a);
        rc_bigint_divmod(&q, &r, &b, &r, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    /* ---- div ---- */

    BEGIN_GROUP("bigint: div 17/5 = 3");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_div(&r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: div -17/5 = -3");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(-17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_div(&r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == -3);
    }
    END_GROUP();

    /* ---- mod ---- */

    BEGIN_GROUP("bigint: mod 17%5 = 2");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_mod(&r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: mod -17%5 = -2");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(-17, &a);
        rc_bigint c = rc_bigint_from_i64(5, &a);
        rc_bigint_mod(&r, &b, &c, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == -2);
    }
    END_GROUP();

    /* ---- int_* shortcuts ---- */

    BEGIN_GROUP("bigint: int_add positive int");
    {
        rc_bigint r = rc_bigint_from_i64(10, &a);
        rc_bigint_int_add(&r, &r, 5, &a);
        ASSERT(rc_bigint_to_i64(&r) == 15);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_add negative int");
    {
        rc_bigint r = rc_bigint_from_i64(10, &a);
        rc_bigint_int_add(&r, &r, -3, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_add INT64_MIN (2-limb)");
    {
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(0, &a);
        rc_bigint_int_add(&r, &b, INT64_MIN, &a);
        ASSERT(rc_bigint_to_i64(&r) == INT64_MIN);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_sub basic");
    {
        rc_bigint r = rc_bigint_from_i64(10, &a);
        rc_bigint_int_sub(&r, &r, 3, &a);
        ASSERT(rc_bigint_to_i64(&r) == 7);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_mul basic");
    {
        rc_bigint r = rc_bigint_from_i64(6, &a);
        rc_bigint_int_mul(&r, &r, 7, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 42);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_mul negative");
    {
        rc_bigint r = rc_bigint_from_i64(6, &a);
        rc_bigint_int_mul(&r, &r, -7, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == -42);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_div basic");
    {
        rc_bigint r = rc_bigint_from_i64(17, &a);
        rc_bigint_int_div(&r, &r, 5, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 3);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_mod basic");
    {
        rc_bigint r = rc_bigint_from_i64(17, &a);
        rc_bigint_int_mod(&r, &r, 5, &a, scratch);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    BEGIN_GROUP("bigint: int_divmod basic");
    {
        rc_bigint q = rc_bigint_make(0, &a);
        rc_bigint r = rc_bigint_make(0, &a);
        rc_bigint b = rc_bigint_from_i64(17, &a);
        rc_bigint_int_divmod(&q, &r, &b, 5, &a, scratch);
        ASSERT(rc_bigint_to_i64(&q) == 3);
        ASSERT(rc_bigint_to_i64(&r) == 2);
    }
    END_GROUP();

    rc_arena_destroy(&scratch);
    rc_arena_destroy(&a);
}

static void test_file(void)
{
    static const char *const tmp = "richc_test_tmp.txt";

    /* ---- rc_save_text / rc_load_text ---- */

    BEGIN_GROUP("file: save and load roundtrip");
    {
        rc_arena a = rc_arena_make_default();
        ASSERT(rc_save_text(tmp, RC_STR("hello, world")) == RC_FILE_OK);
        rc_load_text_result r = rc_load_text(tmp, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(rc_str_is_equal(r.text, RC_STR("hello, world")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: save empty string creates empty file");
    {
        rc_arena a = rc_arena_make_default();
        ASSERT(rc_save_text(tmp, RC_STR("")) == RC_FILE_OK);
        rc_load_text_result r = rc_load_text(tmp, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(rc_str_is_valid(r.text) && rc_str_is_empty(r.text));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: loaded text is null-terminated (as_cstr fast path)");
    {
        rc_arena a = rc_arena_make_default();
        rc_save_text(tmp, RC_STR("abc"));
        rc_load_text_result r = rc_load_text(tmp, &a);
        ASSERT(r.error == RC_FILE_OK);
        /* rc_str_as_cstr returns data directly when already null-terminated */
        ASSERT(rc_str_as_cstr(r.text, NULL, 0) == r.text.data);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: load nonexistent file returns NOT_FOUND");
    {
        rc_arena a = rc_arena_make_default();
        rc_load_text_result r = rc_load_text("richc_nonexistent_12345.txt", &a);
        ASSERT(r.error == RC_FILE_ERROR_NOT_FOUND);
        ASSERT(!rc_str_is_valid(r.text));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: save and load multiline content");
    {
        rc_arena a = rc_arena_make_default();
        rc_save_text(tmp, RC_STR("line1\nline2\nline3"));
        rc_load_text_result r = rc_load_text(tmp, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(rc_str_is_equal(r.text, RC_STR("line1\nline2\nline3")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: save overwrites existing content");
    {
        rc_arena a = rc_arena_make_default();
        rc_save_text(tmp, RC_STR("original long content"));
        rc_save_text(tmp, RC_STR("new"));
        rc_load_text_result r = rc_load_text(tmp, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(rc_str_is_equal(r.text, RC_STR("new")));
        rc_arena_destroy(&a);
    }
    END_GROUP();

    remove(tmp);

    /* ---- rc_save_binary / rc_load_binary / rc_load_binary_array ---- */

    static const char *const tmp_bin = "richc_test_tmp.bin";

    BEGIN_GROUP("file: binary save and load roundtrip (null bytes and 0xFF)");
    {
        rc_arena a = rc_arena_make_default();
        const uint8_t bytes[] = {0x00, 0x01, 0xFF, 0x00, 0xAB};
        rc_view_bytes v = RC_VIEW(bytes);
        ASSERT(rc_save_binary(tmp_bin, v) == RC_FILE_OK);
        rc_load_binary_result r = rc_load_binary(tmp_bin, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(r.data.num == 5);
        ASSERT(memcmp(r.data.data, bytes, 5) == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: binary save empty creates empty file");
    {
        rc_arena a = rc_arena_make_default();
        ASSERT(rc_save_binary(tmp_bin, (rc_view_bytes) {NULL, 0}) == RC_FILE_OK);
        rc_load_binary_result r = rc_load_binary(tmp_bin, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(r.data.num == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: load_binary_array allows mutation via push");
    {
        rc_arena a = rc_arena_make_default();
        const uint8_t bytes[] = {1, 2, 3};
        rc_view_bytes v = RC_VIEW(bytes);
        rc_save_binary(tmp_bin, v);
        rc_load_binary_array_result r = rc_load_binary_array(tmp_bin, &a);
        ASSERT(r.error == RC_FILE_OK);
        ASSERT(r.data.num == 3);
        ASSERT(RC_AT(r.data, 0) == 1);
        ASSERT(RC_AT(r.data, 2) == 3);
        rc_array_bytes_push(&r.data, 4, &a);
        ASSERT(r.data.num == 4);
        ASSERT(RC_AT(r.data, 3) == 4);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: load binary nonexistent file returns NOT_FOUND");
    {
        rc_arena a = rc_arena_make_default();
        rc_load_binary_result r = rc_load_binary("richc_nonexistent_12345.bin", &a);
        ASSERT(r.error == RC_FILE_ERROR_NOT_FOUND);
        ASSERT(r.data.num == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    BEGIN_GROUP("file: load_binary_array nonexistent file returns NOT_FOUND");
    {
        rc_arena a = rc_arena_make_default();
        rc_load_binary_array_result r = rc_load_binary_array("richc_nonexistent_12345.bin", &a);
        ASSERT(r.error == RC_FILE_ERROR_NOT_FOUND);
        ASSERT(r.data.num == 0 && r.data.cap == 0);
        rc_arena_destroy(&a);
    }
    END_GROUP();

    remove(tmp_bin);
}
