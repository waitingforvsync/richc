/*
 * hash_trie.h - template header: 16-way hash trie.
 *
 * A trie where each node has 16 children, indexed by successive 4-bit groups of
 * a 64-bit key hash.  Nodes are stored 16 to a block; the blocks live in an
 * arena-backed pool that is just an rc_array of blocks (the array template does
 * the growth and zero-fill).  One pool can back many tries.
 *
 * Structure
 * ---------
 *   Node  - one slot in a block: { key, val, child_index }
 *   Block - 16 nodes (RC_TRIE_NAME_nodes); addressed by block index
 *   Pool  - rc_array of blocks (RC_TRIE_NAME_pool); shared across tries
 *   Trie  - { pool pointer, root block index } (RC_TRIE_NAME)
 *
 * Node encoding
 * -------------
 * child_index encodes both occupancy and children without a separate flag,
 * avoiding the padding an 'occupied' byte would cost:
 *
 *   child_index == 0                 - empty slot (no key, no children)
 *   child_index == UINT32_MAX        - occupied, no children (leaf)
 *   child_index in 1..UINT32_MAX-1   - occupied, children at that block index
 *
 * Block 0 is reserved as the empty-slot sentinel: it is the root block, stored
 * only in RC_TRIE_NAME.root, never as a child_index.  This works because make()
 * allocates block 0 before any add() runs, so every child block has index >= 1.
 *
 * Hash descent
 * ------------
 * At each depth the low 4 bits of the working hash select a slot (0-15) within
 * the current block.  The hash is then rotated right by 4 bits to bring the next
 * group into position.  After 16 levels all 64 bits have been consumed and the
 * rotation cycles.  Keys with identical 64-bit hashes still work: each new key
 * is placed one level deeper, forming an O(n)-deep chain (access degrades to
 * O(n) for n identical-hash keys, but every operation still terminates).
 *
 * Slot ownership invariant
 * ------------------------
 * Every occupied node with a child link is also a key-holder (no pure relay
 * nodes).  When key K lands in a slot already owned by K', K' stays and K is
 * pushed one level deeper.
 *
 * Deletion and bubble-up
 * ----------------------
 * Deleting a leaf sets child_index to 0 (empty).  Deleting an interior node
 * fills the vacancy by copying up the first occupied node from the child block
 * and then deleting that recursively.  If a child block becomes entirely empty
 * its link is cleared (child_index set to UINT32_MAX, making the parent a leaf
 * again).
 *
 * Lifecycle
 * ---------
 * Make a pool, then make one or more tries on it.  A zero-initialised trie is
 * not valid; always use make().  find(), add(), and delete() assert t->pool is
 * not NULL.
 *
 * Define before including:
 *   RC_TRIE_KEY_TYPE     key type (required)
 *   RC_TRIE_VALUE_TYPE   value type (required)
 *   RC_TRIE_HASH(k)      hash expression; cast to uint64_t (required)
 *   RC_TRIE_EQUAL(a, b)  key equality expression (optional; default (a) == (b))
 *   RC_TRIE_NAME         base name for the generated types and functions
 *                        (optional; default rc_trie_<RC_TRIE_KEY_TYPE>).  The
 *                        default requires a single-identifier key type; pass an
 *                        explicit name for multi-token key types.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Generated types
 * ---------------
 *   RC_TRIE_NAME_node  { RC_TRIE_KEY_TYPE key; RC_TRIE_VALUE_TYPE val; uint32_t child_index; }
 *   RC_TRIE_NAME_pool  arena-backed array of 16-node blocks; shared by tries
 *   RC_TRIE_NAME       { RC_TRIE_NAME_pool *pool; uint32_t root; }
 *
 * Generated functions (all static inline)
 * ----------------------------------------
 *   RC_TRIE_NAME_pool    NAME_pool_make(uint32_t min_blocks, rc_arena *arena)
 *   void                 NAME_pool_reserve(NAME_pool *pool, uint32_t min_blocks, rc_arena *arena)
 *   void                 NAME_pool_destroy(NAME_pool *pool, rc_arena *arena)
 *   RC_TRIE_NAME         NAME_make(NAME_pool *pool, rc_arena *arena)
 *   RC_TRIE_VALUE_TYPE  *NAME_find(NAME *t, RC_TRIE_KEY_TYPE key)
 *   bool                 NAME_add(NAME *t, RC_TRIE_KEY_TYPE key, RC_TRIE_VALUE_TYPE val, rc_arena *arena)
 *   bool                 NAME_delete(NAME *t, RC_TRIE_KEY_TYPE key)
 *
 * find() returns a pointer to the stored value, or NULL if absent; it is
 * invalidated by any later add that grows the pool.  add() returns true if the
 * key was new, false if an existing value was replaced.  delete() returns true
 * if the key was present.
 *
 * Example:
 *   #define RC_TRIE_KEY_TYPE   uint64_t
 *   #define RC_TRIE_VALUE_TYPE int
 *   #define RC_TRIE_HASH(k)    rc_hash_u64(k)
 *   #define RC_TRIE_NAME       rc_trie_u64
 *   #include "richc/template/hash_trie.h"
 *
 *   rc_arena arena = rc_arena_make_default();
 *
 *   // one pool can back many independent tries
 *   rc_trie_u64_pool pool = rc_trie_u64_pool_make(0, &arena);
 *   rc_trie_u64 a = rc_trie_u64_make(&pool, &arena);
 *   rc_trie_u64 b = rc_trie_u64_make(&pool, &arena);
 *
 *   rc_trie_u64_add(&a, 0xDEADBEEF, 42, &arena);   // true (new)
 *   int *v = rc_trie_u64_find(&a, 0xDEADBEEF);     // v && *v == 42
 *   rc_trie_u64_find(&b, 0xDEADBEEF);              // NULL: b is independent
 *
 *   rc_trie_u64_pool_destroy(&pool, &arena);
 */

