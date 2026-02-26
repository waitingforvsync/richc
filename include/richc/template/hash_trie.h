/*
 * hash_trie.h - template header: 16-way hash trie.
 *
 * A trie where each node has 16 children, indexed by successive 4-bit
 * groups of a 64-bit key hash.  Nodes are stored in a flat arena-backed
 * pool allocated 16 at a time (one block per trie level).  Multiple
 * tries can share the same pool.
 *
 * Structure
 * ---------
 * Node   — one slot in a 16-node block: {key, val, child_index}
 * Block  — 16 consecutive nodes in the pool; addressed by block index
 * Pool   — growing flat array of nodes (rc_trie_T_pool)
 * Trie   — {pool pointer, root block index} (rc_trie_T)
 *
 * Node encoding
 * -------------
 * child_index encodes both occupancy and children without a separate flag,
 * avoiding the padding an 'occupied' byte would cause:
 *
 *   child_index == 0               — slot is empty (no key, no children)
 *   child_index == UINT32_MAX      — occupied, no children (leaf)
 *   child_index in 1..UINT32_MAX-1 — occupied, children at block index
 *
 * Block 0 is reserved as the empty-slot sentinel: it is stored only in
 * TRIE_NAME.root (the root block), never as a child_index.  This works
 * because create() pre-allocates block 0 before any add() runs, so every
 * child block allocated by add() receives index >= 1.
 *
 * Hash descent
 * ------------
 * At each depth the low 4 bits of the working hash select a slot (0-15)
 * within the current block.  The hash is then rotated right by 4 bits to
 * bring the next group into position.  After 16 levels all 64 bits have
 * been consumed and the rotation cycles.
 *
 * Because freshly allocated blocks are zeroed (all slots empty), keys with
 * identical 64-bit hashes are handled correctly: each new key is placed one
 * level deeper than the last, forming an O(n)-deep chain.  All operations
 * still terminate; access degrades to O(n) for n identical-hash keys.
 *
 * Slot ownership invariant
 * ------------------------
 * Every occupied node with a child link is also a key-holder (no pure relay
 * nodes).  When a key K is inserted into a slot already owned by K', K'
 * stays and K is pushed one level deeper into K's children.
 *
 * Deletion and bubble-up
 * ----------------------
 * Deleting a leaf sets child_index to 0 (empty).  Deleting an interior node
 * requires filling the vacancy: the first occupied node found in the
 * immediate child block is copied up and then deleted recursively.  After
 * any deletion, if a child block becomes entirely empty its link is cleared
 * (child_index set to UINT32_MAX, making the parent a leaf again).
 *
 * Lifecycle
 * ---------
 * Zero-initialise the pool, then call create() to initialise a trie and
 * pre-allocate its root block.  A zero-initialised TRIE_NAME is not valid;
 * always use create().  find(), add(), and delete() all assert that
 * t->pool is not NULL.
 *
 * Define before including:
 *   TRIE_KEY_T      key type (required)
 *   TRIE_VAL_T      value type (required)
 *   TRIE_HASH(k)    hash expression; cast to uint64_t (required)
 *   TRIE_EQUAL(a,b) equality expression for two keys
 *                   (optional; default: (a) == (b))
 *   TRIE_NAME       base name for all generated types and functions
 *                   (optional; default: rc_trie_##TRIE_KEY_T)
 *                   Must be provided when instantiating multiple tries
 *                   for the same key type.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Generated types
 * ---------------
 *   TRIE_NAME_node  { TRIE_KEY_T key; TRIE_VAL_T val; uint32_t child_index; }
 *
 *   TRIE_NAME_pool  { TRIE_NAME_node *data;
 *                     uint32_t num; uint32_t cap; }
 *        Zero-initialise to get an empty, valid pool.
 *
 *   TRIE_NAME       { TRIE_NAME_pool *pool; uint32_t root; }
 *        Initialise with NAME_create(); do not zero-initialise directly.
 *
 * Generated functions (all static inline)
 * ----------------------------------------
 *
 *   TRIE_NAME NAME_create(TRIE_NAME_pool *pool, rc_arena *a)
 *        Initialise a trie backed by pool, pre-allocating its root block.
 *        pool and a must not be NULL.
 *
 *   TRIE_VAL_T *NAME_find(TRIE_NAME *t, TRIE_KEY_T key)
 *        Returns a pointer to the stored value, or NULL if absent.
 *        Invalidated by any subsequent add that grows the pool.
 *        t->pool must not be NULL.
 *
 *   int NAME_add(TRIE_NAME *t, TRIE_KEY_T key, TRIE_VAL_T val, rc_arena *a)
 *        Insert or update.  Returns 1 if key was new, 0 if updated.
 *        t->pool and a must not be NULL.
 *
 *   int NAME_delete(TRIE_NAME *t, TRIE_KEY_T key)
 *        Remove key.  Returns 1 if present, 0 if absent.  No arena needed.
 *        t->pool must not be NULL.
 *
 * Example:
 *   #define TRIE_KEY_T  uint64_t
 *   #define TRIE_VAL_T  int
 *   #define TRIE_HASH(k) (k)
 *   #define TRIE_NAME   U64Trie
 *   #include "richc/hash_trie.h"
 *
 *   rc_arena     arena = rc_arena_make_default();
 *   U64Trie_pool pool  = {0};
 *   U64Trie      t     = U64Trie_create(&pool, &arena);
 *   U64Trie_add(&t, 0xDEADBEEFu, 42, &arena);
 *   int *v = U64Trie_find(&t, 0xDEADBEEFu);   // v != NULL, *v == 42
 */

