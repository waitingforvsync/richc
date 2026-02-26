/*
 * upper_bound.h - template header: binary search on a sorted view.
 *
 * Finds the first element e in view for which !(e <= value),
 * i.e. the first element strictly greater than value (assuming the
 * view is sorted ascending under the comparison).  Returns the index
 * of that element, or view.num if every element satisfies e <= value.
 *
 * Uses the same CMP(a, b) comparator convention as lower_bound.h
 * (true iff a < b).  Internally, "element <= value" is tested as
 * "!(value < element)", i.e. !CMP(value, element) — no separate <=
 * comparator is needed.
 *
 * Define before including:
 *   UPPER_BOUND_T          element type (required)
 *   UPPER_BOUND_CTX        context type passed to the comparator (optional)
 *   UPPER_BOUND_CMP        comparator expression (optional; see below)
 *   UPPER_BOUND_VIEW       view type for UPPER_BOUND_T
 *                          (optional; default: rc_view_##UPPER_BOUND_T)
 *   UPPER_BOUND_NAME       function name
 *                          (optional; default: rc_upper_bound_##UPPER_BOUND_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * UPPER_BOUND_CMP conventions
 * ---------------------------
 * Without UPPER_BOUND_CTX:
 *   UPPER_BOUND_CMP(a, b)         — true iff a < b
 *   Default: (a) < (b)
 *
 * With UPPER_BOUND_CTX:
 *   UPPER_BOUND_CMP(ctx, a, b)    — true iff a < b given context ctx
 *   ctx is a pointer to UPPER_BOUND_CTX and is the first argument.
 *   Default: ignores ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view, T value)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx, T value)
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef UPPER_BOUND_T
#  error "UPPER_BOUND_T must be defined before including upper_bound.h"
#endif

#ifndef UPPER_BOUND_VIEW
#  define UPPER_BOUND_VIEW RC_CONCAT(rc_view_, UPPER_BOUND_T)
#endif

#ifndef UPPER_BOUND_NAME
#  define UPPER_BOUND_NAME RC_CONCAT(rc_upper_bound_, UPPER_BOUND_T)
#endif

/*
 * Comparator and optional context.
 *
 * UPPER_BOUND_CMP_ is the internal two-argument macro used at every call
 * site within the generated function body.  It closes over 'ctx' from
 * the function scope when a context type is active.
 *
 * When CTX is defined but CMP is not, the default comparator ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef UPPER_BOUND_CTX
#  ifndef UPPER_BOUND_CMP
#    define UPPER_BOUND_CMP(ctx, a, b) ((a) < (b))
#    define UPPER_BOUND_CMP_(a, b)  ((void)ctx, (a) < (b))
#  else
#    define UPPER_BOUND_CMP_(a, b)  UPPER_BOUND_CMP(ctx, a, b)
#  endif
#  define UPPER_BOUND_CTX_PARAM_    , UPPER_BOUND_CTX *ctx
#else
#  ifndef UPPER_BOUND_CMP
#    define UPPER_BOUND_CMP(a, b) ((a) < (b))
#  endif
#  define UPPER_BOUND_CMP_(a, b)    UPPER_BOUND_CMP(a, b)
#  define UPPER_BOUND_CTX_PARAM_
#endif

static inline uint32_t
UPPER_BOUND_NAME(UPPER_BOUND_VIEW view UPPER_BOUND_CTX_PARAM_, UPPER_BOUND_T value)
{
    uint32_t lo = 0;
    uint32_t hi = view.num;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (!UPPER_BOUND_CMP_(value, view.data[mid])) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* ---- cleanup ---- */

#undef UPPER_BOUND_CMP_
#undef UPPER_BOUND_CTX_PARAM_
#undef UPPER_BOUND_CTX
#undef UPPER_BOUND_CMP
#undef UPPER_BOUND_VIEW
#undef UPPER_BOUND_NAME
#undef UPPER_BOUND_T