#ifndef RC_TEMPLATE_HASH_TRIE_H_
#define RC_TEMPLATE_HASH_TRIE_H_

#include "richc/arena.h"   // also provides RC_CONCAT, RC_ASSERT, <stdint.h>, <stdbool.h>

/* Rotate a 64-bit hash right by 4 bits to expose the next 4-bit group. */
static inline uint64_t rc_trie_ror64_(uint64_t h)
{
    return (h >> 4) | (h << 60);
}

#endif /* RC_TEMPLATE_HASH_TRIE_H_ */

/* ===== per-type section ===== */

#ifndef RC_TRIE_KEY_TYPE
#  define RC_TRIE_KEY_TYPE int   // to keep intellisense happy
#  error "RC_TRIE_KEY_TYPE must be defined before including richc/template/hash_trie.h"
#endif
#ifndef RC_TRIE_VALUE_TYPE
#  define RC_TRIE_VALUE_TYPE int
#  error "RC_TRIE_VALUE_TYPE must be defined before including richc/template/hash_trie.h"
#endif
#ifndef RC_TRIE_HASH
#  define RC_TRIE_HASH(k) (k)
#  error "RC_TRIE_HASH must be defined before including richc/template/hash_trie.h"
#endif

#ifndef RC_TRIE_EQUAL
#  define RC_TRIE_EQUAL(a, b) ((a) == (b))
#endif

#ifndef RC_TRIE_NAME
#  define RC_TRIE_NAME RC_CONCAT(rc_trie_, RC_TRIE_KEY_TYPE)
#endif

/* ---- derived type-name macros ---- */

#define RC_TRIE_NODE_   RC_CONCAT(RC_TRIE_NAME, _node)
#define RC_TRIE_NODES_  RC_CONCAT(RC_TRIE_NAME, _nodes)
#define RC_TRIE_POOL_   RC_CONCAT(RC_TRIE_NAME, _pool)
#define RC_TRIE_ARRAY_  RC_CONCAT(rc_array_, RC_TRIE_NODES_)

/* ---- internal array-call macros ---- */

#define RC_TRIE_ARRAY_MAKE_        RC_CONCAT(RC_TRIE_ARRAY_, _make)
#define RC_TRIE_ARRAY_RESERVE_     RC_CONCAT(RC_TRIE_ARRAY_, _reserve)
#define RC_TRIE_ARRAY_PUSH_N_ZERO_ RC_CONCAT(RC_TRIE_ARRAY_, _push_n_zero)

