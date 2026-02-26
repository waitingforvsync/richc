/*
 * accumulate.h - template header: fold a view into a single accumulator value.
 *
 * Applies ACCUM_FUNC cumulatively to each element of the view starting from
 * init, and returns the final accumulator value.  Equivalent to
 * std::accumulate.  No memory allocation is performed.
 *
 * Define before including:
 *   ACCUM_T          element type (required)
 *   ACCUM_RESULT_T   accumulator / result type (required)
 *   ACCUM_FUNC       reduce expression (optional; see below)
 *   ACCUM_CTX        context type passed to the reduce function (optional)
 *   ACCUM_VIEW       view type for ACCUM_T
 *                    (optional; default: rc_view_##ACCUM_T)
 *   ACCUM_NAME       function name
 *                    (optional; default: rc_accumulate_##ACCUM_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * ACCUM_FUNC conventions
 * ----------------------
 * Without ACCUM_CTX:
 *   ACCUM_FUNC(acc, element)        — returns ACCUM_RESULT_T
 *   Default: (acc) + (element)
 *
 * With ACCUM_CTX:
 *   ACCUM_FUNC(ctx, acc, element)   — ctx is a pointer to ACCUM_CTX and is
 *                                     the first argument; returns ACCUM_RESULT_T
 *   Default: ignores ctx, uses (acc) + (element)
 *
 * Generated function signature
 * ----------------------------
 * Without context:
 *   RESULT_T NAME(VIEW view, RESULT_T init)
 *
 * With context:
 *   RESULT_T NAME(VIEW view, CTX *ctx, RESULT_T init)
 *
 * ctx precedes init so that init—the "seed value"—remains last,
 * consistent with the convention used by find.h (ctx before value).
 *
 * Example (no context, default addition):
 *   #define ACCUM_T        int
 *   #define ACCUM_RESULT_T int
 *   #include "richc/accumulate.h"
 *   // defines: int rc_accumulate_int(rc_view_int view, int init)
 *
 * Example (custom func, no context — running product):
 *   #define ACCUM_T            int
 *   #define ACCUM_RESULT_T     int
 *   #define ACCUM_FUNC(acc, e) ((acc) * (e))
 *   #define ACCUM_NAME         int_product
 *   #include "richc/accumulate.h"
 *   // defines: int int_product(rc_view_int view, int init)
 *
 * Example (context — weighted sum):
 *   typedef struct { int weight; } WeightCtx;
 *   #define ACCUM_T                 int
 *   #define ACCUM_RESULT_T          int
 *   #define ACCUM_CTX               WeightCtx
 *   #define ACCUM_FUNC(ctx, acc, e) ((acc) + (e) * (ctx)->weight)
 *   #define ACCUM_NAME              int_weighted_sum
 *   #include "richc/accumulate.h"
 *   // defines: int int_weighted_sum(rc_view_int view, WeightCtx *ctx, int init)
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef ACCUM_T
#  error "ACCUM_T must be defined before including accumulate.h"
#endif
#ifndef ACCUM_RESULT_T
#  error "ACCUM_RESULT_T must be defined before including accumulate.h"
#endif

#ifndef ACCUM_VIEW
#  define ACCUM_VIEW RC_CONCAT(rc_view_, ACCUM_T)
#endif

#ifndef ACCUM_NAME
#  define ACCUM_NAME RC_CONCAT(rc_accumulate_, ACCUM_T)
#endif

/*
 * Reduce function and optional context.
 *
 * ACCUM_FUNC_ is the internal two-argument macro (acc, element) used at the
 * single call site inside the generated function body.  It closes over 'ctx'
 * from the function scope when a context type is active.
 *
 * When CTX is defined but FUNC is not, the default addition ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef ACCUM_CTX
#  ifndef ACCUM_FUNC
#    define ACCUM_FUNC(ctx, acc, element) ((acc) + (element))
#    define ACCUM_FUNC_(acc, element)  ((void)ctx, (acc) + (element))
#  else
#    define ACCUM_FUNC_(acc, element)  ACCUM_FUNC(ctx, acc, element)
#  endif
#  define ACCUM_CTX_PARAM_   , ACCUM_CTX *ctx
#else
#  ifndef ACCUM_FUNC
#    define ACCUM_FUNC(acc, element) ((acc) + (element))
#  endif
#  define ACCUM_FUNC_(acc, element)   ACCUM_FUNC(acc, element)
#  define ACCUM_CTX_PARAM_
#endif

static inline ACCUM_RESULT_T
ACCUM_NAME(ACCUM_VIEW view ACCUM_CTX_PARAM_, ACCUM_RESULT_T init)
{
    ACCUM_RESULT_T acc = init;
    for (uint32_t i = 0; i < view.num; i++)
        acc = ACCUM_FUNC_(acc, view.data[i]);
    return acc;
}

/* ---- cleanup ---- */

#undef ACCUM_FUNC_
#undef ACCUM_CTX_PARAM_
#undef ACCUM_CTX
#undef ACCUM_FUNC
#undef ACCUM_VIEW
#undef ACCUM_NAME
#undef ACCUM_T
#undef ACCUM_RESULT_T
