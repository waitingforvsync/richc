/*
 * sort.h - template header: introsort on a mutable span.
 *
 * Uses quicksort (median-of-three pivot) for large spans, falling back to
 * heapsort when recursion depth exceeds 2*floor(log2(n)) to guarantee
 * O(n log n) worst-case, and insertion sort for small spans (n <= 16).  This
 * matches the strategy used by libstdc++ and libc++.  Include it again (after
 * redefining the control macros) to instantiate another element type.
 *
 * The median-of-three step sorts data[0], data[mid], and data[n-1] in place
 * before partitioning.  This has two benefits: it picks a better pivot on
 * average, and it leaves data[0] <= pivot and data[n-1] >= pivot as sentinels so
 * the inner partition scans need no bounds checks.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_SORT_TYPE   element type (required)
 *   RC_SORT_CTX    context type passed to the comparator (optional)
 *   RC_SORT_CMP    comparator expression (optional; see below)
 *   RC_SORT_SPAN   span type (optional; default rc_span_<TYPE>)
 *   RC_SORT_NAME   function name (optional; default rc_sort_<TYPE>)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Comparator conventions
 * ----------------------
 * Without RC_SORT_CTX:
 *   RC_SORT_CMP(a, b)        - true iff a < b.  Default: (a) < (b).
 * With RC_SORT_CTX:
 *   RC_SORT_CMP(ctx, a, b)   - true iff a < b given context ctx.  ctx is a
 *   pointer to RC_SORT_CTX and is the first argument.  Default: ignores ctx,
 *   uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  void NAME(SPAN span)
 * With context:     void NAME(SPAN span, CTX *ctx)
 *
 * Example (no context):
 *   #define RC_SORT_TYPE int
 *   #include "richc/template/algorithm/sort.h"
 *   // void rc_sort_int(rc_span_int span);
 *
 * Example (context comparator):
 *   #define RC_SORT_TYPE         widget
 *   #define RC_SORT_CTX          widget_cmp_ctx
 *   #define RC_SORT_CMP(ctx, a, b)  widget_less(ctx, a, b)
 *   #define RC_SORT_NAME         rc_sort_widget
 *   #include "richc/template/algorithm/sort.h"
 *   // void rc_sort_widget(rc_span_widget span, widget_cmp_ctx *ctx);
 */

#include <stdint.h>

#include "richc/macros.h"

#ifndef RC_SORT_TYPE
#  define RC_SORT_TYPE int   // to keep intellisense happy
#  error "RC_SORT_TYPE must be defined before including richc/template/algorithm/sort.h"
#endif

#ifndef RC_SORT_SPAN
#  define RC_SORT_SPAN RC_CONCAT(rc_span_, RC_SORT_TYPE)
#endif

#ifndef RC_SORT_NAME
#  define RC_SORT_NAME RC_CONCAT(rc_sort_, RC_SORT_TYPE)
#endif

/*
 * Comparator and optional context.
 *
 * RC_SORT_CMP_ is the internal two-argument comparator used at every call site
 * within the generated functions; when a context type is active it closes over
 * the 'ctx' parameter threaded through the call chain.
 *
 * Every helper takes the span first and the context (when present) immediately
 * after it, threaded through a matching parameter/argument pair:
 *   RC_SORT_SPAN_CTX_PARAM_   - leading parameter list: "SPAN span" or
 *                               "SPAN span, CTX *ctx".  Any further parameters
 *                               follow with their own comma.
 *   RC_SORT_SPAN_CTX_ARG_(s)  - the matching call argument: "s" or "s, ctx",
 *                               where s is the span expression at the call site.
 *
 * So ctx flows through the whole chain without any change to the algorithm.  The
 * default context comparator folds (void)ctx into the comma expression to
 * suppress the unused-parameter warning.
 */
