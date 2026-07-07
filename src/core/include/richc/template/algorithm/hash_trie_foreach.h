/*
 * hash_trie_foreach.h - template header: visit every entry of an rc_trie.
 *
 * A hash trie offers no built-in iteration.  This template bridges that: it walks
 * the trie from its root over the 16-node blocks and invokes a caller-supplied
 * macro on each live entry, handing over the POOL and the entry's node index (the
 * same block*16+slot index find returns), so the callback reaches the key and value
 * through the trie's key_get / value_get (which take the pool) - the index-over-pointer
 * convention.  Include it again (after redefining the control macros) to generate
 * another iterator.
 *
 * The trie is passed by value and the pool explicitly, matching the trie API; an
 * empty trie (root == 0) visits nothing.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_TRIE_FOREACH_TRIE   trie type name (required), e.g. rc_trie_symbol - the same
 *                          name passed to RC_TRIE_NAME; drives the defaults
 *   RC_TRIE_FOREACH_FUNC   per-entry callback (required; see below)
 *   RC_TRIE_FOREACH_CTX    context type threaded to the callback (optional)
 *   RC_TRIE_FOREACH_CONST  define for a read-only walk (const pool); optional
 *   RC_TRIE_FOREACH_NAME   function name (optional; default <TRIE>_foreach)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Const or mutable
 * ----------------
 * By default the pool is MUTABLE, so a callback may update entries in place with
 * value_set (the walk never adds or deletes, so in-place updates are safe).  Define
 * RC_TRIE_FOREACH_CONST for a read-only walk: the pool arrives as a const pointer,
 * so a const owner can iterate without casting, but the callback may only read (via
 * key_get / value_get) - accumulate any output into the mutable CTX.
 *
 * Callback conventions
 * --------------------
 * Each callback receives the POOL pointer and a live entry's node index (not a raw
 * node pointer), so it reaches the key/value through the trie's key_get / value_get
 * - the index-over-pointer convention.  The pool pointer is const iff
 * RC_TRIE_FOREACH_CONST is defined.
 * Without RC_TRIE_FOREACH_CTX:
 *   RC_TRIE_FOREACH_FUNC(pool, index)       - pool is [const] POOL *; index is the
 *                                             node index of a live entry.
 * With RC_TRIE_FOREACH_CTX:
 *   RC_TRIE_FOREACH_FUNC(ctx, pool, index)  - ctx is a pointer to
 *                                             RC_TRIE_FOREACH_CTX, the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  void NAME(TRIE t, [const] TRIE_pool *pool)
 * With context:     void NAME(TRIE t, [const] TRIE_pool *pool, CTX *ctx)
 *
 * The entries are visited in an unspecified order (a trie is unordered).  Unlike
 * pool_foreach, no scratch arena is needed: a reachability walk from the root
 * allocates nothing.  Do not add or delete keys during iteration.
 *
 * Example (read-only context walk):
 *   typedef struct { uint32_t count; } tally;
 *   static void tally_one(tally *c, const rc_trie_u64_pool *pool, uint32_t i) { (void)pool; (void)i; c->count++; }
 *
 *   #define RC_TRIE_FOREACH_TRIE          rc_trie_u64
 *   #define RC_TRIE_FOREACH_CTX           tally
 *   #define RC_TRIE_FOREACH_CONST
 *   #define RC_TRIE_FOREACH_FUNC(c, p, i) tally_one(c, p, i)
 *   #include "richc/template/algorithm/hash_trie_foreach.h"
 *   // void rc_trie_u64_foreach(rc_trie_u64 t, const rc_trie_u64_pool *pool, tally *ctx);
 */

#include "richc/macros.h"   // RC_CONCAT, RC_ASSERT

#ifndef RC_TRIE_FOREACH_TRIE
#  define RC_TRIE_FOREACH_TRIE rc_trie_int   // to keep intellisense happy
#  error "RC_TRIE_FOREACH_TRIE must be defined before including richc/template/algorithm/hash_trie_foreach.h"
#endif

#ifndef RC_TRIE_FOREACH_FUNC
#  define RC_TRIE_FOREACH_FUNC(pool, index) ((void)(pool), (void)(index))   // to keep intellisense happy
#  error "RC_TRIE_FOREACH_FUNC must be defined before including richc/template/algorithm/hash_trie_foreach.h"
#endif

#ifndef RC_TRIE_FOREACH_NAME
#  define RC_TRIE_FOREACH_NAME RC_CONCAT(RC_TRIE_FOREACH_TRIE, _foreach)
#endif

/* The block type and pool type, named after the trie type. */
#define RC_TRIE_FOREACH_NODES_    RC_CONCAT(RC_TRIE_FOREACH_TRIE, _nodes)
#define RC_TRIE_FOREACH_POOL_     RC_CONCAT(RC_TRIE_FOREACH_TRIE, _pool)
#define RC_TRIE_FOREACH_BLOCK_    RC_CONCAT(RC_TRIE_FOREACH_NAME, _block_)

