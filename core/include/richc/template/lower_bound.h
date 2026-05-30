/*
 * lower_bound.h - template header: binary search on a sorted view.
 *
 * Finds the first element e in the view for which !(e < value) - i.e. the first
 * element >= value, assuming the view is sorted ascending under the comparison.
 * Returns the index of that element, or view.num if every element satisfies
 * e < value.  Include it again (after redefining the control macros) to
 * instantiate another element type.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_LOWER_BOUND_TYPE   element type (required)
 *   RC_LOWER_BOUND_CTX    context type passed to the comparator (optional)
 *   RC_LOWER_BOUND_CMP    comparator expression (optional; see below)
 *   RC_LOWER_BOUND_VIEW   view type (optional; default rc_view_<TYPE>)
 *   RC_LOWER_BOUND_NAME   function name (optional; default rc_lower_bound_<TYPE>)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Comparator conventions
 * ----------------------
 * Without RC_LOWER_BOUND_CTX:
 *   RC_LOWER_BOUND_CMP(a, b)        - true iff a < b.  Default: (a) < (b).
 * With RC_LOWER_BOUND_CTX:
 *   RC_LOWER_BOUND_CMP(ctx, a, b)   - true iff a < b given context ctx.  ctx is
 *   a pointer to RC_LOWER_BOUND_CTX and is the first argument.  Default: ignores
 *   ctx, uses (a) < (b).
 *
 * Generated function signature
 * ----------------------------
 * Without context:  uint32_t NAME(VIEW view, TYPE value)
 * With context:     uint32_t NAME(VIEW view, CTX *ctx, TYPE value)
 *
 * Example (no context):
 *   #define RC_LOWER_BOUND_TYPE int
 *   #include "richc/template/lower_bound.h"
 *   // uint32_t rc_lower_bound_int(rc_view_int view, int value);
 *
 * Example (context comparator):
 *   typedef struct { uint32_t key_offset; } record_cmp_ctx;
 *   #define RC_LOWER_BOUND_TYPE         record
 *   #define RC_LOWER_BOUND_CTX          record_cmp_ctx
 *   #define RC_LOWER_BOUND_CMP(ctx, a, b)  (key(ctx, a) < key(ctx, b))
 *   #define RC_LOWER_BOUND_NAME         rc_lower_bound_record
 *   #include "richc/template/lower_bound.h"
 *   // uint32_t rc_lower_bound_record(rc_view_record, record_cmp_ctx *, record);
 */

#include <stdint.h>

#include "richc/macros.h"

#ifndef RC_LOWER_BOUND_TYPE
#  define RC_LOWER_BOUND_TYPE int   // to keep intellisense happy
#  error "RC_LOWER_BOUND_TYPE must be defined before including richc/template/lower_bound.h"
#endif

#ifndef RC_LOWER_BOUND_VIEW
#  define RC_LOWER_BOUND_VIEW RC_CONCAT(rc_view_, RC_LOWER_BOUND_TYPE)
#endif

#ifndef RC_LOWER_BOUND_NAME
#  define RC_LOWER_BOUND_NAME RC_CONCAT(rc_lower_bound_, RC_LOWER_BOUND_TYPE)
#endif

/*
 * RC_LOWER_BOUND_CMP_ is the internal two-argument comparator used inside the
 * function body; when a context type is active it closes over the 'ctx'
 * parameter.  The default context comparator folds (void)ctx into the comma
 * expression to suppress the unused-parameter warning.
 */
#ifdef RC_LOWER_BOUND_CTX
#  ifndef RC_LOWER_BOUND_CMP
#    define RC_LOWER_BOUND_CMP(ctx, a, b) ((a) < (b))
#    define RC_LOWER_BOUND_CMP_(a, b)     ((void)ctx, (a) < (b))
#  else
#    define RC_LOWER_BOUND_CMP_(a, b)     RC_LOWER_BOUND_CMP(ctx, a, b)
#  endif
#else
#  ifndef RC_LOWER_BOUND_CMP
#    define RC_LOWER_BOUND_CMP(a, b) ((a) < (b))
#  endif
#  define RC_LOWER_BOUND_CMP_(a, b)       RC_LOWER_BOUND_CMP(a, b)
#endif

static inline uint32_t
#ifdef RC_LOWER_BOUND_CTX
RC_LOWER_BOUND_NAME(RC_LOWER_BOUND_VIEW view, RC_LOWER_BOUND_CTX *ctx, RC_LOWER_BOUND_TYPE value)
#else
RC_LOWER_BOUND_NAME(RC_LOWER_BOUND_VIEW view, RC_LOWER_BOUND_TYPE value)
#endif
{
    uint32_t lo = 0;
    uint32_t hi = view.num;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (RC_LOWER_BOUND_CMP_(view.data[mid], value)) {
            lo = mid + 1u;
        }
        else {
            hi = mid;
        }
    }
    return lo;
}

/* ---- cleanup ---- */

#undef RC_LOWER_BOUND_CMP_
#undef RC_LOWER_BOUND_CTX
#undef RC_LOWER_BOUND_CMP
#undef RC_LOWER_BOUND_VIEW
#undef RC_LOWER_BOUND_NAME
#undef RC_LOWER_BOUND_TYPE