#ifdef RC_SORT_CTX
#  ifndef RC_SORT_CMP
#    define RC_SORT_CMP(ctx, a, b) ((a) < (b))
#    define RC_SORT_CMP_(a, b)     ((void)ctx, (a) < (b))
#  else
#    define RC_SORT_CMP_(a, b)     RC_SORT_CMP(ctx, a, b)
#  endif
#  define RC_SORT_SPAN_CTX_PARAM_   RC_SORT_SPAN span, RC_SORT_CTX *ctx
#  define RC_SORT_SPAN_CTX_ARG_(s)  s, ctx
#else
#  ifndef RC_SORT_CMP
#    define RC_SORT_CMP(a, b) ((a) < (b))
#  endif
#  define RC_SORT_CMP_(a, b)       RC_SORT_CMP(a, b)
#  define RC_SORT_SPAN_CTX_PARAM_   RC_SORT_SPAN span
#  define RC_SORT_SPAN_CTX_ARG_(s)  s
#endif

/*
 * Private helper names - derived from RC_SORT_NAME so they stay unique when the
 * header is instantiated for several types in the same translation unit.
 */
#define RC_SORT_SWAP_      RC_CONCAT(RC_SORT_NAME, _swap_)
#define RC_SORT_ISORT_     RC_CONCAT(RC_SORT_NAME, _isort_)
#define RC_SORT_SIFTDOWN_  RC_CONCAT(RC_SORT_NAME, _siftdown_)
#define RC_SORT_HEAPSORT_  RC_CONCAT(RC_SORT_NAME, _heapsort_)
#define RC_SORT_INTRO_     RC_CONCAT(RC_SORT_NAME, _intro_)

/* Subspan operations from the array template, named after the span type. */
#define RC_SORT_GET_HEAD_  RC_CONCAT(RC_SORT_SPAN, _get_head)
#define RC_SORT_GET_TAIL_  RC_CONCAT(RC_SORT_SPAN, _get_tail)

/* Spans of this size or smaller are sorted with insertion sort. */
#define RC_SORT_THRESHOLD_ 16

