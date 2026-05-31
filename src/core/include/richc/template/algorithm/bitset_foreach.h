/*
 * bitset_foreach.h - template header: visit the set bits of an rc_bitset.
 *
 * Walks the set bits in ascending index order using the bitset iteration idiom
 * (get_first_set / get_next_set) and invokes a caller-supplied macro on each set
 * bit's index, with an optional context.  Include it again (after redefining the
 * control macros) to generate another iterator.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_BITSET_FOREACH_FUNC   per-bit callback (required; see below)
 *   RC_BITSET_FOREACH_CTX    context type threaded to the callback (optional)
 *   RC_BITSET_FOREACH_NAME   function name (optional; default rc_bitset_foreach)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Callback conventions
 * --------------------
 * Without RC_BITSET_FOREACH_CTX:
 *   RC_BITSET_FOREACH_FUNC(index)        - index is the uint32_t set-bit index.
 * With RC_BITSET_FOREACH_CTX:
 *   RC_BITSET_FOREACH_FUNC(ctx, index)   - ctx is a pointer to RC_BITSET_FOREACH_CTX
 *                                          and is the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  void NAME(const rc_bitset *bs)
 * With context:     void NAME(const rc_bitset *bs, CTX *ctx)
 *
 * Example (context):
 *   typedef struct { uint32_t count; } counter;
 *   static void bump(counter *c, uint32_t i) { (void)i; c->count++; }
 *
 *   #define RC_BITSET_FOREACH_CTX        counter
 *   #define RC_BITSET_FOREACH_FUNC(c, i) bump(c, i)
 *   #include "richc/template/algorithm/bitset_foreach.h"
 *   // void rc_bitset_foreach(const rc_bitset *bs, counter *ctx);
 */

#include "richc/bitset.h"

#ifndef RC_BITSET_FOREACH_FUNC
#  define RC_BITSET_FOREACH_FUNC(index) ((void)(index))   // to keep intellisense happy
#  error "RC_BITSET_FOREACH_FUNC must be defined before including richc/template/algorithm/bitset_foreach.h"
#endif

#ifndef RC_BITSET_FOREACH_NAME
#  define RC_BITSET_FOREACH_NAME rc_bitset_foreach
#endif

/*
 * Threading of the optional context, mirroring the other foreach templates:
 *   RC_BITSET_FOREACH_FUNC_(index)         - the callback as used in the body;
 *                                            closes over 'ctx' when active.
 *   RC_BITSET_FOREACH_BITSET_CTX_PARAM_    - the parameter list, "const rc_bitset
 *                                            *bs" or "..., CTX *ctx".
 */
#ifdef RC_BITSET_FOREACH_CTX
#  define RC_BITSET_FOREACH_FUNC_(index)      RC_BITSET_FOREACH_FUNC(ctx, index)
#  define RC_BITSET_FOREACH_BITSET_CTX_PARAM_ const rc_bitset *bs, RC_BITSET_FOREACH_CTX *ctx
#else
#  define RC_BITSET_FOREACH_FUNC_(index)      RC_BITSET_FOREACH_FUNC(index)
#  define RC_BITSET_FOREACH_BITSET_CTX_PARAM_ const rc_bitset *bs
#endif

/* Call RC_BITSET_FOREACH_FUNC on each set bit's index, in ascending order. */
static inline void RC_BITSET_FOREACH_NAME(RC_BITSET_FOREACH_BITSET_CTX_PARAM_)
{
    for (uint32_t i = rc_bitset_get_first_set(bs);
         i != RC_INDEX_NONE;
         i = rc_bitset_get_next_set(bs, i + 1)) {
        RC_BITSET_FOREACH_FUNC_(i);
    }
}

/* ---- cleanup ---- */

#undef RC_BITSET_FOREACH_FUNC_
#undef RC_BITSET_FOREACH_BITSET_CTX_PARAM_
#undef RC_BITSET_FOREACH_NAME
#undef RC_BITSET_FOREACH_CTX
#undef RC_BITSET_FOREACH_FUNC
