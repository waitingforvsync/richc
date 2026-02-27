/*
 * remove.h - template header: in-place removal from a span.
 *
 * Applies REMOVE_PRED to each element of the span.  Elements for which
 * the predicate is true are removed; the remaining elements are shifted
 * left to fill the gaps, preserving their relative order.  span->num is
 * updated to the new count and the number of removed items is returned.
 *
 * The operation is O(n) and performs at most n writes.  No arena is needed.
 *
 * Define before including:
 *   REMOVE_T       element type (required)
 *   REMOVE_PRED    remove predicate expression (required; see below)
 *   REMOVE_CTX     context type passed to the predicate (optional)
 *   REMOVE_SPAN    span type for REMOVE_T
 *                  (optional; default: rc_span_##REMOVE_T)
 *   REMOVE_NAME    function name
 *                  (optional; default: rc_remove_##REMOVE_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * REMOVE_PRED conventions
 * -----------------------
 * Without REMOVE_CTX:
 *   REMOVE_PRED(element)        — true iff element should be removed
 *
 * With REMOVE_CTX:
 *   REMOVE_PRED(ctx, element)   — true iff element should be removed given ctx
 *   ctx is a pointer to REMOVE_CTX and is the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(SPAN *span)
 * With context:     uint32_t NAME(SPAN *span, CTX *ctx)
 *
 * Return value
 * ------------
 * The number of elements removed.  span->num is updated in place.
 *
 * Example (no context):
 *   #define REMOVE_T            int
 *   #define REMOVE_PRED(e)      ((e) < 0)
 *   #define REMOVE_NAME         int_remove_negative
 *   #include "richc/template/remove.h"
 *   // defines: uint32_t int_remove_negative(rc_span_int *span)
 *
 * Example (context):
 *   typedef struct { int threshold; } ThreshCtx;
 *   #define REMOVE_T               int
 *   #define REMOVE_CTX             ThreshCtx
 *   #define REMOVE_PRED(ctx, e)    ((e) < (ctx)->threshold)
 *   #define REMOVE_NAME            int_remove_below
 *   #include "richc/template/remove.h"
 *   // defines: uint32_t int_remove_below(rc_span_int *span, ThreshCtx *ctx)
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef REMOVE_T
#  error "REMOVE_T must be defined before including remove.h"
#endif
#ifndef REMOVE_PRED
#  error "REMOVE_PRED must be defined before including remove.h"
#endif

#ifndef REMOVE_SPAN
#  define REMOVE_SPAN RC_CONCAT(rc_span_, REMOVE_T)
#endif

#ifndef REMOVE_NAME
#  define REMOVE_NAME RC_CONCAT(rc_remove_, REMOVE_T)
#endif

/*
 * Predicate and optional context.
 *
 * REMOVE_PRED_ is the internal single-argument macro used at the call
 * site inside the generated function body.  It closes over 'ctx' from
 * the function scope when a context type is active.
 */
#ifdef REMOVE_CTX
#  define REMOVE_PRED_(element)  REMOVE_PRED(ctx, element)
#  define REMOVE_CTX_PARAM_      , REMOVE_CTX *ctx
#else
#  define REMOVE_PRED_(element)  REMOVE_PRED(element)
#  define REMOVE_CTX_PARAM_
#endif

static inline uint32_t
REMOVE_NAME(REMOVE_SPAN *span REMOVE_CTX_PARAM_)
{
    uint32_t write = 0;
    for (uint32_t i = 0; i < span->num; i++) {
        if (!REMOVE_PRED_(span->data[i]))
            span->data[write++] = span->data[i];
    }
    uint32_t removed = span->num - write;
    span->num = write;
    return removed;
}

/* ---- cleanup ---- */

#undef REMOVE_PRED_
#undef REMOVE_CTX_PARAM_
#undef REMOVE_CTX
#undef REMOVE_PRED
#undef REMOVE_SPAN
#undef REMOVE_NAME
#undef REMOVE_T
