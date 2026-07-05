/*
 * hash_trie_foreach.h - template header: visit every entry of an rc_trie.
 *
 * A hash trie offers no built-in iteration.  This template bridges that: it walks
 * the trie from its root over the 16-node blocks and invokes a caller-supplied
 * macro on each live entry, handing over the trie and the entry's node index (the
 * same block*16+slot index find returns), so the callback reaches the key and value
 * through the trie's key_get / value_get - the index-over-pointer convention.
 * Include it again (after redefining the control macros) to generate another
 * iterator.
 *
 * Control macros (define before including)
 * ----------------------------------------
 *   RC_TRIE_FOREACH_TYPE   trie suffix (required); drives the defaults
 *   RC_TRIE_FOREACH_FUNC   per-entry callback (required; see below)
 *   RC_TRIE_FOREACH_CTX    context type threaded to the callback (optional)
 *   RC_TRIE_FOREACH_TRIE   trie type name (optional; default rc_trie_<TYPE>)
 *   RC_TRIE_FOREACH_NAME   function name (optional; default rc_trie_foreach_<TYPE>)
 *
 * All macros defined before inclusion are undefined again by this header.
 *
 * Callback conventions
 * --------------------
 * Each callback receives the trie pointer and a live entry's node index (not a raw
 * node pointer), so it reaches the key/value through the trie's key_get / value_get
 * - the index-over-pointer convention.
 * Without RC_TRIE_FOREACH_CTX:
 *   RC_TRIE_FOREACH_FUNC(trie, index)       - trie is TRIE *; index is the node
 *                                             index of a live entry.
 * With RC_TRIE_FOREACH_CTX:
 *   RC_TRIE_FOREACH_FUNC(ctx, trie, index)  - ctx is a pointer to
 *                                             RC_TRIE_FOREACH_CTX, the first argument.
 *
 * Generated function signature
 * ----------------------------
 * Without context:  void NAME(TRIE *t)
 * With context:     void NAME(TRIE *t, CTX *ctx)
 *
 * The entries are visited in an unspecified order (a trie is unordered).  Unlike
 * pool_foreach, no scratch arena is needed: a reachability walk from the root
 * allocates nothing.  Do not add or delete keys during iteration.
 *
 * Example (context):
 *   typedef struct { uint32_t count; } tally;
 *   static void tally_one(tally *c, rc_trie_u64 *t, uint32_t i) { (void)t; (void)i; c->count++; }
 *
 *   #define RC_TRIE_FOREACH_TYPE          u64
 *   #define RC_TRIE_FOREACH_CTX           tally
 *   #define RC_TRIE_FOREACH_FUNC(c, t, i) tally_one(c, t, i)
 *   #include "richc/template/algorithm/hash_trie_foreach.h"
 *   // void rc_trie_foreach_u64(rc_trie_u64 *t, tally *ctx);
 */

#include "richc/macros.h"   // RC_CONCAT, RC_ASSERT

#ifndef RC_TRIE_FOREACH_TYPE
#  define RC_TRIE_FOREACH_TYPE int   // to keep intellisense happy
#  error "RC_TRIE_FOREACH_TYPE must be defined before including richc/template/algorithm/hash_trie_foreach.h"
#endif

#ifndef RC_TRIE_FOREACH_FUNC
#  define RC_TRIE_FOREACH_FUNC(trie, index) ((void)(trie), (void)(index))   // to keep intellisense happy
#  error "RC_TRIE_FOREACH_FUNC must be defined before including richc/template/algorithm/hash_trie_foreach.h"
#endif

#ifndef RC_TRIE_FOREACH_TRIE
#  define RC_TRIE_FOREACH_TRIE RC_CONCAT(rc_trie_, RC_TRIE_FOREACH_TYPE)
#endif

#ifndef RC_TRIE_FOREACH_NAME
#  define RC_TRIE_FOREACH_NAME RC_CONCAT(rc_trie_foreach_, RC_TRIE_FOREACH_TYPE)
#endif

/* The block type and the pool's element accessor, named after the trie type. */
#define RC_TRIE_FOREACH_NODES_   RC_CONCAT(RC_TRIE_FOREACH_TRIE, _nodes)
#define RC_TRIE_FOREACH_POOL_AT_ RC_CONCAT(RC_TRIE_FOREACH_TRIE, _pool_at)
#define RC_TRIE_FOREACH_BLOCK_   RC_CONCAT(RC_TRIE_FOREACH_NAME, _block_)

/*
 * Threading of the optional context, mirroring pool_foreach:
 *   RC_TRIE_FOREACH_FUNC_(index) - the callback as used in the body; closes over
 *                                  't' (and 'ctx' when a context type is active).
 *   RC_TRIE_FOREACH_PARAM_ - the leading parameter list, "TRIE *t" or
 *                            "TRIE *t, CTX *ctx".
 *   RC_TRIE_FOREACH_ARG_   - the matching argument list for the recursive call.
 */
#ifdef RC_TRIE_FOREACH_CTX
#  define RC_TRIE_FOREACH_FUNC_(index) RC_TRIE_FOREACH_FUNC(ctx, t, index)
#  define RC_TRIE_FOREACH_PARAM_       RC_TRIE_FOREACH_TRIE *t, RC_TRIE_FOREACH_CTX *ctx
#  define RC_TRIE_FOREACH_ARG_         t, ctx
#else
#  define RC_TRIE_FOREACH_FUNC_(index) RC_TRIE_FOREACH_FUNC(t, index)
#  define RC_TRIE_FOREACH_PARAM_       RC_TRIE_FOREACH_TRIE *t
#  define RC_TRIE_FOREACH_ARG_         t
#endif

/*
 * Recurse over one 16-node block: visit every occupied node (each one holds a key -
 * there are no pure relay nodes) and descend into any child block.  child_index is 0
 * for an empty slot, UINT32_MAX for a leaf, else the child block's index.  The walk
 * allocates nothing, so the block pointer stays valid across the recursive call.
 */
static inline void RC_TRIE_FOREACH_BLOCK_(RC_TRIE_FOREACH_PARAM_, uint32_t block_idx)
{
    RC_TRIE_FOREACH_NODES_ *block = RC_TRIE_FOREACH_POOL_AT_(t->pool, block_idx);
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

/* Visit every live entry of the trie, in unspecified order. */
static inline void RC_TRIE_FOREACH_NAME(RC_TRIE_FOREACH_PARAM_)
{
    RC_ASSERT(t->pool != NULL);
    RC_TRIE_FOREACH_BLOCK_(RC_TRIE_FOREACH_ARG_, t->root);
}

/* ---- cleanup ---- */

#undef RC_TRIE_FOREACH_FUNC_
#undef RC_TRIE_FOREACH_PARAM_
#undef RC_TRIE_FOREACH_ARG_
#undef RC_TRIE_FOREACH_NODES_
#undef RC_TRIE_FOREACH_POOL_AT_
#undef RC_TRIE_FOREACH_BLOCK_
#undef RC_TRIE_FOREACH_NAME
#undef RC_TRIE_FOREACH_TRIE
#undef RC_TRIE_FOREACH_CTX
#undef RC_TRIE_FOREACH_FUNC
#undef RC_TRIE_FOREACH_TYPE
