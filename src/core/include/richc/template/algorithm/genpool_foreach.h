/*
 * genpool_foreach.h - template header: visit the live entries of a genpool.
 *
 * A genpool offers no built-in iteration because the generation does not encode
 * liveness (it only advances on free).  This template bridges that: it builds a
 * bitset of the free-list (dead) slots in a scratch arena via the pool's
 * NAME_free_bitset, then walks the slots and invokes a caller-supplied macro
 * with each live element's handle (reconstructed via NAME_handle_at).  Include
 * it again (after redefining the control macros) to generate another iterator.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_GENPOOL_FOREACH_POOL   pool type name (required), e.g. rc_genpool_thing -
 *                             the same name passed to RC_GENPOOL_NAME; drives
 *                             the defaults
 *   RC_GENPOOL_FOREACH_FUNC   per-element callback (required; see below)
 *   RC_GENPOOL_FOREACH_CTX    context type threaded to the callback (optional)
 *   RC_GENPOOL_FOREACH_NAME   function name (optional; default <POOL>_foreach)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Callback conventions
 * --------------------
 * Each callback receives the pool pointer and the live element's handle (not a
 * raw element pointer), so it reaches the object through the pool's get/set/at -
 * the handle-over-pointer convention.
 * Without RC_GENPOOL_FOREACH_CTX:
 *   RC_GENPOOL_FOREACH_FUNC(pool, handle)       - pool is POOL *; handle is the
 *                                                 rc_genpool_handle of a live
 *                                                 element.
 * With RC_GENPOOL_FOREACH_CTX:
 *   RC_GENPOOL_FOREACH_FUNC(ctx, pool, handle)  - ctx is a pointer to
 *                                                 RC_GENPOOL_FOREACH_CTX, the
 *                                                 first argument.
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
 *   static void add_cost(sum_ctx *c, rc_genpool_thing *pool, rc_genpool_handle h)
 *   { c->total += rc_genpool_thing_at(pool, h)->cost; }
 *
 *   #define RC_GENPOOL_FOREACH_POOL          rc_genpool_thing
 *   #define RC_GENPOOL_FOREACH_CTX           sum_ctx
 *   #define RC_GENPOOL_FOREACH_FUNC(c, p, h) add_cost(c, p, h)
 *   #include "richc/template/algorithm/genpool_foreach.h"
 *   // void rc_genpool_thing_foreach(rc_genpool_thing *pool, sum_ctx *ctx, rc_arena scratch);
 */

#include "richc/arena.h"    // rc_arena (by value), RC_CONCAT
#include "richc/bitset.h"   // rc_bitset, rc_bitset_is_set

#ifndef RC_GENPOOL_FOREACH_POOL
#  define RC_GENPOOL_FOREACH_POOL rc_genpool_int   // to keep intellisense happy
#  error "RC_GENPOOL_FOREACH_POOL must be defined before including richc/template/algorithm/genpool_foreach.h"
#endif

#ifndef RC_GENPOOL_FOREACH_FUNC
#  define RC_GENPOOL_FOREACH_FUNC(pool, handle) ((void)(pool), (void)(handle))   // to keep intellisense happy
#  error "RC_GENPOOL_FOREACH_FUNC must be defined before including richc/template/algorithm/genpool_foreach.h"
#endif

#ifndef RC_GENPOOL_FOREACH_NAME
#  define RC_GENPOOL_FOREACH_NAME RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _foreach)
#endif

/*
 * Threading of the optional context, mirroring the sort/bounds templates:
 *   RC_GENPOOL_FOREACH_FUNC_(handle) - the callback as used in the body; closes
 *                                      over 'pool' (and 'ctx' when a context type
 *                                      is active).
 *   RC_GENPOOL_FOREACH_POOL_CTX_PARAM_ - the leading parameter list, "POOL *pool"
 *                                        or "POOL *pool, CTX *ctx".
 */
#ifdef RC_GENPOOL_FOREACH_CTX
#  define RC_GENPOOL_FOREACH_FUNC_(handle)   RC_GENPOOL_FOREACH_FUNC(ctx, pool, handle)
#  define RC_GENPOOL_FOREACH_POOL_CTX_PARAM_ RC_GENPOOL_FOREACH_POOL *pool, RC_GENPOOL_FOREACH_CTX *ctx
#else
#  define RC_GENPOOL_FOREACH_FUNC_(handle)   RC_GENPOOL_FOREACH_FUNC(pool, handle)
#  define RC_GENPOOL_FOREACH_POOL_CTX_PARAM_ RC_GENPOOL_FOREACH_POOL *pool
#endif

/* The free-list bitset and handle-reconstruction ops, named after the pool type. */
#define RC_GENPOOL_FOREACH_FREE_BITSET_ RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _free_bitset)
#define RC_GENPOOL_FOREACH_HANDLE_AT_   RC_CONCAT(RC_GENPOOL_FOREACH_POOL, _handle_at)

/*
 * Call RC_GENPOOL_FOREACH_FUNC with the pool and each live slot's handle.  The
 * dead-slot bitset's bit count equals the pool's slot count, so it doubles as the
 * loop bound; handle_at is only ever called on the clear (live) bits, satisfying
 * its live-slot restriction.
 */
static inline void RC_GENPOOL_FOREACH_NAME(RC_GENPOOL_FOREACH_POOL_CTX_PARAM_, rc_arena scratch)
{
    rc_bitset dead = RC_GENPOOL_FOREACH_FREE_BITSET_(pool, &scratch);
    for (uint32_t i = 0; i < dead.num; i++) {
        if (!rc_bitset_is_set(&dead, i)) {
            RC_GENPOOL_FOREACH_FUNC_(RC_GENPOOL_FOREACH_HANDLE_AT_(pool, i));
        }
    }
}

/* ---- cleanup ---- */

#undef RC_GENPOOL_FOREACH_FUNC_
#undef RC_GENPOOL_FOREACH_POOL_CTX_PARAM_
#undef RC_GENPOOL_FOREACH_FREE_BITSET_
#undef RC_GENPOOL_FOREACH_HANDLE_AT_
#undef RC_GENPOOL_FOREACH_NAME
#undef RC_GENPOOL_FOREACH_POOL
#undef RC_GENPOOL_FOREACH_CTX
#undef RC_GENPOOL_FOREACH_FUNC