/* ---- internal function-name macros ---- */

#define RC_TRIE_ALLOC_BLOCK_ RC_CONCAT(RC_TRIE_NAME, _alloc_block_)
#define RC_TRIE_BLOCK_EMPTY_ RC_CONCAT(RC_TRIE_NAME, _block_empty_)
#define RC_TRIE_DEL_FROM_    RC_CONCAT(RC_TRIE_NAME, _del_from_)

/* ---- public function-name macros ---- */

#define RC_TRIE_POOL_MAKE_    RC_CONCAT(RC_TRIE_NAME, _pool_make)
#define RC_TRIE_POOL_RESERVE_ RC_CONCAT(RC_TRIE_NAME, _pool_reserve)
#define RC_TRIE_POOL_DESTROY_ RC_CONCAT(RC_TRIE_NAME, _pool_destroy)
#define RC_TRIE_MAKE_         RC_CONCAT(RC_TRIE_NAME, _make)
#define RC_TRIE_FIND_         RC_CONCAT(RC_TRIE_NAME, _find)
#define RC_TRIE_ADD_          RC_CONCAT(RC_TRIE_NAME, _add)
#define RC_TRIE_DELETE_       RC_CONCAT(RC_TRIE_NAME, _delete)

/* ---- generated types ---- */

/*
 * Node: one slot in a 16-node block.  See the file header for the child_index
 * encoding (0 empty, UINT32_MAX leaf, else child block index).
 */
typedef struct RC_TRIE_NODE_ {
    RC_TRIE_KEY_TYPE   key;
    RC_TRIE_VALUE_TYPE val;
    uint32_t           child_index;
} RC_TRIE_NODE_;

/* Block: 16 consecutive nodes; the element type of the pool array. */
typedef struct RC_TRIE_NODES_ {
    RC_TRIE_NODE_ node[16];
} RC_TRIE_NODES_;

/* Pool: an arena-backed array of blocks (one block == one trie level). */
#define RC_ARRAY_TYPE RC_TRIE_NODES_
#define RC_ARRAY_NAME RC_TRIE_NODES_
#include "richc/template/array.h"

typedef RC_TRIE_ARRAY_ RC_TRIE_POOL_;

/*
 * Trie: a 16-way hash trie backed by a node pool.  root is the block index of
 * the root block; always valid after make().  Do NOT zero-initialise.
 */
typedef struct RC_TRIE_NAME {
    RC_TRIE_POOL_ *pool;
    uint32_t       root;
} RC_TRIE_NAME;

/* ---- pool lifecycle ---- */

/* Create a pool, optionally pre-reserving min_blocks blocks (0 for none). */
static inline RC_TRIE_POOL_ RC_TRIE_POOL_MAKE_(uint32_t min_blocks, rc_arena *arena)
{
    return RC_TRIE_ARRAY_MAKE_(min_blocks, arena);
}

/* Ensure the pool has capacity for at least min_blocks blocks (exact reserve). */
static inline void RC_TRIE_POOL_RESERVE_(RC_TRIE_POOL_ *pool, uint32_t min_blocks, rc_arena *arena)
{
    RC_TRIE_ARRAY_RESERVE_(pool, min_blocks, arena);
}

/*
 * Release the pool's backing (best-effort: rc_arena_free reclaims it only if it
 * is the arena's most recent allocation; otherwise it is reclaimed on arena
 * reset) and reset the pool to empty.  Any trie still referencing the pool must
 * not be used afterwards.
 */
static inline void RC_TRIE_POOL_DESTROY_(RC_TRIE_POOL_ *pool, rc_arena *arena)
{
    RC_ASSERT(arena != NULL);
    rc_arena_free(arena, pool->data, (uint32_t)((size_t)pool->cap * sizeof(RC_TRIE_NODES_)));
    *pool = (RC_TRIE_POOL_){0};
}

/* ---- internal: allocate a fresh, zeroed 16-node block ---- */

/*
 * Append one zeroed block to the pool and return its block index.  Zeroing sets
 * child_index = 0 (empty) for every node.  May reallocate pool->data, so callers
 * must re-derive any node pointers afterwards.
 */
