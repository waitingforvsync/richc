/*
 * transform.h - template header: map a source view into a destination array.
 *
 * Applies TRANSFORM_FUNC to each element of src and appends the results to
 * *dst, growing the array via the arena as needed.  Returns the index in
 * *dst of the first appended element (== dst->num on entry before any
 * reallocation), so the caller can reference the output range.  If src is
 * empty the function is a no-op and returns dst->num unchanged.
 *
 * Requires that array.h has already been included for TRANSFORM_DST_T so
 * that the generated rc_array_DST_T_push helper is available.
 *
 * Define before including:
 *   TRANSFORM_SRC_T      source element type (required)
 *   TRANSFORM_DST_T      destination element type (required)
 *   TRANSFORM_FUNC       transform expression (optional; see below)
 *   TRANSFORM_CTX        context type passed to the transform (optional)
 *   TRANSFORM_SRC_VIEW   source view type
 *                        (optional; default: rc_view_##TRANSFORM_SRC_T)
 *   TRANSFORM_DST_ARRAY  destination array type
 *                        (optional; default: rc_array_##TRANSFORM_DST_T)
 *   TRANSFORM_NAME       function name
 *                        (optional; default: rc_transform_##TRANSFORM_SRC_T)
 *                        Must be provided explicitly when instantiating
 *                        multiple transforms from the same source type.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * TRANSFORM_FUNC conventions
 * --------------------------
 * Without TRANSFORM_CTX:
 *   TRANSFORM_FUNC(element)          — returns TRANSFORM_DST_T
 *   Default: (element)   (identity / implicit numeric conversion)
 *
 * With TRANSFORM_CTX:
 *   TRANSFORM_FUNC(ctx, element)     — ctx is first; returns TRANSFORM_DST_T
 *   Default: ignores ctx, uses (element).
 *
 * Generated function signature
 * ----------------------------
 * Without context:
 *   uint32_t NAME(SRC_VIEW src, DST_ARRAY *dst, rc_arena *a)
 *
 * With context:
 *   uint32_t NAME(SRC_VIEW src, DST_ARRAY *dst, CTX *ctx, rc_arena *a)
 *
 * a may be NULL when dst already has sufficient capacity.
 *
 * Example (no context):
 *   #define TRANSFORM_SRC_T   int
 *   #define TRANSFORM_DST_T   int
 *   #define TRANSFORM_FUNC(e) ((e) * (e))
 *   #define TRANSFORM_NAME    int_square
 *   #include "richc/transform.h"
 *   // defines: uint32_t int_square(rc_view_int src, rc_array_int *dst, rc_arena *a)
 *
 * Example (context):
 *   typedef struct { int factor; } ScaleCtx;
 *   #define TRANSFORM_SRC_T        int
 *   #define TRANSFORM_DST_T        int
 *   #define TRANSFORM_CTX          ScaleCtx
 *   #define TRANSFORM_FUNC(ctx, e) ((e) * (ctx)->factor)
 *   #define TRANSFORM_NAME         int_scale
 *   #include "richc/transform.h"
 *   // defines: uint32_t int_scale(rc_view_int src, rc_array_int *dst, ScaleCtx *ctx, rc_arena *a)
 */

#include "richc/template_util.h"
#include "richc/arena.h"

#ifndef TRANSFORM_SRC_T
#  error "TRANSFORM_SRC_T must be defined before including transform.h"
#endif
#ifndef TRANSFORM_DST_T
#  error "TRANSFORM_DST_T must be defined before including transform.h"
#endif

#ifndef TRANSFORM_SRC_VIEW
#  define TRANSFORM_SRC_VIEW  RC_CONCAT(rc_view_, TRANSFORM_SRC_T)
#endif

#ifndef TRANSFORM_DST_ARRAY
#  define TRANSFORM_DST_ARRAY RC_CONCAT(rc_array_, TRANSFORM_DST_T)
#endif

#ifndef TRANSFORM_NAME
#  define TRANSFORM_NAME RC_CONCAT(rc_transform_, TRANSFORM_SRC_T)
#endif

/*
 * Transform function and optional context.
 *
 * TRANSFORM_FUNC_ is the internal single-argument macro used at the call
 * site inside the generated function body.  It closes over 'ctx' from the
 * function scope when a context type is active.
 *
 * When CTX is defined but FUNC is not, the identity default ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef TRANSFORM_CTX
#  ifndef TRANSFORM_FUNC
#    define TRANSFORM_FUNC(ctx, element) (element)
#    define TRANSFORM_FUNC_(element)  ((void)ctx, (element))
#  else
#    define TRANSFORM_FUNC_(element)  TRANSFORM_FUNC(ctx, element)
#  endif
#  define TRANSFORM_CTX_PARAM_   , TRANSFORM_CTX *ctx
#else
#  ifndef TRANSFORM_FUNC
#    define TRANSFORM_FUNC(element) (element)
#  endif
#  define TRANSFORM_FUNC_(element)   TRANSFORM_FUNC(element)
#  define TRANSFORM_CTX_PARAM_
#endif

/* Internal name for the destination array's push function. */
#define TRANSFORM_PUSH_  RC_CONCAT(RC_CONCAT(rc_array_, TRANSFORM_DST_T), _push)

static inline uint32_t
TRANSFORM_NAME(TRANSFORM_SRC_VIEW src, TRANSFORM_DST_ARRAY *dst TRANSFORM_CTX_PARAM_, rc_arena *a)
{
    uint32_t first = dst->num;
    for (uint32_t i = 0; i < src.num; i++)
        TRANSFORM_PUSH_(dst, TRANSFORM_FUNC_(src.data[i]), a);
    return first;
}

/* ---- cleanup ---- */

#undef TRANSFORM_FUNC_
#undef TRANSFORM_CTX_PARAM_
#undef TRANSFORM_PUSH_
#undef TRANSFORM_CTX
#undef TRANSFORM_FUNC
#undef TRANSFORM_SRC_VIEW
#undef TRANSFORM_DST_ARRAY
#undef TRANSFORM_NAME
#undef TRANSFORM_SRC_T
#undef TRANSFORM_DST_T
