/*
 * sort.h - template header: introsort on a mutable span.
 *
 * Uses quicksort (median-of-three pivot) for large spans, falling back
 * to heapsort when recursion depth exceeds 2*floor(log2(n)) to guarantee
 * O(n log n) worst-case, and insertion sort for small spans (n <= 16).
 * This matches the strategy used by libstdc++ and libc++.
 *
 * The median-of-three step sorts data[0], data[mid], and data[n-1] in
 * place before partitioning.  This has two benefits: it picks a better
 * pivot on average, and it leaves data[0] <= pivot and data[n-1] >= pivot
 * as sentinels so the inner partition scans need no bounds checks.
 *
 * Define before including:
 *   SORT_T          element type (required)
 *   SORT_CTX        context type passed to the comparator (optional)
 *   SORT_CMP        comparator expression (optional; see below)
 *   SORT_SPAN       span type for SORT_T
 *                   (optional; default: rc_span_##SORT_T)
 *   SORT_NAME       function name
 *                   (optional; default: rc_sort_##SORT_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * SORT_CMP conventions
 * --------------------
 * Without SORT_CTX:
 *   SORT_CMP(a, b)            — true iff a < b
 *   Default: (a) < (b)
 *
 * With SORT_CTX:
 *   SORT_CMP(ctx, a, b)       — true iff a < b given context ctx
 *   ctx is a pointer to SORT_CTX and is the first argument.
 *   Default: ignores ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  void NAME(SPAN span)
 * With context:     void NAME(SPAN span, CTX *ctx)
 *
 * Example (no context):
 *   #define SORT_T int
 *   #include "richc/sort.h"
 *   // defines: void rc_sort_int(rc_span_int span);
 *
 * Example (context comparator):
 *   #define SORT_T    Widget
 *   #define SORT_CTX  WidgetCmpCtx
 *   #define SORT_CMP(ctx, a, b)  widget_less(ctx, a, b)
 *   #define SORT_NAME widget_sort
 *   #include "richc/sort.h"
 *   // defines: void widget_sort(rc_span_Widget span, WidgetCmpCtx *ctx);
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef SORT_T
#  error "SORT_T must be defined before including sort.h"
#endif

#ifndef SORT_SPAN
#  define SORT_SPAN RC_CONCAT(rc_span_, SORT_T)
#endif

#ifndef SORT_NAME
#  define SORT_NAME RC_CONCAT(rc_sort_, SORT_T)
#endif

/*
 * Comparator and optional context.
 *
 * SORT_CMP_ is the internal two-argument macro used at every comparator
 * call site within the generated functions.  It closes over 'ctx' from
 * the enclosing function scope when a context type is active.
 *
 * SORT_CTX_PARAM_ — trailing parameter declaration (, CTX *ctx or empty)
 * SORT_CTX_ARG_   — trailing call argument      (, ctx or empty)
 *
 * Both are appended at the end of each internal helper's parameter list
 * and each call site, so ctx flows through the entire call chain without
 * any change to the algorithm logic.
 *
 * When CTX is defined but CMP is not, the default comparator ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside a function body.
 */
#ifdef SORT_CTX
#  ifndef SORT_CMP
#    define SORT_CMP(ctx, a, b) ((a) < (b))
#    define SORT_CMP_(a, b)  ((void)ctx, (a) < (b))
#  else
#    define SORT_CMP_(a, b)  SORT_CMP(ctx, a, b)
#  endif
#  define SORT_CTX_PARAM_    , SORT_CTX *ctx
#  define SORT_CTX_ARG_      , ctx
#else
#  ifndef SORT_CMP
#    define SORT_CMP(a, b) ((a) < (b))
#  endif
#  define SORT_CMP_(a, b)    SORT_CMP(a, b)
#  define SORT_CTX_PARAM_
#  define SORT_CTX_ARG_
#endif

/*
 * Private helper names — derived from SORT_NAME so they remain unique
 * when the header is instantiated for multiple types in the same TU.
 */
#define SORT_SWAP_      RC_CONCAT(SORT_NAME, _swap_)
#define SORT_ISORT_     RC_CONCAT(SORT_NAME, _isort_)
#define SORT_SIFTDOWN_  RC_CONCAT(SORT_NAME, _siftdown_)
#define SORT_HEAPSORT_  RC_CONCAT(SORT_NAME, _heapsort_)
#define SORT_INTRO_     RC_CONCAT(SORT_NAME, _intro_)

/* Spans of this size or smaller are sorted with insertion sort. */
#define SORT_THRESHOLD_ 16

