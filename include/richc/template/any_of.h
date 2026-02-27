/*
 * any_of.h - template header: test whether any element of a view satisfies a predicate.
 *
 * Scans the view and returns true if at least one element satisfies ANY_OF_PRED.
 * Returns false for an empty view.
 * Short-circuits on the first matching element.
 *
 * Define before including:
 *   ANY_OF_T       element type (required)
 *   ANY_OF_PRED    unary predicate (required; see below)
 *   ANY_OF_CTX     context type passed to the predicate (optional)
 *   ANY_OF_VIEW    view type for ANY_OF_T
 *                  (optional; default: rc_view_##ANY_OF_T)
 *   ANY_OF_NAME    function name
 *                  (optional; default: rc_any_of_##ANY_OF_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * ANY_OF_PRED conventions
 * -----------------------
 * Without ANY_OF_CTX:
 *   ANY_OF_PRED(element)        — true iff element satisfies the predicate
 *
 * With ANY_OF_CTX:
 *   ANY_OF_PRED(ctx, element)   — true iff element satisfies the predicate given ctx
 *   ctx is a pointer to ANY_OF_CTX and is the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  bool NAME(VIEW view)
 * With context:     bool NAME(VIEW view, CTX *ctx)
 *
 * Return value
 * ------------
 * true  as soon as any element satisfies the predicate.
 * false if no element satisfies the predicate, or the view is empty.
 *
 * Example (no context):
 *   #define ANY_OF_T            int
 *   #define ANY_OF_PRED(e)      ((e) < 0)
 *   #define ANY_OF_NAME         int_any_negative
 *   #include "richc/template/any_of.h"
 *   // defines: bool int_any_negative(rc_view_int view)
 *
 * Example (context):
 *   typedef struct { int threshold; } ThreshCtx;
 *   #define ANY_OF_T               int
 *   #define ANY_OF_CTX             ThreshCtx
 *   #define ANY_OF_PRED(ctx, e)    ((e) >= (ctx)->threshold)
 *   #define ANY_OF_NAME            int_any_at_least
 *   #include "richc/template/any_of.h"
 *   // defines: bool int_any_at_least(rc_view_int view, ThreshCtx *ctx)
 */

#include <stdbool.h>
#include <stdint.h>
#include "richc/template_util.h"

#ifndef ANY_OF_T
#  error "ANY_OF_T must be defined before including any_of.h"
#endif
#ifndef ANY_OF_PRED
#  error "ANY_OF_PRED must be defined before including any_of.h"
#endif

#ifndef ANY_OF_VIEW
#  define ANY_OF_VIEW RC_CONCAT(rc_view_, ANY_OF_T)
#endif

#ifndef ANY_OF_NAME
#  define ANY_OF_NAME RC_CONCAT(rc_any_of_, ANY_OF_T)
#endif

#ifdef ANY_OF_CTX
#  define ANY_OF_PRED_(element)  ANY_OF_PRED(ctx, element)
#  define ANY_OF_CTX_PARAM_      , ANY_OF_CTX *ctx
#else
#  define ANY_OF_PRED_(element)  ANY_OF_PRED(element)
#  define ANY_OF_CTX_PARAM_
#endif

static inline bool
ANY_OF_NAME(ANY_OF_VIEW view ANY_OF_CTX_PARAM_)
{
    for (uint32_t i = 0; i < view.num; i++)
        if (ANY_OF_PRED_(view.data[i]))
            return true;
    return false;
}

/* ---- cleanup ---- */

#undef ANY_OF_PRED_
#undef ANY_OF_CTX_PARAM_
#undef ANY_OF_CTX
#undef ANY_OF_PRED
#undef ANY_OF_VIEW
#undef ANY_OF_NAME
#undef ANY_OF_T
