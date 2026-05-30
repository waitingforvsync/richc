/*
 * pool_foreach.h - template header: visit the live entries of an rc_pool.
 *
 * A pool offers no built-in iteration because a freed slot is byte-identical to a
 * live one.  This template bridges that: it builds a bitset of the free-list (dead)
 * slots in a scratch arena via the pool's NAME_free_bitset, then walks the slots
 * and invokes a caller-supplied macro on a pointer to each live element.  Include
 * it again (after redefining the control macros) to generate another iterator.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_POOL_FOREACH_TYPE   element type / suffix (required); drives the defaults
 *   RC_POOL_FOREACH_FUNC   per-element callback (required; see below)
 *   RC_POOL_FOREACH_CTX    context type threaded to the callback (optional)
 *   RC_POOL_FOREACH_POOL   pool type name (optional; default rc_pool_<TYPE>)
 *   RC_POOL_FOREACH_NAME   function name (optional; default rc_pool_foreach_<TYPE>)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Callback conventions
 * --------------------
 * Without RC_POOL_FOREACH_CTX:
 *   RC_POOL_FOREACH_FUNC(elem)        - elem is RC_POOL_TYPE *, pointing at the
 *                                       live element.
 * With RC_POOL_FOREACH_CTX:
 *   RC_POOL_FOREACH_FUNC(ctx, elem)   - ctx is a pointer to RC_POOL_FOREACH_CTX and
 *                                       is the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  void NAME(POOL *pool, rc_arena scratch)
 * With context:     void NAME(POOL *pool, CTX *ctx, rc_arena scratch)
 *
 * The scratch arena is taken BY VALUE (the standard scratch pattern): the bitset
 * is built in the caller's snapshot and discarded on return, so the caller's arena
 * is left untouched.
 *
 * Example (context):
 *   typedef struct { int total; } sum_ctx;
 *   static void add_cost(sum_ctx *c, thing *t) { c->total += t->cost; }
 *
 *   #define RC_POOL_FOREACH_TYPE       thing
 *   #define RC_POOL_FOREACH_CTX        sum_ctx
 *   #define RC_POOL_FOREACH_FUNC(c, e) add_cost(c, e)
 *   #include "richc/template/pool_foreach.h"
 *   // void rc_pool_foreach_thing(rc_pool_thing *pool, sum_ctx *ctx, rc_arena scratch);
 */

#include "richc/arena.h"    // rc_arena (by value), RC_CONCAT
#include "richc/bitset.h"   // rc_bitset, rc_bitset_is_set

#ifndef RC_POOL_FOREACH_TYPE
#  define RC_POOL_FOREACH_TYPE int   // to keep intellisense happy
#  error "RC_POOL_FOREACH_TYPE must be defined before including richc/template/pool_foreach.h"
#endif

#ifndef RC_POOL_FOREACH_FUNC
#  define RC_POOL_FOREACH_FUNC(elem) ((void)(elem))   // to keep intellisense happy
#  error "RC_POOL_FOREACH_FUNC must be defined before including richc/template/pool_foreach.h"
#endif

#ifndef RC_POOL_FOREACH_POOL
#  define RC_POOL_FOREACH_POOL RC_CONCAT(rc_pool_, RC_POOL_FOREACH_TYPE)
#endif

#ifndef RC_POOL_FOREACH_NAME
#  define RC_POOL_FOREACH_NAME RC_CONCAT(rc_pool_foreach_, RC_POOL_FOREACH_TYPE)
#endif

/*
 * Threading of the optional context, mirroring the sort/bounds templates:
 *   RC_POOL_FOREACH_FUNC_(elem)        - the callback as used in the body; closes
 *                                        over 'ctx' when a context type is active.
 *   RC_POOL_FOREACH_POOL_CTX_PARAM_    - the leading parameter list, "POOL *pool"
 *                                        or "POOL *pool, CTX *ctx".
 */
#ifdef RC_POOL_FOREACH_CTX
#  define RC_POOL_FOREACH_FUNC_(elem)     RC_POOL_FOREACH_FUNC(ctx, elem)
#  define RC_POOL_FOREACH_POOL_CTX_PARAM_ RC_POOL_FOREACH_POOL *pool, RC_POOL_FOREACH_CTX *ctx
#else
#  define RC_POOL_FOREACH_FUNC_(elem)     RC_POOL_FOREACH_FUNC(elem)
#  define RC_POOL_FOREACH_POOL_CTX_PARAM_ RC_POOL_FOREACH_POOL *pool
#endif

/* Pool operations, named after the pool type. */
#define RC_POOL_FOREACH_AT_          RC_CONCAT(RC_POOL_FOREACH_POOL, _at)
#define RC_POOL_FOREACH_FREE_BITSET_ RC_CONCAT(RC_POOL_FOREACH_POOL, _free_bitset)

/*
 * Call RC_POOL_FOREACH_FUNC on each live element.  The dead-slot bitset's bit
 * count equals the pool's slot count, so it doubles as the loop bound and the body
 * never touches the pool's internals.
 */
static inline void RC_POOL_FOREACH_NAME(RC_POOL_FOREACH_POOL_CTX_PARAM_, rc_arena scratch)
{
    rc_bitset dead = RC_POOL_FOREACH_FREE_BITSET_(pool, &scratch);
    for (uint32_t i = 0; i < dead.num; i++) {
        if (!rc_bitset_is_set(&dead, i)) {
            RC_POOL_FOREACH_FUNC_(RC_POOL_FOREACH_AT_(pool, i));
        }
    }
}

/* ---- cleanup ---- */

#undef RC_POOL_FOREACH_FUNC_
#undef RC_POOL_FOREACH_POOL_CTX_PARAM_
#undef RC_POOL_FOREACH_AT_
#undef RC_POOL_FOREACH_FREE_BITSET_
#undef RC_POOL_FOREACH_NAME
#undef RC_POOL_FOREACH_POOL
#undef RC_POOL_FOREACH_CTX
#undef RC_POOL_FOREACH_FUNC
#undef RC_POOL_FOREACH_TYPE