static inline uint32_t RC_TRIE_ALLOC_BLOCK_(RC_TRIE_POOL_ *pool, rc_arena *arena)
{
    return RC_TRIE_ARRAY_PUSH_N_ZERO_(pool, 1, arena);
}

/* ---- internal: is a block entirely empty? ---- */

static inline bool RC_TRIE_BLOCK_EMPTY_(RC_TRIE_NODES_ *block)
{
    for (uint32_t i = 0; i < 16; i++) {
        if (block->node[i].child_index != 0) {
            return false;
        }
    }
    return true;
}

/* ---- trie make ---- */

/* Make a trie backed by pool, allocating its own root block.  pool/arena != NULL. */
static inline RC_TRIE_NAME RC_TRIE_MAKE_(RC_TRIE_POOL_ *pool, rc_arena *arena)
{
    RC_ASSERT(pool != NULL);
    RC_ASSERT(arena != NULL);
    RC_TRIE_NAME t;
    t.pool = pool;
    t.root = RC_TRIE_ALLOC_BLOCK_(pool, arena);
    return t;
}

/* ---- find ---- */

/* Pointer to the stored value for key, or NULL if absent.  t->pool != NULL. */
static inline RC_TRIE_VALUE_TYPE *RC_TRIE_FIND_(RC_TRIE_NAME *t, RC_TRIE_KEY_TYPE key)
{
    RC_ASSERT(t->pool != NULL);

    uint64_t h         = (uint64_t)RC_TRIE_HASH(key);
    uint32_t block_idx = t->root;

    for (;;) {
        uint32_t slot = (uint32_t)(h & 0xF);
        h = rc_trie_ror64_(h);

        RC_TRIE_NODE_ *node = &t->pool->data[block_idx].node[slot];

        if (node->child_index == 0)          return NULL;   // empty slot
        if (RC_TRIE_EQUAL(node->key, key))   return &node->val;
        if (node->child_index == UINT32_MAX) return NULL;   // leaf, no match
        block_idx = node->child_index;
    }
}

/* ---- add ---- */

/*
 * Insert or update.  Returns true if the key was new, false if an existing value
 * was replaced.  t->pool and arena must not be NULL.
 *
 * When a slot is already owned by a different key, that key stays and the new
 * key descends into children; a child block is allocated on demand.  Because
 * the alloc may reallocate pool->data, the child link is written through a freshly
 * re-derived pool->data slot, never through the cached node pointer.
 */
static inline bool RC_TRIE_ADD_(RC_TRIE_NAME *t, RC_TRIE_KEY_TYPE key, RC_TRIE_VALUE_TYPE val, rc_arena *arena)
{
    RC_ASSERT(t->pool != NULL);
    RC_ASSERT(arena != NULL);

    uint64_t h         = (uint64_t)RC_TRIE_HASH(key);
    uint32_t block_idx = t->root;

    for (;;) {
        uint32_t slot = (uint32_t)(h & 0xF);
        h = rc_trie_ror64_(h);

        RC_TRIE_NODE_ *node = &t->pool->data[block_idx].node[slot];

        if (node->child_index == 0) {
            // empty slot: place key here as a leaf
            node->key         = key;
            node->val         = val;
            node->child_index = UINT32_MAX;
            return true;
        }

        if (RC_TRIE_EQUAL(node->key, key)) {
            node->val = val;
            return false;
        }

        if (node->child_index == UINT32_MAX) {
            // first collision here: allocate a child block (may realloc the pool)
            uint32_t child_idx = RC_TRIE_ALLOC_BLOCK_(t->pool, arena);
            t->pool->data[block_idx].node[slot].child_index = child_idx;
            block_idx = child_idx;
        }
        else {
            block_idx = node->child_index;
        }
    }
}

/* ---- internal: recursive delete ---- */

/*
 * Delete key from block block_idx at trie depth `depth`.  Returns true if the
 * key was found and removed.
 *
 * After recursing into a child block, the caller checks whether that block has
 * become entirely empty and, if so, unlinks it (resetting this node to a leaf).
 * The caller can do this itself because it holds the child block index - no
 * out-parameter is needed.  This upholds the invariant that an interior node
 * always points to a non-empty child block, which the bubble-up below relies on.
 * No arena needed (delete never grows the pool, so node pointers stay valid
 * across the recursion).
 */
