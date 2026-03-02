/*
 * min_element.h - template header: index of the minimum element in a view.
 *
 * Scans the view and returns the index of the first element e for which
 * no other element is less than e (i.e. the leftmost minimum under the
 * given comparison).  Returns RC_INDEX_NONE if the view is empty.
 *
 * Define before including:
 *   MIN_ELEMENT_T       element type (required)
 *   MIN_ELEMENT_CTX     context type passed to the comparator (optional)
 *   MIN_ELEMENT_CMP     comparator expression (optional; see below)
 *   MIN_ELEMENT_VIEW    view type for MIN_ELEMENT_T
 *                       (optional; default: rc_view_##MIN_ELEMENT_T)
 *   MIN_ELEMENT_NAME    function name
 *                       (optional; default: rc_min_element_##MIN_ELEMENT_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * MIN_ELEMENT_CMP conventions
 * ---------------------------
 * Without MIN_ELEMENT_CTX:
 *   MIN_ELEMENT_CMP(a, b)         — true iff a < b
 *   Default: (a) < (b)
 *
 * With MIN_ELEMENT_CTX:
 *   MIN_ELEMENT_CMP(ctx, a, b)    — true iff a < b given context ctx
 *   ctx is a pointer to MIN_ELEMENT_CTX and is the first argument.
 *   Default: ignores ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx)
 *
 * Return value
 * ------------
 * Index of the first minimum element, or RC_INDEX_NONE if view is empty.
 * RC_INDEX_NONE == (uint32_t)-1 == UINT32_MAX.
 *
 * Example (no context):
 *   #define MIN_ELEMENT_T int
 *   #include "richc/template/min_element.h"
 *   // defines: uint32_t rc_min_element_int(rc_view_int view)
 *
 * Example (context comparator):
 *   #define MIN_ELEMENT_T               Record
 *   #define MIN_ELEMENT_CTX             RecordCmpCtx
 *   #define MIN_ELEMENT_CMP(ctx, a, b)  ((a).key < (b).key)
 *   #define MIN_ELEMENT_NAME            record_min_by_key
 *   #include "richc/template/min_element.h"
 *   // defines: uint32_t record_min_by_key(rc_view_Record view, RecordCmpCtx *ctx)
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef MIN_ELEMENT_T
#  error "MIN_ELEMENT_T must be defined before including min_element.h"
#endif

#ifndef MIN_ELEMENT_VIEW
#  define MIN_ELEMENT_VIEW RC_CONCAT(rc_view_, MIN_ELEMENT_T)
#endif

#ifndef MIN_ELEMENT_NAME
#  define MIN_ELEMENT_NAME RC_CONCAT(rc_min_element_, MIN_ELEMENT_T)
#endif

/*
 * Comparator and optional context.
 *
 * MIN_ELEMENT_CMP_ is the internal two-argument macro used at the single
 * call site within the generated function body.  It closes over 'ctx' from
 * the function scope when a context type is active.
 *
 * When CTX is defined but CMP is not, the default comparator ignores ctx.
 * Folding (void)ctx into the comma expression suppresses the
 * "unused parameter" warning without any #ifdef inside the function body.
 */
#ifdef MIN_ELEMENT_CTX
#  ifndef MIN_ELEMENT_CMP
#    define MIN_ELEMENT_CMP(ctx, a, b) ((a) < (b))
#    define MIN_ELEMENT_CMP_(a, b)  ((void)ctx, (a) < (b))
#  else
#    define MIN_ELEMENT_CMP_(a, b)  MIN_ELEMENT_CMP(ctx, a, b)
#  endif
#  define MIN_ELEMENT_CTX_PARAM_    , MIN_ELEMENT_CTX *ctx
#else
#  ifndef MIN_ELEMENT_CMP
#    define MIN_ELEMENT_CMP(a, b) ((a) < (b))
#  endif
#  define MIN_ELEMENT_CMP_(a, b)    MIN_ELEMENT_CMP(a, b)
#  define MIN_ELEMENT_CTX_PARAM_
#endif

static inline uint32_t
MIN_ELEMENT_NAME(MIN_ELEMENT_VIEW view MIN_ELEMENT_CTX_PARAM_)
{
    if (view.num == 0) return RC_INDEX_NONE;
    uint32_t best = 0;
    for (uint32_t i = 1; i < view.num; i++)
        if (MIN_ELEMENT_CMP_(view.data[i], view.data[best]))
            best = i;
    return best;
}

/* ---- cleanup ---- */

#undef MIN_ELEMENT_CMP_
#undef MIN_ELEMENT_CTX_PARAM_
#undef MIN_ELEMENT_CTX
#undef MIN_ELEMENT_CMP
#undef MIN_ELEMENT_VIEW
#undef MIN_ELEMENT_NAME
#undef MIN_ELEMENT_T