/*
 * Const-ness of the walk is the caller's choice.  By default the pool is mutable, so
 * a callback may update values in place (value_set); defining RC_TRIE_FOREACH_CONST
 * makes it a read-only walk - a const pool, read through the pool's _at_const
 * accessor, so a const owner can iterate without casting.  RC_TRIE_FOREACH_POOL_QUAL_
 * is the qualifier ('const' or nothing) applied to the pool pointer and the block
 * pointer; RC_TRIE_FOREACH_POOL_AT_ is the matching pool accessor.  Either way the
 * walk itself only reads structure - it never adds or deletes keys.
 */
#ifdef RC_TRIE_FOREACH_CONST
#  define RC_TRIE_FOREACH_POOL_QUAL_ const
#  define RC_TRIE_FOREACH_POOL_AT_   RC_CONCAT(RC_TRIE_FOREACH_TRIE, _pool_at_const)
#else
#  define RC_TRIE_FOREACH_POOL_QUAL_
#  define RC_TRIE_FOREACH_POOL_AT_   RC_CONCAT(RC_TRIE_FOREACH_TRIE, _pool_at)
#endif

/*
 * Threading of the optional context, mirroring pool_foreach:
 *   RC_TRIE_FOREACH_FUNC_(index) - the callback as used in the body; closes over
 *                                  'pool' (and 'ctx' when a context type is active).
 *   RC_TRIE_FOREACH_PARAM_ - the trailing parameter list, "[const] POOL *pool" or
 *                            "[const] POOL *pool, CTX *ctx".
 *   RC_TRIE_FOREACH_ARG_   - the matching argument list for the recursive call.
 */
#ifdef RC_TRIE_FOREACH_CTX
#  define RC_TRIE_FOREACH_FUNC_(index) RC_TRIE_FOREACH_FUNC(ctx, pool, index)
#  define RC_TRIE_FOREACH_PARAM_       RC_TRIE_FOREACH_POOL_QUAL_ RC_TRIE_FOREACH_POOL_ *pool, RC_TRIE_FOREACH_CTX *ctx
#  define RC_TRIE_FOREACH_ARG_         pool, ctx
#else
#  define RC_TRIE_FOREACH_FUNC_(index) RC_TRIE_FOREACH_FUNC(pool, index)
#  define RC_TRIE_FOREACH_PARAM_       RC_TRIE_FOREACH_POOL_QUAL_ RC_TRIE_FOREACH_POOL_ *pool
#  define RC_TRIE_FOREACH_ARG_         pool
#endif

/*
 * Recurse over one 16-node block: visit every occupied node (each one holds a key -
 * there are no pure relay nodes) and descend into any child block.  child_index is 0
 * for an empty slot, UINT32_MAX for a leaf, else the child block's index.  The walk
 * allocates nothing, so the block pointer stays valid across the recursive call.
 */
static inline void RC_TRIE_FOREACH_BLOCK_(RC_TRIE_FOREACH_PARAM_, uint32_t block_idx)
{
    RC_TRIE_FOREACH_POOL_QUAL_ RC_TRIE_FOREACH_NODES_ *block = RC_TRIE_FOREACH_POOL_AT_(pool, block_idx);
    for (uint32_t slot = 0; slot < 16; slot++) {
        uint32_t child_index = block->node[slot].child_index;
        if (child_index == 0) {
            continue;   // empty slot
        }
        RC_TRIE_FOREACH_FUNC_(block_idx * 16 + slot);   // live entry: hand over its node index
        if (child_index != UINT32_MAX) {
            RC_TRIE_FOREACH_BLOCK_(RC_TRIE_FOREACH_ARG_, child_index);   // interior node: descend
        }
    }
}

/* Visit every live entry of the trie, in unspecified order.  An empty trie visits nothing. */
static inline void RC_TRIE_FOREACH_NAME(RC_TRIE_FOREACH_TRIE t, RC_TRIE_FOREACH_PARAM_)
{
    RC_ASSERT(pool != NULL);
    if (t.root == 0) {
        return;   // empty trie: no root block
    }
    RC_TRIE_FOREACH_BLOCK_(RC_TRIE_FOREACH_ARG_, t.root - 1);
}

/* ---- cleanup ---- */

#undef RC_TRIE_FOREACH_FUNC_
#undef RC_TRIE_FOREACH_PARAM_
#undef RC_TRIE_FOREACH_ARG_
#undef RC_TRIE_FOREACH_NODES_
#undef RC_TRIE_FOREACH_POOL_
#undef RC_TRIE_FOREACH_POOL_QUAL_
#undef RC_TRIE_FOREACH_POOL_AT_
#undef RC_TRIE_FOREACH_BLOCK_
#undef RC_TRIE_FOREACH_NAME
#undef RC_TRIE_FOREACH_TRIE
#undef RC_TRIE_FOREACH_CONST
#undef RC_TRIE_FOREACH_CTX
#undef RC_TRIE_FOREACH_FUNC
