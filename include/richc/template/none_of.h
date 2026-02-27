/*
 * none_of.h - template header: test whether no element of a view satisfies a predicate.
 *
 * Scans the view and returns true if no element satisfies NONE_OF_PRED.
 * Returns true for an empty view (vacuous truth).
 * Short-circuits on the first matching element.
 *
 * Define before including:
 *   NONE_OF_T       element type (required)
 *   NONE_OF_PRED    unary predicate (required; see below)
 *   NONE_OF_CTX     context type passed to the predicate (optional)
 *   NONE_OF_VIEW    view type for NONE_OF_T
 *                   (optional; default: rc_view_##NONE_OF_T)
 *   NONE_OF_NAME    function name
 *                   (optional; default: rc_none_of_##NONE_OF_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * NONE_OF_PRED conventions
 * ------------------------
 * Without NONE_OF_CTX:
 *   NONE_OF_PRED(element)        — true iff element satisfies the predicate
 *
 * With NONE_OF_CTX:
 *   NONE_OF_PRED(ctx, element)   — true iff element satisfies the predicate given ctx
 *   ctx is a pointer to NONE_OF_CTX and is the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  bool NAME(VIEW view)
 * With context:     bool NAME(VIEW view, CTX *ctx)
 *
 * Return value
 * ------------
 * true  if no element satisfies the predicate, or the view is empty.
 * false as soon as any element satisfies the predicate.
 *
 * Example (no context):
 *   #define NONE_OF_T            int
 *   #define NONE_OF_PRED(e)      ((e) < 0)
 *   #define NONE_OF_NAME         int_none_negative
 *   #include "richc/template/none_of.h"
 *   // defines: bool int_none_negative(rc_view_int view)
 *
 * Example (context):
 *   typedef struct { int threshold; } ThreshCtx;
 *   #define NONE_OF_T               int
 *   #define NONE_OF_CTX             ThreshCtx
 *   #define NONE_OF_PRED(ctx, e)    ((e) >= (ctx)->threshold)
 *   #define NONE_OF_NAME            int_none_at_least
 *   #include "richc/template/none_of.h"
 *   // defines: bool int_none_at_least(rc_view_int view, ThreshCtx *ctx)
 */

#include <stdbool.h>
#include <stdint.h>
#include "richc/template_util.h"

#ifndef NONE_OF_T
#  error "NONE_OF_T must be defined before including none_of.h"
#endif
#ifndef NONE_OF_PRED
#  error "NONE_OF_PRED must be defined before including none_of.h"
#endif

#ifndef NONE_OF_VIEW
#  define NONE_OF_VIEW RC_CONCAT(rc_view_, NONE_OF_T)
#endif

#ifndef NONE_OF_NAME
#  define NONE_OF_NAME RC_CONCAT(rc_none_of_, NONE_OF_T)
#endif

#ifdef NONE_OF_CTX
#  define NONE_OF_PRED_(element)  NONE_OF_PRED(ctx, element)
#  define NONE_OF_CTX_PARAM_      , NONE_OF_CTX *ctx
#else
#  define NONE_OF_PRED_(element)  NONE_OF_PRED(element)
#  define NONE_OF_CTX_PARAM_
#endif

static inline bool
NONE_OF_NAME(NONE_OF_VIEW view NONE_OF_CTX_PARAM_)
{
    for (uint32_t i = 0; i < view.num; i++)
        if (NONE_OF_PRED_(view.data[i]))
            return false;
    return true;
}

/* ---- cleanup ---- */

#undef NONE_OF_PRED_
#undef NONE_OF_CTX_PARAM_
#undef NONE_OF_CTX
#undef NONE_OF_PRED
#undef NONE_OF_VIEW
#undef NONE_OF_NAME
#undef NONE_OF_T