/* RC_SORT_SWAP_ does not use the comparator and needs no ctx parameter. */
static inline void RC_SORT_SWAP_(RC_SORT_TYPE *a, RC_SORT_TYPE *b)
{
    RC_SORT_TYPE tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Insertion sort on the span. */
static inline void RC_SORT_ISORT_(RC_SORT_SPAN_CTX_PARAM_)
{
    for (uint32_t i = 1; i < span.num; i++) {
        RC_SORT_TYPE key = span.data[i];
        uint32_t     j   = i;
        while (j > 0 && RC_SORT_CMP_(key, span.data[j - 1])) {
            span.data[j] = span.data[j - 1];
            j--;
        }
        span.data[j] = key;
    }
}

/* Sift down the element at index i in a max-heap occupying the whole span. */
static inline void RC_SORT_SIFTDOWN_(RC_SORT_SPAN_CTX_PARAM_, uint32_t i)
{
    for (;;) {
        uint32_t largest = i;
        uint32_t left    = 2 * i + 1;
        uint32_t right   = 2 * i + 2;
        if (left  < span.num && RC_SORT_CMP_(span.data[largest], span.data[left]))  largest = left;
        if (right < span.num && RC_SORT_CMP_(span.data[largest], span.data[right])) largest = right;
        if (largest == i) break;
        RC_SORT_SWAP_(&span.data[i], &span.data[largest]);
        i = largest;
    }
}

/* Heapsort on the span. */
static inline void RC_SORT_HEAPSORT_(RC_SORT_SPAN_CTX_PARAM_)
{
    for (uint32_t i = span.num / 2; i-- > 0; ) {
        RC_SORT_SIFTDOWN_(RC_SORT_SPAN_CTX_ARG_(span), i);
    }
    for (uint32_t i = span.num - 1; i > 0; i--) {
        RC_SORT_SWAP_(&span.data[0], &span.data[i]);
        // shrink the heap to the unsorted prefix [0, i)
        RC_SORT_SIFTDOWN_(RC_SORT_SPAN_CTX_ARG_(RC_SORT_GET_HEAD_(span, i)), 0);
    }
}

/* Introsort on the span, with depth quicksort levels remaining. */
static inline void RC_SORT_INTRO_(RC_SORT_SPAN_CTX_PARAM_, uint32_t depth)
{
    while (span.num > RC_SORT_THRESHOLD_) {
        if (depth == 0) {
            RC_SORT_HEAPSORT_(RC_SORT_SPAN_CTX_ARG_(span));
            return;
        }
        --depth;

        uint32_t n = span.num;

        // Median-of-three: apply a sorting network to span.data[0],
        // span.data[mid], span.data[n-1].  Afterwards those three are in order.
        //
        // They now serve a second role as sentinels:
        //   span.data[0]   <= pivot  ->  the downward scan stops before index 0.
        //   span.data[n-1] >= pivot  ->  the upward scan stops before index n-1.
        uint32_t mid = n / 2;
        if (RC_SORT_CMP_(span.data[mid],   span.data[0]))   RC_SORT_SWAP_(&span.data[0],   &span.data[mid]);
        if (RC_SORT_CMP_(span.data[n - 1], span.data[0]))   RC_SORT_SWAP_(&span.data[0],   &span.data[n - 1]);
        if (RC_SORT_CMP_(span.data[n - 1], span.data[mid])) RC_SORT_SWAP_(&span.data[mid], &span.data[n - 1]);

        // Move the pivot (the median) to span.data[n-2], just inside the right
        // sentinel, so it is excluded from the scan range.
        RC_SORT_SWAP_(&span.data[mid], &span.data[n - 2]);
        RC_SORT_TYPE pivot = span.data[n - 2];

        // Hoare-style partition of span.data[1..n-3].  The sentinels at
        // span.data[0] and span.data[n-1] guarantee the scans halt without
        // explicit bounds checks.
        uint32_t lo = 1;
        uint32_t hi = n - 3;
        for (;;) {
            while (RC_SORT_CMP_(span.data[lo], pivot)) ++lo;
            while (RC_SORT_CMP_(pivot, span.data[hi])) --hi;
            if (lo >= hi) break;
            RC_SORT_SWAP_(&span.data[lo], &span.data[hi]);
            ++lo;
            --hi;
        }

        // Place the pivot at its final sorted position.
        RC_SORT_SWAP_(&span.data[lo], &span.data[n - 2]);
        uint32_t p = lo;

        // Recurse on the smaller partition and loop on the larger.  This keeps
        // the call stack depth to O(log n) regardless of how poorly the pivot
        // divides the array.
        RC_SORT_SPAN left  = RC_SORT_GET_HEAD_(span, p);
        RC_SORT_SPAN right = RC_SORT_GET_TAIL_(span, p + 1);
        if (left.num < right.num) {
            RC_SORT_INTRO_(RC_SORT_SPAN_CTX_ARG_(left), depth);
            span = right;
        }
        else {
            RC_SORT_INTRO_(RC_SORT_SPAN_CTX_ARG_(right), depth);
            span = left;
        }
    }
    RC_SORT_ISORT_(RC_SORT_SPAN_CTX_ARG_(span));
}

/* Public sort function. */
static inline void RC_SORT_NAME(RC_SORT_SPAN_CTX_PARAM_)
{
    if (span.num < 2) return;
    // Depth limit: 2 * floor(log2(n)), matching libstdc++ / libc++.
    uint32_t depth = 0;
    for (uint32_t n = span.num; n > 1; n >>= 1) {
        depth += 2;
    }
    RC_SORT_INTRO_(RC_SORT_SPAN_CTX_ARG_(span), depth);
}

/* ---- cleanup ---- */

#undef RC_SORT_THRESHOLD_
#undef RC_SORT_SWAP_
#undef RC_SORT_ISORT_
#undef RC_SORT_SIFTDOWN_
#undef RC_SORT_HEAPSORT_
#undef RC_SORT_INTRO_
#undef RC_SORT_GET_HEAD_
#undef RC_SORT_GET_TAIL_

#undef RC_SORT_CMP_
#undef RC_SORT_SPAN_CTX_PARAM_
#undef RC_SORT_SPAN_CTX_ARG_
#undef RC_SORT_CTX
#undef RC_SORT_CMP
#undef RC_SORT_SPAN
#undef RC_SORT_NAME
#undef RC_SORT_TYPE
