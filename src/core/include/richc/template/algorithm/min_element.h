/*
 * min_element.h - template header: index of the minimum element in a view.
 *
 * Scans the view and returns the index of the first element e for which no other
 * element is less than e - the leftmost minimum under the comparison.  Returns
 * RC_INDEX_NONE if the view is empty.  Include it again (after redefining the
 * control macros) to instantiate another element type.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_MIN_ELEMENT_TYPE   element type (required)
 *   RC_MIN_ELEMENT_CTX    context type passed to the comparator (optional)
 *   RC_MIN_ELEMENT_CMP    comparator expression (optional; see below)
 *   RC_MIN_ELEMENT_VIEW   view type (optional; default rc_view_<TYPE>)
 *   RC_MIN_ELEMENT_NAME   function name (optional; default rc_min_element_<TYPE>)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Comparator conventions
 * ----------------------
 * Without RC_MIN_ELEMENT_CTX:
 *   RC_MIN_ELEMENT_CMP(a, b)        - true iff a < b.  Default: (a) < (b).
 * With RC_MIN_ELEMENT_CTX:
 *   RC_MIN_ELEMENT_CMP(ctx, a, b)   - true iff a < b given context ctx.  ctx is a
 *   pointer to RC_MIN_ELEMENT_CTX and is the first argument.  Default: ignores
 *   ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx)
 * Returns the index of the first minimum element, or RC_INDEX_NONE if empty.
 *
 * Example (no context):
 *   #define RC_MIN_ELEMENT_TYPE int
 *   #include "richc/template/algorithm/min_element.h"
 *   // uint32_t rc_min_element_int(rc_view_int view);
 *
 * Example (context comparator):
 *   #define RC_MIN_ELEMENT_TYPE         record
 *   #define RC_MIN_ELEMENT_CTX          record_cmp_ctx
 *   #define RC_MIN_ELEMENT_CMP(ctx, a, b)  ((a).key < (b).key)
 *   #define RC_MIN_ELEMENT_NAME         rc_min_element_record
 *   #include "richc/template/algorithm/min_element.h"
 *   // uint32_t rc_min_element_record(rc_view_record view, record_cmp_ctx *ctx);
 */

#include <stdint.h>

#include "richc/macros.h"

#ifndef RC_MIN_ELEMENT_TYPE
#  define RC_MIN_ELEMENT_TYPE int   // to keep intellisense happy
#  error "RC_MIN_ELEMENT_TYPE must be defined before including richc/template/algorithm/min_element.h"
#endif

#ifndef RC_MIN_ELEMENT_VIEW
#  define RC_MIN_ELEMENT_VIEW RC_CONCAT(rc_view_, RC_MIN_ELEMENT_TYPE)
#endif

#ifndef RC_MIN_ELEMENT_NAME
#  define RC_MIN_ELEMENT_NAME RC_CONCAT(rc_min_element_, RC_MIN_ELEMENT_TYPE)
#endif

/*
 * RC_MIN_ELEMENT_CMP_ is the internal two-argument comparator used inside the
 * function body; when a context type is active it closes over the 'ctx'
 * parameter.  The default context comparator folds (void)ctx into the comma
 * expression to suppress the unused-parameter warning.
 *
 * RC_MIN_ELEMENT_VIEW_CTX_PARAM_ is the leading parameter list - "VIEW view" or
 * "VIEW view, CTX *ctx" - so the context, when present, follows the view.
 */
#ifdef RC_MIN_ELEMENT_CTX
#  ifndef RC_MIN_ELEMENT_CMP
#    define RC_MIN_ELEMENT_CMP(ctx, a, b) ((a) < (b))
#    define RC_MIN_ELEMENT_CMP_(a, b)     ((void)ctx, (a) < (b))
#  else
#    define RC_MIN_ELEMENT_CMP_(a, b)     RC_MIN_ELEMENT_CMP(ctx, a, b)
#  endif
#  define RC_MIN_ELEMENT_VIEW_CTX_PARAM_  RC_MIN_ELEMENT_VIEW view, RC_MIN_ELEMENT_CTX *ctx
#else
#  ifndef RC_MIN_ELEMENT_CMP
#    define RC_MIN_ELEMENT_CMP(a, b) ((a) < (b))
#  endif
#  define RC_MIN_ELEMENT_CMP_(a, b)       RC_MIN_ELEMENT_CMP(a, b)
#  define RC_MIN_ELEMENT_VIEW_CTX_PARAM_  RC_MIN_ELEMENT_VIEW view
#endif

static inline uint32_t
RC_MIN_ELEMENT_NAME(RC_MIN_ELEMENT_VIEW_CTX_PARAM_)
{
    if (view.num == 0) {
        return RC_INDEX_NONE;
    }
    uint32_t best = 0;
    for (uint32_t i = 1; i < view.num; i++) {
        // keep the leftmost minimum: only move on a strictly smaller element
        if (RC_MIN_ELEMENT_CMP_(view.data[i], view.data[best])) {
            best = i;
        }
    }
    return best;
}

/* ---- cleanup ---- */

#undef RC_MIN_ELEMENT_CMP_
#undef RC_MIN_ELEMENT_VIEW_CTX_PARAM_
#undef RC_MIN_ELEMENT_CTX
#undef RC_MIN_ELEMENT_CMP
#undef RC_MIN_ELEMENT_VIEW
#undef RC_MIN_ELEMENT_NAME
#undef RC_MIN_ELEMENT_TYPE