#include <stdint.h>
#include <string.h>
#include "richc/template_util.h"
#include "richc/arena.h"

#ifndef TRIE_KEY_T
#  error "TRIE_KEY_T must be defined before including hash_trie.h"
#endif
#ifndef TRIE_VAL_T
#  error "TRIE_VAL_T must be defined before including hash_trie.h"
#endif
#ifndef TRIE_HASH
#  error "TRIE_HASH must be defined before including hash_trie.h"
#endif

#ifndef TRIE_EQUAL
#  define TRIE_EQUAL(a, b)  ((a) == (b))
#endif

#ifndef TRIE_NAME
#  define TRIE_NAME     RC_CONCAT(rc_trie_, TRIE_KEY_T)
#endif

/* ---- derived type name macros (expand to clean public names) ---- */

#define TRIE_NODE_T_   RC_CONCAT(TRIE_NAME, _node)
#define TRIE_POOL_T_   RC_CONCAT(TRIE_NAME, _pool)

/* ---- internal function name macros ---- */

#define TRIE_ALLOC_BLOCK_  RC_CONCAT(TRIE_NAME, _alloc_block_)
#define TRIE_BLOCK_EMPTY_  RC_CONCAT(TRIE_NAME, _block_empty_)
#define TRIE_DEL_FROM_     RC_CONCAT(TRIE_NAME, _del_from_)

/* ---- public function name macros ---- */

#define TRIE_CREATE_   RC_CONCAT(TRIE_NAME, _create)
#define TRIE_FIND_     RC_CONCAT(TRIE_NAME, _find)
#define TRIE_ADD_      RC_CONCAT(TRIE_NAME, _add)
#define TRIE_DELETE_   RC_CONCAT(TRIE_NAME, _delete)

/* ---- once-only helper ---- */

#ifndef RC_TRIE_ONCE_
#define RC_TRIE_ONCE_

/*
 * Rotate a 64-bit hash right by 4 bits to expose the next 4-bit group
 * at the low end.
 */
static inline uint64_t rc_trie_ror64_(uint64_t h)
{
    return (h >> 4) | (h << 60);
}

#endif /* RC_TRIE_ONCE_ */

/* ---- generated types ---- */

/*
 * Node: one slot in a 16-node trie block.
 *
 * child_index encodes both occupancy and children:
 *   0              — empty slot (no key, no children)
 *   UINT32_MAX     — occupied, no children (leaf)
 *   1..UINT32_MAX-1 — occupied, children at block index child_index
 */
typedef struct {
    TRIE_KEY_T key;
    TRIE_VAL_T val;
    uint32_t   child_index;
} TRIE_NODE_T_;

/*
 * Pool: flat arena-backed array of nodes, grown 16 at a time.
 * Can be shared by multiple TRIE_NAME instances.
 * Zero-initialise to get an empty, valid pool.
 * num is always a multiple of 16.
 */
typedef struct {
    TRIE_NODE_T_ *data;
    uint32_t      num;
    uint32_t      cap;
} TRIE_POOL_T_;

/*
 * Trie: a 16-way hash trie backed by a node pool.
 * root is the block index of the root block; always valid after create().
 * Do NOT zero-initialise; always use TRIE_NAME_create().
 */
typedef struct {
    TRIE_POOL_T_ *pool;
    uint32_t      root;
} TRIE_NAME;

/* ---- internal: allocate a fresh 16-node block ---- */

/*
 * Appends 16 freshly-zeroed nodes to the pool and returns the new block
 * index.  Zeroing sets child_index = 0 (empty) for every node.
 * May call rc_arena_realloc, invalidating any raw pointers into pool->data.
 */
