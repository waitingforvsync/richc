/*
 * lower_bound.h - template header: binary search on a sorted view.
 *
 * Finds the first element e in view for which !(e < value),
 * i.e. the first element >= value (assuming the view is sorted
 * ascending under the comparison).  Returns the index of that element,
 * or view.num if every element satisfies e < value.
 *
 * Define before including:
 *   LOWER_BOUND_T          element type (required)
 *   LOWER_BOUND_CTX        context type passed to the comparator (optional)
 *   LOWER_BOUND_CMP        comparator expression (optional; see below)
 *   LOWER_BOUND_VIEW       view type for LOWER_BOUND_T
 *                          (optional; default: rc_view_##LOWER_BOUND_T)
 *   LOWER_BOUND_NAME       function name
 *                          (optional; default: rc_lower_bound_##LOWER_BOUND_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * LOWER_BOUND_CMP conventions
 * ---------------------------
 * Without LOWER_BOUND_CTX:
 *   LOWER_BOUND_CMP(a, b)         — true iff a < b
 *   Default: (a) < (b)
 *
 * With LOWER_BOUND_CTX:
 *   LOWER_BOUND_CMP(ctx, a, b)    — true iff a < b given context ctx
 *   ctx is a pointer to LOWER_BOUND_CTX and is the first argument.
 *   Default: ignores ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view, T value)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx, T value)
 *
 * Example (no context):
 *   #define LOWER_BOUND_T int
 *   #include "richc/lower_bound.h"
 *   // defines: uint32_t rc_lower_bound_int(rc_view_int view, int value);
 *
 * Example (context comparator):
 *   typedef struct { int field_offset; } RecordCmpCtx;
 *   #define LOWER_BOUND_T           Record
 *   #define LOWER_BOUND_CTX         RecordCmpCtx
 *   #define LOWER_BOUND_CMP(ctx, a, b)  (field(ctx, a) < field(ctx, b))
 *   #define LOWER_BOUND_NAME        Record_lower_bound_ctx
 *   #include "richc/lower_bound.h"
 *   // defines: uint32_t Record_lower_bound_ctx(rc_view_Record, RecordCmpCtx *, Record);
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef LOWER_BOUND_T
#  error "LOWER_BOUND_T must be defined before including lower_bound.h"
#endif

#ifndef LOWER_BOUND_VIEW
#  define LOWER_BOUND_VIEW RC_CONCAT(rc_view_, LOWER_BOUND_T)
#endif

#ifndef LOWER_BOUND_NAME
#  define LOWER_BOUND_NAME RC_CONCAT(rc_lower_bound_, LOWER_BOUND_T)
#endif

/*
 * Comparator and optional context.
 *
 * LOWER_BOUND_CMP_ is the internal two-argument macro used at every call
 * site within the generated function body.  It closes over 'ctx' from
 * the function scope when a context type is active.
 *
 * When CTX is defined but CMP is not, the default comparator ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef LOWER_BOUND_CTX
#  ifndef LOWER_BOUND_CMP
#    define LOWER_BOUND_CMP(ctx, a, b) ((a) < (b))
#    define LOWER_BOUND_CMP_(a, b)  ((void)ctx, (a) < (b))
#  else
#    define LOWER_BOUND_CMP_(a, b)  LOWER_BOUND_CMP(ctx, a, b)
#  endif
#  define LOWER_BOUND_CTX_PARAM_    , LOWER_BOUND_CTX *ctx
#else
#  ifndef LOWER_BOUND_CMP
#    define LOWER_BOUND_CMP(a, b) ((a) < (b))
#  endif
#  define LOWER_BOUND_CMP_(a, b)    LOWER_BOUND_CMP(a, b)
#  define LOWER_BOUND_CTX_PARAM_
#endif

static inline uint32_t
LOWER_BOUND_NAME(LOWER_BOUND_VIEW view LOWER_BOUND_CTX_PARAM_, LOWER_BOUND_T value)
{
    uint32_t lo = 0;
    uint32_t hi = view.num;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (LOWER_BOUND_CMP_(view.data[mid], value)) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* ---- cleanup ---- */

#undef LOWER_BOUND_CMP_
#undef LOWER_BOUND_CTX_PARAM_
#undef LOWER_BOUND_CTX
#undef LOWER_BOUND_CMP
#undef LOWER_BOUND_VIEW
#undef LOWER_BOUND_NAME
#undef LOWER_BOUND_T
