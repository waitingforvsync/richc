/*
 * all_of.h - template header: test whether all elements of a view satisfy a predicate.
 *
 * Scans the view and returns true if every element satisfies ALL_OF_PRED.
 * Returns true for an empty view (vacuous truth).
 * Short-circuits on the first non-matching element.
 *
 * Define before including:
 *   ALL_OF_T       element type (required)
 *   ALL_OF_PRED    unary predicate (required; see below)
 *   ALL_OF_CTX     context type passed to the predicate (optional)
 *   ALL_OF_VIEW    view type for ALL_OF_T
 *                  (optional; default: rc_view_##ALL_OF_T)
 *   ALL_OF_NAME    function name
 *                  (optional; default: rc_all_of_##ALL_OF_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * ALL_OF_PRED conventions
 * -----------------------
 * Without ALL_OF_CTX:
 *   ALL_OF_PRED(element)        — true iff element satisfies the predicate
 *
 * With ALL_OF_CTX:
 *   ALL_OF_PRED(ctx, element)   — true iff element satisfies the predicate given ctx
 *   ctx is a pointer to ALL_OF_CTX and is the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  bool NAME(VIEW view)
 * With context:     bool NAME(VIEW view, CTX *ctx)
 *
 * Return value
 * ------------
 * true  if every element satisfies the predicate, or the view is empty.
 * false as soon as any element does not satisfy the predicate.
 *
 * Example (no context):
 *   #define ALL_OF_T            int
 *   #define ALL_OF_PRED(e)      ((e) > 0)
 *   #define ALL_OF_NAME         int_all_positive
 *   #include "richc/template/all_of.h"
 *   // defines: bool int_all_positive(rc_view_int view)
 *
 * Example (context):
 *   typedef struct { int threshold; } ThreshCtx;
 *   #define ALL_OF_T               int
 *   #define ALL_OF_CTX             ThreshCtx
 *   #define ALL_OF_PRED(ctx, e)    ((e) >= (ctx)->threshold)
 *   #define ALL_OF_NAME            int_all_at_least
 *   #include "richc/template/all_of.h"
 *   // defines: bool int_all_at_least(rc_view_int view, ThreshCtx *ctx)
 */

#include <stdbool.h>
#include <stdint.h>
#include "richc/template_util.h"

#ifndef ALL_OF_T
#  error "ALL_OF_T must be defined before including all_of.h"
#endif
#ifndef ALL_OF_PRED
#  error "ALL_OF_PRED must be defined before including all_of.h"
#endif

#ifndef ALL_OF_VIEW
#  define ALL_OF_VIEW RC_CONCAT(rc_view_, ALL_OF_T)
#endif

#ifndef ALL_OF_NAME
#  define ALL_OF_NAME RC_CONCAT(rc_all_of_, ALL_OF_T)
#endif

#ifdef ALL_OF_CTX
#  define ALL_OF_PRED_(element)  ALL_OF_PRED(ctx, element)
#  define ALL_OF_CTX_PARAM_      , ALL_OF_CTX *ctx
#else
#  define ALL_OF_PRED_(element)  ALL_OF_PRED(element)
#  define ALL_OF_CTX_PARAM_
#endif

static inline bool
ALL_OF_NAME(ALL_OF_VIEW view ALL_OF_CTX_PARAM_)
{
    for (uint32_t i = 0; i < view.num; i++)
        if (!ALL_OF_PRED_(view.data[i]))
            return false;
    return true;
}

/* ---- cleanup ---- */

#undef ALL_OF_PRED_
#undef ALL_OF_CTX_PARAM_
#undef ALL_OF_CTX
#undef ALL_OF_PRED
#undef ALL_OF_VIEW
#undef ALL_OF_NAME
#undef ALL_OF_T