static inline uint32_t TRIE_ALLOC_BLOCK_(TRIE_POOL_T_ *pool, rc_arena *a)
{
    if (pool->num + 16 > pool->cap) {
        uint32_t old_cap = pool->cap;
        uint32_t new_cap = old_cap ? old_cap * 2 : 64;
        RC_ASSERT((size_t)new_cap * sizeof(TRIE_NODE_T_) <= UINT32_MAX &&
                     "hash trie: pool allocation exceeds 4 GB");
        /* rc_arena_realloc handles ptr=NULL, old_size=0 as a plain alloc. */
        pool->data = rc_arena_realloc(a, pool->data,
                                   (uint32_t)((size_t)old_cap * sizeof(TRIE_NODE_T_)),
                                   (uint32_t)((size_t)new_cap * sizeof(TRIE_NODE_T_)));
        RC_ASSERT(pool->data != NULL && "hash trie: pool OOM");
        pool->cap = new_cap;
    }

    uint32_t block_idx = pool->num / 16;
    memset(pool->data + pool->num, 0, 16 * sizeof(TRIE_NODE_T_));
    pool->num += 16;
    return block_idx;
}

/* ---- internal: test whether a block is entirely empty ---- */

/* Returns 1 if every node in block blk has child_index == 0. */
static inline int TRIE_BLOCK_EMPTY_(TRIE_NODE_T_ *blk)
{
    for (uint32_t i = 0; i < 16; i++) {
        if (blk[i].child_index != 0)
            return 0;
    }
    return 1;
}

/* ---- public: create ---- */

/*
 * Initialise a trie backed by pool, pre-allocating its root block.
 * pool and a must not be NULL.
 */
static inline TRIE_NAME TRIE_CREATE_(TRIE_POOL_T_ *pool, rc_arena *a)
{
    RC_ASSERT(pool != NULL && "hash trie: pool must not be NULL");
    RC_ASSERT(a    != NULL && "hash trie: arena must not be NULL");
    TRIE_NAME t;
    t.pool = pool;
    t.root = TRIE_ALLOC_BLOCK_(pool, a);
    return t;
}

/* ---- public: find ---- */

/*
 * Returns a pointer to the stored value for key, or NULL if absent.
 * t->pool must not be NULL.
 */
static inline TRIE_VAL_T *TRIE_FIND_(TRIE_NAME *t, TRIE_KEY_T key)
{
    RC_ASSERT(t->pool != NULL && "hash trie: pool must not be NULL");

    uint64_t h         = (uint64_t)TRIE_HASH(key);
    uint32_t block_idx = t->root;

    for (;;) {
        uint32_t      slot = (uint32_t)(h & 0xF);
        h = rc_trie_ror64_(h);

        TRIE_NODE_T_ *node = &t->pool->data[block_idx * 16 + slot];

        if (node->child_index == 0)          return NULL;   /* empty slot     */
        if (TRIE_EQUAL(node->key, key))      return &node->val;
        if (node->child_index == UINT32_MAX) return NULL;   /* leaf, no match */
        block_idx = node->child_index;
    }
}

/* ---- public: add ---- */

/*
 * Insert or update.  Returns 1 if key was new, 0 if an existing value
 * was replaced.  t->pool and a must not be NULL.
 *
 * When a slot is already occupied by a different key K', K' stays and the
 * new key is pushed into children.  A child block is allocated on demand.
 *
 * CAUTION: TRIE_ALLOC_BLOCK_ may realloc pool->data.  All node pointers
 * are re-derived from block/slot indices after every potential realloc.
 */
static inline int TRIE_ADD_(TRIE_NAME *t, TRIE_KEY_T key, TRIE_VAL_T val,
                             rc_arena *a)
{
    RC_ASSERT(t->pool != NULL && "hash trie: pool must not be NULL");
    RC_ASSERT(a       != NULL && "hash trie: arena must not be NULL");

    uint64_t h         = (uint64_t)TRIE_HASH(key);
    uint32_t block_idx = t->root;

    for (;;) {
        uint32_t slot = (uint32_t)(h & 0xF);
        h = rc_trie_ror64_(h);

        TRIE_NODE_T_ *node = &t->pool->data[block_idx * 16 + slot];

        if (node->child_index == 0) {
            /* Empty slot: place key here as a leaf. */
            node->key         = key;
            node->val         = val;
            node->child_index = UINT32_MAX;
            return 1;
        }

        if (TRIE_EQUAL(node->key, key)) {
            node->val = val;
            return 0;
        }

        /*
         * Slot owned by a different key.  Descend into children,
         * allocating a child block if this is the first collision here.
         *
         * child_index is re-read via pool->data[...] after the alloc
         * because the alloc may have moved the pool array.
         */
        if (node->child_index == UINT32_MAX) {
            uint32_t child_idx = TRIE_ALLOC_BLOCK_(t->pool, a);
            t->pool->data[block_idx * 16 + slot].child_index = child_idx;
            block_idx = child_idx;
        } else {
            block_idx = node->child_index;
        }
    }
}

