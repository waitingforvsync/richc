/*
 * mismatch.h - template header: find the first mismatched element between two views.
 *
 * Scans both views in parallel and returns the index of the first position
 * where the elements are not equal under MISMATCH_PRED.  If no mismatch is
 * found within the overlapping region, returns min(view1.num, view2.num).
 *
 * This matches the behaviour of C++ std::mismatch: the returned index is
 * always in [0, min(view1.num, view2.num)].  The caller can test for "no
 * mismatch in the overlap" by comparing the result to that minimum.
 *
 * Define before including:
 *   MISMATCH_T       element type (required)
 *   MISMATCH_PRED    equality predicate (optional; see below)
 *   MISMATCH_CTX     context type passed to the predicate (optional)
 *   MISMATCH_VIEW    view type for MISMATCH_T
 *                    (optional; default: rc_view_##MISMATCH_T)
 *   MISMATCH_NAME    function name
 *                    (optional; default: rc_mismatch_##MISMATCH_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * MISMATCH_PRED conventions
 * -------------------------
 * The predicate returns true iff the two elements are considered EQUAL
 * (i.e. they match).  A mismatch is reported when the predicate is false.
 *
 * Without MISMATCH_CTX:
 *   MISMATCH_PRED(a, b)         — true iff a == b
 *   Default: (a) == (b)
 *
 * With MISMATCH_CTX:
 *   MISMATCH_PRED(ctx, a, b)    — true iff a == b given context ctx
 *   ctx is a pointer to MISMATCH_CTX and is the first argument.
 *   Default: ignores ctx, uses (a) == (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view1, VIEW view2)
 * With context:     uint32_t NAME(VIEW view1, VIEW view2, CTX *ctx)
 *
 * Return value
 * ------------
 * Index of the first position where the elements differ, or
 * min(view1.num, view2.num) if the views agree over their entire overlap.
 * The return is always in [0, min(view1.num, view2.num)].
 *
 * Example (no context, default equality):
 *   #define MISMATCH_T int
 *   #include "richc/template/mismatch.h"
 *   // defines: uint32_t rc_mismatch_int(rc_view_int view1, rc_view_int view2)
 *
 * Example (custom predicate, no context):
 *   #define MISMATCH_T            Record
 *   #define MISMATCH_PRED(a, b)   ((a).key == (b).key)
 *   #define MISMATCH_NAME         record_mismatch_by_key
 *   #include "richc/template/mismatch.h"
 *   // defines: uint32_t record_mismatch_by_key(rc_view_Record v1, rc_view_Record v2)
 *
 * Example (context predicate):
 *   typedef struct { int tolerance; } TolCtx;
 *   #define MISMATCH_T                 int
 *   #define MISMATCH_CTX               TolCtx
 *   #define MISMATCH_PRED(ctx, a, b)   (abs((a)-(b)) <= (ctx)->tolerance)
 *   #define MISMATCH_NAME              int_mismatch_tol
 *   #include "richc/template/mismatch.h"
 *   // defines: uint32_t int_mismatch_tol(rc_view_int v1, rc_view_int v2, TolCtx *ctx)
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef MISMATCH_T
#  error "MISMATCH_T must be defined before including mismatch.h"
#endif

#ifndef MISMATCH_VIEW
#  define MISMATCH_VIEW RC_CONCAT(rc_view_, MISMATCH_T)
#endif

#ifndef MISMATCH_NAME
#  define MISMATCH_NAME RC_CONCAT(rc_mismatch_, MISMATCH_T)
#endif

/*
 * Predicate and optional context.
 *
 * MISMATCH_PRED_ is the internal two-argument macro used at the single call
 * site within the generated function body.  It closes over 'ctx' from the
 * function scope when a context type is active.
 *
 * When CTX is defined but PRED is not, the default (a)==(b) ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef MISMATCH_CTX
#  ifndef MISMATCH_PRED
#    define MISMATCH_PRED(ctx, a, b)  ((a) == (b))
#    define MISMATCH_PRED_(a, b)      ((void)ctx, (a) == (b))
#  else
#    define MISMATCH_PRED_(a, b)      MISMATCH_PRED(ctx, a, b)
#  endif
#  define MISMATCH_CTX_PARAM_         , MISMATCH_CTX *ctx
#else
#  ifndef MISMATCH_PRED
#    define MISMATCH_PRED(a, b)  ((a) == (b))
#  endif
#  define MISMATCH_PRED_(a, b)    MISMATCH_PRED(a, b)
#  define MISMATCH_CTX_PARAM_
#endif

static inline uint32_t
MISMATCH_NAME(MISMATCH_VIEW view1, MISMATCH_VIEW view2 MISMATCH_CTX_PARAM_)
{
    uint32_t n = view1.num < view2.num ? view1.num : view2.num;
    for (uint32_t i = 0; i < n; i++)
        if (!MISMATCH_PRED_(view1.data[i], view2.data[i]))
            return i;
    return n;
}

/* ---- cleanup ---- */

#undef MISMATCH_PRED_
#undef MISMATCH_CTX_PARAM_
#undef MISMATCH_CTX
#undef MISMATCH_PRED
#undef MISMATCH_VIEW
#undef MISMATCH_NAME
#undef MISMATCH_T