static inline bool RC_TRIE_DEL_FROM_(RC_TRIE_POOL_ *pool, uint32_t block_idx,
                                     RC_TRIE_KEY_TYPE key, uint32_t depth)
{
    // recompute the hash rotation state for this depth
    uint64_t h = (uint64_t)RC_TRIE_HASH(key);
    for (uint32_t i = 0; i < depth; i++) {
        h = rc_trie_ror64_(h);
    }

    uint32_t       slot = (uint32_t)(h & 0xF);
    RC_TRIE_NODE_ *node = &pool->data[block_idx].node[slot];

    if (node->child_index == 0) {
        return false;   // empty slot: key not present
    }

    if (!RC_TRIE_EQUAL(node->key, key)) {
        // key is not the owner of this slot; descend into children
        if (node->child_index == UINT32_MAX) {
            return false;
        }
        uint32_t child_idx = node->child_index;
        bool found = RC_TRIE_DEL_FROM_(pool, child_idx, key, depth + 1);
        if (found && RC_TRIE_BLOCK_EMPTY_(&pool->data[child_idx])) {
            pool->data[block_idx].node[slot].child_index = UINT32_MAX;
        }
        return found;
    }

    // found the key at this node

    if (node->child_index == UINT32_MAX) {
        node->child_index = 0;   // leaf: mark the slot empty
        return true;
    }

    // interior node: bubble up the first occupied slot from the child block
    uint32_t       child_block_idx = node->child_index;
    RC_TRIE_NODE_ *child_blk       = pool->data[child_block_idx].node;
    uint32_t       r               = UINT32_MAX;

    for (uint32_t i = 0; i < 16; i++) {
        if (child_blk[i].child_index != 0) { r = i; break; }
    }
    RC_ASSERT(r != UINT32_MAX);   // interior node must point to a non-empty child block

    RC_TRIE_KEY_TYPE   rep_key = child_blk[r].key;
    RC_TRIE_VALUE_TYPE rep_val = child_blk[r].val;

    node->key = rep_key;
    node->val = rep_val;
    // node->child_index (the child block) is unchanged

    RC_TRIE_DEL_FROM_(pool, child_block_idx, rep_key, depth + 1);
    if (RC_TRIE_BLOCK_EMPTY_(&pool->data[child_block_idx])) {
        pool->data[block_idx].node[slot].child_index = UINT32_MAX;
    }
    return true;
}

/* ---- delete ---- */

/* Remove key.  Returns true if the key was present.  t->pool != NULL. */
static inline bool RC_TRIE_DELETE_(RC_TRIE_NAME *t, RC_TRIE_KEY_TYPE key)
{
    RC_ASSERT(t->pool != NULL);
    return RC_TRIE_DEL_FROM_(t->pool, t->root, key, 0);
}

/* ---- cleanup ---- */

#undef RC_TRIE_NODE_
#undef RC_TRIE_NODES_
#undef RC_TRIE_POOL_
#undef RC_TRIE_ARRAY_
#undef RC_TRIE_ARRAY_MAKE_
#undef RC_TRIE_ARRAY_RESERVE_
#undef RC_TRIE_ARRAY_PUSH_N_ZERO_
#undef RC_TRIE_ALLOC_BLOCK_
#undef RC_TRIE_BLOCK_EMPTY_
#undef RC_TRIE_DEL_FROM_
#undef RC_TRIE_POOL_MAKE_
#undef RC_TRIE_POOL_RESERVE_
#undef RC_TRIE_POOL_DESTROY_
#undef RC_TRIE_MAKE_
#undef RC_TRIE_FIND_
#undef RC_TRIE_ADD_
#undef RC_TRIE_DELETE_

#undef RC_TRIE_EQUAL
#undef RC_TRIE_NAME
#undef RC_TRIE_KEY_TYPE
#undef RC_TRIE_VALUE_TYPE
#undef RC_TRIE_HASH