/* ---- internal: recursive delete ---- */

/*
 * Delete key from block block_idx at trie depth 'depth'.
 * Sets *became_empty to 1 if the block is now entirely empty.
 * Returns 1 if key was found and removed, 0 otherwise.
 *
 * When deleting an interior node, the first occupied slot in the child block
 * is bubbled up to fill the vacancy; its deletion is then applied
 * recursively.  If the child block becomes empty after this, the parent
 * node's child_index is set to UINT32_MAX (it becomes a leaf).
 *
 * Recursion depth is bounded by the length of the deepest hash chain;
 * for distinct 64-bit hashes this is at most 16 levels.
 */
static inline int TRIE_DEL_FROM_(TRIE_POOL_T_ *pool, uint32_t block_idx,
                                  TRIE_KEY_T key, uint32_t depth,
                                  int *became_empty)
{
    /* Recompute the hash rotation state for this depth. */
    uint64_t h = (uint64_t)TRIE_HASH(key);
    for (uint32_t i = 0; i < depth; i++) h = rc_trie_ror64_(h);

    uint32_t      slot = (uint32_t)(h & 0xF);
    TRIE_NODE_T_ *node = &pool->data[block_idx * 16 + slot];

    if (node->child_index == 0) {
        /* Empty slot: key not present. */
        *became_empty = 0;
        return 0;
    }

    if (!TRIE_EQUAL(node->key, key)) {
        /* Key is not the owner of this slot; descend into children. */
        if (node->child_index == UINT32_MAX) {
            *became_empty = 0;
            return 0;
        }
        int child_empty = 0;
        int found = TRIE_DEL_FROM_(pool, node->child_index,
                                   key, depth + 1, &child_empty);
        if (child_empty)
            pool->data[block_idx * 16 + slot].child_index = UINT32_MAX;
        *became_empty = found &&
            TRIE_BLOCK_EMPTY_(pool->data + block_idx * 16);
        return found;
    }

    /* Found the key at this node. */

    if (node->child_index == UINT32_MAX) {
        /* Leaf: mark slot empty. */
        node->child_index = 0;
        *became_empty = TRIE_BLOCK_EMPTY_(pool->data + block_idx * 16);
        return 1;
    }

    /* Interior node: bubble up the first occupied slot from the child block. */
    uint32_t      child_block_idx = node->child_index;
    TRIE_NODE_T_ *child_blk       = pool->data + child_block_idx * 16;
    uint32_t      r               = UINT32_MAX;

    for (uint32_t i = 0; i < 16; i++) {
        if (child_blk[i].child_index != 0) {r = i; break;}
    }
    RC_ASSERT(r != UINT32_MAX &&
                 "hash trie: interior node points to empty child block");

    TRIE_KEY_T rep_key = child_blk[r].key;
    TRIE_VAL_T rep_val = child_blk[r].val;

    node->key = rep_key;
    node->val = rep_val;
    /* node->child_index (points to child block) is unchanged */

    int child_empty = 0;
    TRIE_DEL_FROM_(pool, child_block_idx, rep_key, depth + 1, &child_empty);
    if (child_empty)
        pool->data[block_idx * 16 + slot].child_index = UINT32_MAX;

    *became_empty = 0;   /* this node is still occupied after bubble-up */
    return 1;
}

/* ---- public: delete ---- */

/*
 * Remove key.  Returns 1 if key was present, 0 if absent.
 * No arena needed.  t->pool must not be NULL.
 */
static inline int TRIE_DELETE_(TRIE_NAME *t, TRIE_KEY_T key)
{
    RC_ASSERT(t->pool != NULL && "hash trie: pool must not be NULL");
    int became_empty = 0;
    return TRIE_DEL_FROM_(t->pool, t->root, key, 0, &became_empty);
}

/* ---- cleanup ---- */

#undef TRIE_NODE_T_
#undef TRIE_POOL_T_
#undef TRIE_ALLOC_BLOCK_
#undef TRIE_BLOCK_EMPTY_
#undef TRIE_DEL_FROM_
#undef TRIE_CREATE_
#undef TRIE_FIND_
#undef TRIE_ADD_
#undef TRIE_DELETE_

#undef TRIE_EQUAL
#undef TRIE_NAME
#undef TRIE_KEY_T
#undef TRIE_VAL_T
#undef TRIE_HASH
