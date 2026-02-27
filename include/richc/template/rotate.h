/*
 * rotate.h - template header: in-place rotation of a span.
 *
 * Rotates the elements of span so that the element originally at index k
 * is at index 0 after the call, with all other elements following in the
 * same relative order (a left rotation by k positions).
 *
 * The operation is O(n) and uses O(1) extra space (one element temporary
 * for swaps).  No arena is needed.
 *
 * If k == 0 or k >= span.num the call is a no-op.
 *
 * Define before including:
 *   ROTATE_T       element type (required)
 *   ROTATE_SPAN    span type for ROTATE_T
 *                  (optional; default: rc_span_##ROTATE_T)
 *   ROTATE_NAME    function name
 *                  (optional; default: rc_rotate_##ROTATE_T)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Generated function signature
 * ----------------------------
 *   void NAME(SPAN span, uint32_t k)
 *
 * Example:
 *   #define ROTATE_T int
 *   #include "richc/template/rotate.h"
 *   // defines: void rc_rotate_int(rc_span_int span, uint32_t k)
 *
 *   int data[] = {1, 2, 3, 4, 5};
 *   rc_rotate_int(RC_SPAN(data), 2);   // -> {3, 4, 5, 1, 2}
 *
 * Algorithm
 * ---------
 * Three-reversal method:
 *   1. Reverse data[0, k)
 *   2. Reverse data[k, n)
 *   3. Reverse data[0, n)
 * Each reversal is O(n/2) swaps; the total cost is exactly n/2 + (n-k)/2 + k/2
 * <= n swaps, with a single ROTATE_T temporary.
 */

#include <stdint.h>
#include "richc/template_util.h"

#ifndef ROTATE_T
#  error "ROTATE_T must be defined before including rotate.h"
#endif

#ifndef ROTATE_SPAN
#  define ROTATE_SPAN RC_CONCAT(rc_span_, ROTATE_T)
#endif

#ifndef ROTATE_NAME
#  define ROTATE_NAME RC_CONCAT(rc_rotate_, ROTATE_T)
#endif

/* Private helper name — derived from ROTATE_NAME to stay unique per instantiation. */
#define ROTATE_REVERSE_ RC_CONCAT(ROTATE_NAME, _reverse_)

/*
 * Reverse data[lo, hi) in place using a single ROTATE_T temporary.
 */
static inline void ROTATE_REVERSE_(ROTATE_T *data, uint32_t lo, uint32_t hi)
{
    while (lo + 1 < hi) {
        hi--;
        ROTATE_T tmp = data[lo];
        data[lo]     = data[hi];
        data[hi]     = tmp;
        lo++;
    }
}

/*
 * Rotate span left by k positions so that span.data[k] becomes span.data[0].
 * No-op when k == 0 or k >= span.num.
 */
static inline void ROTATE_NAME(ROTATE_SPAN span, uint32_t k)
{
    if (k == 0 || k >= span.num) return;
    ROTATE_REVERSE_(span.data, 0, k);
    ROTATE_REVERSE_(span.data, k, span.num);
    ROTATE_REVERSE_(span.data, 0, span.num);
}

/* ---- cleanup ---- */

#undef ROTATE_REVERSE_
#undef ROTATE_SPAN
#undef ROTATE_NAME
#undef ROTATE_T
