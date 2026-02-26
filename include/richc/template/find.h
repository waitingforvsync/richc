/*
 * find.h - template header: linear search on a view.
 *
 * Returns the index of the first element for which the predicate is true,
 * or RC_INDEX_NONE (== UINT32_MAX) if no element matches.
 *
 * Define before including:
 *   FIND_T       element type (required)
 *   FIND_CTX     context type passed to the predicate (optional)
 *   FIND_PRED    match predicate expression (optional; see below)
 *   FIND_VIEW    view type for FIND_T
 *                (optional; default: rc_view_##FIND_T)
 *   FIND_NAME    function name
 *                (optional; default: rc_find_##FIND_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * FIND_PRED conventions
 * ---------------------
 * Without FIND_CTX:
 *   FIND_PRED(element, value)        — true iff element matches value
 *   Default: (element) == (value)
 *
 * With FIND_CTX:
 *   FIND_PRED(ctx, element, value)   — true iff element matches value given ctx
 *   ctx is a pointer to FIND_CTX and is the first argument.
 *   Default: ignores ctx, uses (element) == (value).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view, T value)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx, T value)
 *
 * Return value
 * ------------
 * RC_INDEX_NONE — (uint32_t)-1 == UINT32_MAX — returned when no element matches.
 * Defined once on the first inclusion of this header.
 *
 * Example (no context, default equality):
 *   #define FIND_T int
 *   #include "richc/find.h"
 *   // defines: uint32_t rc_find_int(rc_view_int view, int value);
 *
 * Example (custom predicate, no context):
 *   #define FIND_T        Record
 *   #define FIND_PRED(e, v)  ((e).key == (v).key)
 *   #include "richc/find.h"
 *   // defines: uint32_t Record_find(rc_view_Record view, Record value);
 *
 * Example (context predicate):
 *   typedef struct { int tolerance; } TolCtx;
 *   #define FIND_T               int
 *   #define FIND_CTX             TolCtx
 *   #define FIND_PRED(ctx, e, v) ((e) >= (v) - (ctx)->tolerance && \
 *                                 (e) <= (v) + (ctx)->tolerance)
 *   #define FIND_NAME            int_find_tol
 *   #include "richc/find.h"
 *   // defines: uint32_t int_find_tol(rc_view_int view, TolCtx *ctx, int value);
 */

#include <stdint.h>
#include "richc/template_util.h"

/* RC_INDEX_NONE: sentinel returned when no element is found.
 * Protected by #ifndef so multiple inclusions emit it only once. */
#ifndef RC_INDEX_NONE
#  define RC_INDEX_NONE ((uint32_t)-1)
#endif

#ifndef FIND_T
#  error "FIND_T must be defined before including find.h"
#endif

#ifndef FIND_VIEW
#  define FIND_VIEW RC_CONCAT(rc_view_, FIND_T)
#endif

#ifndef FIND_NAME
#  define FIND_NAME RC_CONCAT(rc_find_, FIND_T)
#endif

/*
 * Predicate and optional context.
 *
 * FIND_PRED_ is the internal two-argument macro used at the single call
 * site within the generated function body.  It closes over 'ctx' from
 * the function scope when a context type is active.
 *
 * When CTX is defined but PRED is not, the default (element)==(value)
 * ignores ctx.  Folding (void)ctx into the comma expression suppresses
 * the "unused parameter" warning without any #ifdef in the function body.
 */
#ifdef FIND_CTX
#  ifndef FIND_PRED
#    define FIND_PRED(ctx, element, value) ((element) == (value))
#    define FIND_PRED_(element, value)  ((void)ctx, (element) == (value))
#  else
#    define FIND_PRED_(element, value)  FIND_PRED(ctx, element, value)
#  endif
#  define FIND_CTX_PARAM_   , FIND_CTX *ctx
#else
#  ifndef FIND_PRED
#    define FIND_PRED(element, value) ((element) == (value))
#  endif
#  define FIND_PRED_(element, value)   FIND_PRED(element, value)
#  define FIND_CTX_PARAM_
#endif

static inline uint32_t
FIND_NAME(FIND_VIEW view FIND_CTX_PARAM_, FIND_T value)
{
    for (uint32_t i = 0; i < view.num; i++)
        if (FIND_PRED_(view.data[i], value))
            return i;
    return RC_INDEX_NONE;
}

/* ---- cleanup ---- */

#undef FIND_PRED_
#undef FIND_CTX_PARAM_
#undef FIND_CTX
#undef FIND_PRED
#undef FIND_VIEW
#undef FIND_NAME
#undef FIND_T