/* SORT_SWAP_ does not use the comparator and needs no ctx parameter. */
static inline void SORT_SWAP_(SORT_T *a, SORT_T *b)
{
    SORT_T tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Insertion sort on data[0..n). */
static inline void SORT_ISORT_(SORT_T *data, uint32_t n SORT_CTX_PARAM_)
{
    for (uint32_t i = 1; i < n; i++) {
        SORT_T   key = data[i];
        uint32_t j   = i;
        while (j > 0 && SORT_CMP_(key, data[j - 1])) {
            data[j] = data[j - 1];
            j--;
        }
        data[j] = key;
    }
}

/* Sift down element at index i in a max-heap of size n. */
static inline void SORT_SIFTDOWN_(SORT_T *data, uint32_t i, uint32_t n SORT_CTX_PARAM_)
{
    for (;;) {
        uint32_t largest = i;
        uint32_t left    = 2 * i + 1;
        uint32_t right   = 2 * i + 2;
        if (left  < n && SORT_CMP_(data[largest], data[left]))  largest = left;
        if (right < n && SORT_CMP_(data[largest], data[right])) largest = right;
        if (largest == i) break;
        SORT_SWAP_(&data[i], &data[largest]);
        i = largest;
    }
}

/* Heapsort on data[0..n). */
static inline void SORT_HEAPSORT_(SORT_T *data, uint32_t n SORT_CTX_PARAM_)
{
    for (uint32_t i = n / 2; i-- > 0; )
        SORT_SIFTDOWN_(data, i, n SORT_CTX_ARG_);
    for (uint32_t i = n - 1; i > 0; i--) {
        SORT_SWAP_(&data[0], &data[i]);
        SORT_SIFTDOWN_(data, 0, i SORT_CTX_ARG_);
    }
}

/* Introsort on data[0..n), with depth quicksort levels remaining. */
static inline void SORT_INTRO_(SORT_T *data, uint32_t n, uint32_t depth SORT_CTX_PARAM_)
{
    while (n > SORT_THRESHOLD_) {
        if (depth == 0) {
            SORT_HEAPSORT_(data, n SORT_CTX_ARG_);
            return;
        }
        --depth;

        /*
         * Median-of-three: apply a sorting network to data[0], data[mid],
         * data[n-1].  Afterwards data[0] <= data[mid] <= data[n-1].
         *
         * These three elements are now in their correct relative order and
         * serve a second role as sentinels:
         *   data[0]   <= pivot  →  the downward scan stops before index 0.
         *   data[n-1] >= pivot  →  the upward scan stops before index n-1.
         */
        uint32_t mid = n / 2;
        if (SORT_CMP_(data[mid],   data[0]))     SORT_SWAP_(&data[0],   &data[mid]);
        if (SORT_CMP_(data[n - 1], data[0]))     SORT_SWAP_(&data[0],   &data[n - 1]);
        if (SORT_CMP_(data[n - 1], data[mid]))   SORT_SWAP_(&data[mid], &data[n - 1]);

        /* Move the pivot (the median) to data[n-2], just inside the right
         * sentinel, so it is excluded from the scan range. */
        SORT_SWAP_(&data[mid], &data[n - 2]);
        SORT_T pivot = data[n - 2];

        /* Hoare-style partition of data[1..n-3].
         * The sentinels at data[0] and data[n-1] guarantee the scans
         * halt without explicit bounds checks. */
        uint32_t lo = 1;
        uint32_t hi = n - 3;
        for (;;) {
            while (SORT_CMP_(data[lo], pivot)) ++lo;
            while (SORT_CMP_(pivot, data[hi])) --hi;
            if (lo >= hi) break;
            SORT_SWAP_(&data[lo], &data[hi]);
            ++lo;
            --hi;
        }

        /* Place the pivot at its final sorted position. */
        SORT_SWAP_(&data[lo], &data[n - 2]);
        uint32_t p = lo;

        /*
         * Recurse on the smaller partition and loop on the larger.
         * This keeps the call stack depth to O(log n) regardless of
         * how poorly the pivot divides the array.
         */
        if (p < n - 1 - p) {
            SORT_INTRO_(data, p, depth SORT_CTX_ARG_);
            data += p + 1;
            n   -= p + 1;
        } else {
            SORT_INTRO_(data + p + 1, n - p - 1, depth SORT_CTX_ARG_);
            n = p;
        }
    }
    SORT_ISORT_(data, n SORT_CTX_ARG_);
}

/* Public sort function. */
static inline void SORT_NAME(SORT_SPAN span SORT_CTX_PARAM_)
{
    if (span.num < 2) return;
    /* Depth limit: 2 * floor(log2(n)), matching libstdc++ / libc++. */
    uint32_t depth = 0;
    for (uint32_t n = span.num; n > 1; n >>= 1)
        depth += 2;
    SORT_INTRO_(span.data, span.num, depth SORT_CTX_ARG_);
}

/* ---- cleanup ---- */

#undef SORT_THRESHOLD_
#undef SORT_SWAP_
#undef SORT_ISORT_
#undef SORT_SIFTDOWN_
#undef SORT_HEAPSORT_
#undef SORT_INTRO_

#undef SORT_CMP_
#undef SORT_CTX_PARAM_
#undef SORT_CTX_ARG_
#undef SORT_CTX
#undef SORT_CMP
#undef SORT_SPAN
#undef SORT_NAME
#undef SORT_T
