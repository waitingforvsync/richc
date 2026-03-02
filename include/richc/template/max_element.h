/*
 * max_element.h - template header: index of the maximum element in a view.
 *
 * Scans the view and returns the index of the first element e for which
 * no other element is greater than e (i.e. the leftmost maximum under the
 * given comparison).  Returns RC_INDEX_NONE if the view is empty.
 *
 * Define before including:
 *   MAX_ELEMENT_T       element type (required)
 *   MAX_ELEMENT_CTX     context type passed to the comparator (optional)
 *   MAX_ELEMENT_CMP     comparator expression (optional; see below)
 *   MAX_ELEMENT_VIEW    view type for MAX_ELEMENT_T
 *                       (optional; default: rc_view_##MAX_ELEMENT_T)
 *   MAX_ELEMENT_NAME    function name
 *                       (optional; default: rc_max_element_##MAX_ELEMENT_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * MAX_ELEMENT_CMP conventions
 * ---------------------------
 * Without MAX_ELEMENT_CTX:
 *   MAX_ELEMENT_CMP(a, b)         — true iff a < b
 *   Default: (a) < (b)
 *
 * With MAX_ELEMENT_CTX:
 *   MAX_ELEMENT_CMP(ctx, a, b)    — true iff a < b given context ctx
 *   ctx is a pointer to MAX_ELEMENT_CTX and is the first argument.
 *   Default: ignores ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx)
 *
 * Return value
 * ------------
 * Index of the first maximum element, or RC_INDEX_NONE if view is empty.
 * RC_INDEX_NONE == (uint32_t)-1 == UINT32_MAX.
 *
 * Example (no context):
 *   #define MAX_ELEMENT_T int
 *   #include "richc/template/max_element.h"
 *   // defines: uint32_t rc_max_element_int(rc_view_int view)
 *
 * Example (context comparator):
 *   #define MAX_ELEMENT_T               Record
 *   #define MAX_ELEMENT_CTX             RecordCmpCtx
 *   #define MAX_ELEMENT_CMP(ctx, a, b)  ((a).key < (b).key)
 *   #define MAX_ELEMENT_NAME            record_max_by_key
 *   #include "richc/template/max_element.h"
 *   // defines: uint32_t record_max_by_key(rc_view_Record view, RecordCmpCtx *ctx)
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef MAX_ELEMENT_T
#  error "MAX_ELEMENT_T must be defined before including max_element.h"
#endif

#ifndef MAX_ELEMENT_VIEW
#  define MAX_ELEMENT_VIEW RC_CONCAT(rc_view_, MAX_ELEMENT_T)
#endif

#ifndef MAX_ELEMENT_NAME
#  define MAX_ELEMENT_NAME RC_CONCAT(rc_max_element_, MAX_ELEMENT_T)
#endif

/*
 * Comparator and optional context.
 *
 * MAX_ELEMENT_CMP_ is the internal two-argument macro used at the single
 * call site within the generated function body.  It closes over 'ctx' from
 * the function scope when a context type is active.
 *
 * When CTX is defined but CMP is not, the default comparator ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef MAX_ELEMENT_CTX
#  ifndef MAX_ELEMENT_CMP
#    define MAX_ELEMENT_CMP(ctx, a, b) ((a) < (b))
#    define MAX_ELEMENT_CMP_(a, b)  ((void)ctx, (a) < (b))
#  else
#    define MAX_ELEMENT_CMP_(a, b)  MAX_ELEMENT_CMP(ctx, a, b)
#  endif
#  define MAX_ELEMENT_CTX_PARAM_    , MAX_ELEMENT_CTX *ctx
#else
#  ifndef MAX_ELEMENT_CMP
#    define MAX_ELEMENT_CMP(a, b) ((a) < (b))
#  endif
#  define MAX_ELEMENT_CMP_(a, b)    MAX_ELEMENT_CMP(a, b)
#  define MAX_ELEMENT_CTX_PARAM_
#endif

static inline uint32_t
MAX_ELEMENT_NAME(MAX_ELEMENT_VIEW view MAX_ELEMENT_CTX_PARAM_)
{
    if (view.num == 0) return RC_INDEX_NONE;
    uint32_t best = 0;
    for (uint32_t i = 1; i < view.num; i++)
        if (MAX_ELEMENT_CMP_(view.data[best], view.data[i]))
            best = i;
    return best;
}

/* ---- cleanup ---- */

#undef MAX_ELEMENT_CMP_
#undef MAX_ELEMENT_CTX_PARAM_
#undef MAX_ELEMENT_CTX
#undef MAX_ELEMENT_CMP
#undef MAX_ELEMENT_VIEW
#undef MAX_ELEMENT_NAME
#undef MAX_ELEMENT_T
