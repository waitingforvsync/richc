/*
 * hash_trie.h - template header: 16-way hash trie.
 *
 * A trie where each node has 16 children, indexed by successive 4-bit groups of
 * a 64-bit key hash.  Nodes are stored 16 to a block; the blocks live in an
 * arena-backed pool that is just an rc_array of blocks (the array template does
 * the growth and zero-fill).  One pool can back many tries.
 *
 * A trie is a value, not a handle: it is nothing but a root block index, so it is
 * trivially copyable and a zero-initialised trie ({ 0 }) is a valid empty trie -
 * no make() call, matching the pool it sits on.  The pool is NOT stored in the
 * trie; it is passed to every operation (like the pool's own accessors), so a
 * struct that owns tries + a shared pool can be copied or returned by value freely.
 *
 * Structure
 * ---------
 *   Node  - one slot in a block: { key, val, child_index }
 *   Block - 16 nodes (RC_TRIE_NAME_nodes); addressed by block index
 *   Pool  - free-list pool of blocks (RC_TRIE_NAME_pool, an rc_pool); shared
 *           across tries; a block emptied by delete is recycled
 *   Trie  - { root } where root is the root block index + 1 (0 == empty, no root
 *           block yet); RC_TRIE_NAME
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
 * The trie's `root` field uses the same off-by-one the pool free-list does: it
 * stores the root block index + 1, so 0 is free to mean "no root block".  The
 * very first block a pool ever hands out is index 0, always claimed by some trie's
 * root (add allocates the root before any child block), so a child link is never
 * index 0 - which keeps child_index == 0 unambiguously "empty".
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
 * again) and the empty block is freed back to the pool for reuse.  The root block
 * is never freed, so once set `root` stays put.
 *
 * Lifecycle
 * ---------
 * Zero-initialise a trie ({ 0 }) and use it - no make() needed.  add() lazily
 * allocates the root block on the first insert.  find(), contains() and delete()
 * on an empty trie (root == 0) report "not present" without touching the pool.
 * Free the backing with pool_deinit.
 *
 * Define before including:
 *   RC_TRIE_KEY_TYPE     key type (required)
 *   RC_TRIE_VALUE_TYPE   value type (optional): define it for a map, omit it for
 *                        a set.  In a set the node carries no value, add() takes
 *                        no val, and find / find_ptr / value_* are not generated.
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
 *   RC_TRIE_NAME_node  { RC_TRIE_KEY_TYPE key; [RC_TRIE_VALUE_TYPE val;] uint32_t child_index; }
 *                      (val present only in a map)
 *   RC_TRIE_NAME_pool  rc_pool of 16-node blocks; shared by tries
 *   RC_TRIE_NAME       { uint32_t root; }   ({ 0 } is a valid empty trie)
 *
 * Generated functions (all static inline)
 * ----------------------------------------
 * The pool is passed to every operation; the trie is passed BY VALUE (it is a
 * plain value), except add() which takes it by pointer because it may lazily set
 * the root on the first insert.  The index accessors take only the pool, since a
 * node index is pool-global, not tied to one trie.  Read-only operations take a
 * CONST pool (contains / find / key_get / value_get and the by-value getters); only
 * the mutators and the pointer accessors (which hand out writable access) take a
 * mutable pool.
 *   RC_TRIE_NAME_pool    NAME_pool_make(uint32_t min_blocks, rc_arena *arena)
 *   void                 NAME_pool_reserve(NAME_pool *pool, uint32_t min_blocks, rc_arena *arena)
 *   void                 NAME_pool_deinit(NAME_pool *pool, rc_arena *arena)
 *   bool                 NAME_contains(NAME t, const NAME_pool *pool, RC_TRIE_KEY_TYPE key)
 *   bool                 NAME_add(NAME *t, NAME_pool *pool, RC_TRIE_KEY_TYPE key, [RC_TRIE_VALUE_TYPE val,] rc_arena *arena)
 *   bool                 NAME_delete(NAME t, NAME_pool *pool, RC_TRIE_KEY_TYPE key)
 *   RC_TRIE_KEY_TYPE     NAME_key_get(const NAME_pool *pool, uint32_t index)
 *   const RC_TRIE_KEY_TYPE *NAME_key_at(const NAME_pool *pool, uint32_t index)
 *   -- map only (RC_TRIE_VALUE_TYPE defined) --
 *   uint32_t             NAME_find(NAME t, const NAME_pool *pool, RC_TRIE_KEY_TYPE key)
 *   RC_TRIE_VALUE_TYPE  *NAME_find_ptr(NAME t, NAME_pool *pool, RC_TRIE_KEY_TYPE key)
 *   RC_TRIE_VALUE_TYPE   NAME_value_get(const NAME_pool *pool, uint32_t index)
 *   void                 NAME_value_set(NAME_pool *pool, uint32_t index, RC_TRIE_VALUE_TYPE value)
 *   RC_TRIE_VALUE_TYPE  *NAME_value_at(NAME_pool *pool, uint32_t index)
 *
 * contains() reports whether a key is present (both set and map tries).  add()
 * returns true if the key was new, false if it was already present (a map then
 * replaces its value); the val argument is omitted for a set.  delete() returns
 * true if the key was present.  For maps, find() returns a node index
 * (block * 16 + slot), or RC_INDEX_NONE if absent; read or write its value with
 * value_get / value_set / value_at.  The index is stable across pool growth
 * (prefer it for holding a result).  find_ptr() returns a value pointer directly
 * - faster for one-shot access, but the pointer follows the usual pool rule
 * (survives in-place growth, invalidated by a relocating grow).
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
 *   // one pool can back many independent tries; a trie is just { 0 }
 *   rc_trie_u64_pool pool = rc_trie_u64_pool_make(0, &arena);
 *   rc_trie_u64 a = { 0 };
 *   rc_trie_u64 b = { 0 };
 *
 *   rc_trie_u64_add(&a, &pool, 0xDEADBEEF, 42, &arena);   // true (new)
 *   uint32_t i = rc_trie_u64_find(a, &pool, 0xDEADBEEF);  // i != RC_INDEX_NONE
 *   int v = rc_trie_u64_value_get(&pool, i);              // 42
 *   int *p = rc_trie_u64_find_ptr(a, &pool, 0xDEADBEEF);  // direct pointer (or NULL)
 *   rc_trie_u64_find(b, &pool, 0xDEADBEEF);               // RC_INDEX_NONE: b is independent
 *
 *   rc_trie_u64_pool_deinit(&pool, &arena);
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
// RC_TRIE_VALUE_TYPE is optional: define it for a map, omit it for a set (then
// the node carries no value, and find / find_ptr / value_* are not generated).
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

/* ---- internal pool-call macros ---- */

#define RC_TRIE_POOL_ALLOC_    RC_CONCAT(RC_TRIE_POOL_, _alloc)
#define RC_TRIE_POOL_FREE_     RC_CONCAT(RC_TRIE_POOL_, _free)
#define RC_TRIE_POOL_AT_       RC_CONCAT(RC_TRIE_POOL_, _at)
#define RC_TRIE_POOL_AT_CONST_ RC_CONCAT(RC_TRIE_POOL_, _at_const)

/* ---- internal function-name macros ---- */

#define RC_TRIE_ALLOC_BLOCK_ RC_CONCAT(RC_TRIE_NAME, _alloc_block_)
#define RC_TRIE_BLOCK_EMPTY_ RC_CONCAT(RC_TRIE_NAME, _block_empty_)
#define RC_TRIE_DEL_FROM_    RC_CONCAT(RC_TRIE_NAME, _del_from_)
#define RC_TRIE_PROBE_       RC_CONCAT(RC_TRIE_NAME, _probe_)

/* ---- public function-name macros ---- */

#define RC_TRIE_CONTAINS_     RC_CONCAT(RC_TRIE_NAME, _contains)
#define RC_TRIE_FIND_         RC_CONCAT(RC_TRIE_NAME, _find)
#define RC_TRIE_FIND_PTR_     RC_CONCAT(RC_TRIE_NAME, _find_ptr)
#define RC_TRIE_VALUE_GET_    RC_CONCAT(RC_TRIE_NAME, _value_get)
#define RC_TRIE_VALUE_SET_    RC_CONCAT(RC_TRIE_NAME, _value_set)
#define RC_TRIE_VALUE_AT_     RC_CONCAT(RC_TRIE_NAME, _value_at)
#define RC_TRIE_KEY_GET_      RC_CONCAT(RC_TRIE_NAME, _key_get)
#define RC_TRIE_KEY_AT_       RC_CONCAT(RC_TRIE_NAME, _key_at)
#define RC_TRIE_ADD_          RC_CONCAT(RC_TRIE_NAME, _add)
#define RC_TRIE_DELETE_       RC_CONCAT(RC_TRIE_NAME, _delete)

/* ---- generated types ---- */

/*
 * Node: one slot in a 16-node block.  See the file header for the child_index
 * encoding (0 empty, UINT32_MAX leaf, else child block index).
 */
typedef struct RC_TRIE_NODE_ {
    RC_TRIE_KEY_TYPE   key;
#ifdef RC_TRIE_VALUE_TYPE
    RC_TRIE_VALUE_TYPE val;
#endif
    uint32_t           child_index;
} RC_TRIE_NODE_;

/* Block: 16 consecutive nodes; the pool's element type. */
typedef struct RC_TRIE_NODES_ {
    RC_TRIE_NODE_ node[16];
} RC_TRIE_NODES_;

/*
 * Pool: a free-list pool of blocks (one block == one trie level).  Using a pool
 * rather than a plain array lets a block emptied by delete be recycled by a
 * later allocation instead of leaking.  The pool type is RC_TRIE_NAME_pool.
 */
#define RC_POOL_TYPE RC_TRIE_NODES_
#define RC_POOL_NAME RC_TRIE_POOL_
#include "richc/template/pool.h"

/*
 * Trie: a 16-way hash trie backed by a node pool.  It is nothing but a root block
 * index, stored + 1 so that 0 means "no root block yet".  A zero-initialised trie
 * ({ 0 }) is a valid empty trie, so no make() is needed; add() allocates the root
 * on the first insert.  The pool is passed to every operation, never stored here,
 * so the trie is a position-independent value.
 */
typedef struct RC_TRIE_NAME {
    uint32_t root;
} RC_TRIE_NAME;

/*
 * The pool lifecycle (make / reserve / reset / deinit) and its low-level block
 * ops (alloc / free / get / set / at) all come from the rc_pool template, named
 * RC_TRIE_NAME_pool_*.  Use pool_deinit to free the backing.  The block ops are
 * used internally by the trie; do not call them directly on a pool that backs
 * tries.
 */

/* ---- internal: allocate a fresh, zeroed 16-node block ---- */

/*
 * Allocate a zeroed block from the pool, reusing a freed one when available, and
 * return its block index.  Zeroing sets child_index = 0 (empty) for every node.
 * May reallocate the backing, so callers must re-derive any node pointers
 * afterwards.
 */
static inline uint32_t RC_TRIE_ALLOC_BLOCK_(RC_TRIE_POOL_ *pool, rc_arena *arena)
{
    return RC_TRIE_POOL_ALLOC_(pool, arena);
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

/* ---- probe / contains ---- */

/*
 * Internal: the node index of key (block * 16 + slot), or RC_INDEX_NONE if
 * absent (including an empty trie, root == 0).  The index packs the block index
 * and the 4-bit slot; a valid pool never holds anywhere near 2^28 blocks, so it
 * fits a uint32_t.  The index is stable across pool growth (unlike a raw pointer)
 * but only valid until the next add/delete that restructures the trie.
 */
static inline uint32_t RC_TRIE_PROBE_(RC_TRIE_NAME t, const RC_TRIE_POOL_ *pool, RC_TRIE_KEY_TYPE key)
{
    RC_ASSERT(pool != NULL);
    if (t.root == 0) {
        return RC_INDEX_NONE;   // empty trie: no root block
    }

    uint64_t h         = (uint64_t)RC_TRIE_HASH(key);
    uint32_t block_idx = t.root - 1;

    for (;;) {
        uint32_t slot = (uint32_t)(h & 0xF);
        h = rc_trie_ror64_(h);

        const RC_TRIE_NODE_ *node = &RC_TRIE_POOL_AT_CONST_(pool, block_idx)->node[slot];

        if (node->child_index == 0)          return RC_INDEX_NONE;   // empty slot
        if (RC_TRIE_EQUAL(node->key, key))   return block_idx * 16 + slot;
        if (node->child_index == UINT32_MAX) return RC_INDEX_NONE;   // leaf, no match
        block_idx = node->child_index;
    }
}

/* True if key is present.  Available for both set and map tries. */
static inline bool RC_TRIE_CONTAINS_(RC_TRIE_NAME t, const RC_TRIE_POOL_ *pool, RC_TRIE_KEY_TYPE key)
{
    return RC_TRIE_PROBE_(t, pool, key) != RC_INDEX_NONE;
}

/* ---- key access by node index (set and map tries) ---- */

/*
 * The key at a node index (block * 16 + slot), as returned by find or handed to a
 * foreach callback.  A node index is pool-global, so these take only the pool, not
 * the trie.  A key is immutable once placed (there is no key_set), so BOTH forms are
 * read-only: they take a const pool, and key_at returns a const pointer (usual
 * pool-pointer rule: valid until a relocating grow), key_get returns it by value.
 * Both assert the index refers to an occupied node.
 */
static inline const RC_TRIE_KEY_TYPE *RC_TRIE_KEY_AT_(const RC_TRIE_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index != RC_INDEX_NONE);
    const RC_TRIE_NODE_ *node = &RC_TRIE_POOL_AT_CONST_(pool, index / 16)->node[index % 16];
    RC_ASSERT(node->child_index != 0);   // the index must refer to an occupied node
    return &node->key;
}

/* The key at a node index, by value. */
static inline RC_TRIE_KEY_TYPE RC_TRIE_KEY_GET_(const RC_TRIE_POOL_ *pool, uint32_t index)
{
    return *RC_TRIE_KEY_AT_(pool, index);
}

#ifdef RC_TRIE_VALUE_TYPE

/* ---- value access by node index (map tries) ---- */

/*
 * find returns a node index (block * 16 + slot) - stable across pool growth, so
 * prefer it for holding a result.  value_at returns a pointer to its value, which
 * follows the usual pool rule (survives in-place growth, invalidated by a
 * relocating grow); it asserts the index refers to an occupied node.  A node index
 * is pool-global, so these take only the pool, not the trie.
 */
static inline RC_TRIE_VALUE_TYPE *RC_TRIE_VALUE_AT_(RC_TRIE_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index != RC_INDEX_NONE);
    RC_TRIE_NODE_ *node = &RC_TRIE_POOL_AT_(pool, index / 16)->node[index % 16];
    RC_ASSERT(node->child_index != 0);   // the index must refer to an occupied node
    return &node->val;
}

/* The value at a node index, by value.  Read-only, so it takes a const pool. */
static inline RC_TRIE_VALUE_TYPE RC_TRIE_VALUE_GET_(const RC_TRIE_POOL_ *pool, uint32_t index)
{
    RC_ASSERT(index != RC_INDEX_NONE);
    const RC_TRIE_NODE_ *node = &RC_TRIE_POOL_AT_CONST_(pool, index / 16)->node[index % 16];
    RC_ASSERT(node->child_index != 0);   // the index must refer to an occupied node
    return node->val;
}

/* Store a value at a node index. */
static inline void RC_TRIE_VALUE_SET_(RC_TRIE_POOL_ *pool, uint32_t index, RC_TRIE_VALUE_TYPE value)
{
    *RC_TRIE_VALUE_AT_(pool, index) = value;
}

/* ---- find (map tries) ---- */

/*
 * Node index of key (block * 16 + slot), or RC_INDEX_NONE if absent.  Pair it
 * with value_get / value_set / value_at.
 */
static inline uint32_t RC_TRIE_FIND_(RC_TRIE_NAME t, const RC_TRIE_POOL_ *pool, RC_TRIE_KEY_TYPE key)
{
    return RC_TRIE_PROBE_(t, pool, key);
}

/*
 * Pointer to the value of key, or NULL if absent.  Convenient for one-shot
 * access; the returned pointer follows the usual pool rule (survives in-place
 * growth, invalidated by a relocating grow), whereas the index from find is
 * always growth-stable - prefer find + value_* to hold a result across growth.
 */
static inline RC_TRIE_VALUE_TYPE *RC_TRIE_FIND_PTR_(RC_TRIE_NAME t, RC_TRIE_POOL_ *pool, RC_TRIE_KEY_TYPE key)
{
    uint32_t index = RC_TRIE_FIND_(t, pool, key);
    return index == RC_INDEX_NONE ? NULL : RC_TRIE_VALUE_AT_(pool, index);
}

#endif /* RC_TRIE_VALUE_TYPE */

/* ---- add ---- */

/*
 * Insert (map: insert or update).  Returns true if the key was new, false if it
 * was already present (a map then replaces its value).  In set tries there is no
 * val parameter.  pool and arena must not be NULL.
 *
 * add is the one operation that takes the trie by pointer: on the first insert
 * into an empty trie (root == 0) it lazily allocates the root block and writes the
 * root back.  When a slot is already owned by a different key, that key stays and
 * the new key descends into children; a child block is allocated on demand.
 * Because an alloc may reallocate the backing, links are written through freshly
 * re-derived slots, never through a cached node pointer.
 */
static inline bool RC_TRIE_ADD_(RC_TRIE_NAME *t, RC_TRIE_POOL_ *pool, RC_TRIE_KEY_TYPE key,
#ifdef RC_TRIE_VALUE_TYPE
                                RC_TRIE_VALUE_TYPE val,
#endif
                                rc_arena *arena)
{
    RC_ASSERT(t != NULL);
    RC_ASSERT(pool != NULL);
    RC_ASSERT(arena != NULL);

    if (t->root == 0) {
        t->root = RC_TRIE_ALLOC_BLOCK_(pool, arena) + 1;   // lazily create the root block
    }

    uint64_t h         = (uint64_t)RC_TRIE_HASH(key);
    uint32_t block_idx = t->root - 1;

    for (;;) {
        uint32_t slot = (uint32_t)(h & 0xF);
        h = rc_trie_ror64_(h);

        RC_TRIE_NODE_ *node = &RC_TRIE_POOL_AT_(pool, block_idx)->node[slot];

        if (node->child_index == 0) {
            // empty slot: place key here as a leaf
            *node = (RC_TRIE_NODE_) {
                .key         = key,
#ifdef RC_TRIE_VALUE_TYPE
                .val         = val,
#endif
                .child_index = UINT32_MAX
            };
            return true;
        }

        if (RC_TRIE_EQUAL(node->key, key)) {
#ifdef RC_TRIE_VALUE_TYPE
            node->val = val;
#endif
            return false;
        }

        if (node->child_index == UINT32_MAX) {
            // first collision here: allocate a child block (may realloc the pool)
            uint32_t child_idx = RC_TRIE_ALLOC_BLOCK_(pool, arena);
            RC_TRIE_POOL_AT_(pool, block_idx)->node[slot].child_index = child_idx;
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
    RC_TRIE_NODE_ *node = &RC_TRIE_POOL_AT_(pool, block_idx)->node[slot];

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
        if (found && RC_TRIE_BLOCK_EMPTY_(RC_TRIE_POOL_AT_(pool, child_idx))) {
            RC_TRIE_POOL_AT_(pool, block_idx)->node[slot].child_index = UINT32_MAX;
            RC_TRIE_POOL_FREE_(pool, child_idx);   // recycle the emptied block
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
    RC_TRIE_NODE_ *child_blk       = RC_TRIE_POOL_AT_(pool, child_block_idx)->node;
    uint32_t       r               = UINT32_MAX;

    for (uint32_t i = 0; i < 16; i++) {
        if (child_blk[i].child_index != 0) { r = i; break; }
    }
    RC_ASSERT(r != UINT32_MAX);   // interior node must point to a non-empty child block

    RC_TRIE_KEY_TYPE rep_key = child_blk[r].key;

    node->key = rep_key;
#ifdef RC_TRIE_VALUE_TYPE
    node->val = child_blk[r].val;
#endif
    // node->child_index (the child block) is unchanged

    RC_TRIE_DEL_FROM_(pool, child_block_idx, rep_key, depth + 1);
    if (RC_TRIE_BLOCK_EMPTY_(RC_TRIE_POOL_AT_(pool, child_block_idx))) {
        RC_TRIE_POOL_AT_(pool, block_idx)->node[slot].child_index = UINT32_MAX;
        RC_TRIE_POOL_FREE_(pool, child_block_idx);   // recycle the emptied block
    }
    return true;
}

/* ---- delete ---- */

/* Remove key.  Returns true if the key was present (false on an empty trie). */
static inline bool RC_TRIE_DELETE_(RC_TRIE_NAME t, RC_TRIE_POOL_ *pool, RC_TRIE_KEY_TYPE key)
{
    RC_ASSERT(pool != NULL);
    if (t.root == 0) {
        return false;   // empty trie: nothing to delete
    }
    return RC_TRIE_DEL_FROM_(pool, t.root - 1, key, 0);
}

/* ---- cleanup ---- */

#undef RC_TRIE_NODE_
#undef RC_TRIE_NODES_
#undef RC_TRIE_POOL_
#undef RC_TRIE_POOL_ALLOC_
#undef RC_TRIE_POOL_FREE_
#undef RC_TRIE_POOL_AT_
#undef RC_TRIE_POOL_AT_CONST_
#undef RC_TRIE_ALLOC_BLOCK_
#undef RC_TRIE_BLOCK_EMPTY_
#undef RC_TRIE_DEL_FROM_
#undef RC_TRIE_PROBE_
#undef RC_TRIE_CONTAINS_
#undef RC_TRIE_FIND_
#undef RC_TRIE_FIND_PTR_
#undef RC_TRIE_VALUE_GET_
#undef RC_TRIE_VALUE_SET_
#undef RC_TRIE_VALUE_AT_
#undef RC_TRIE_KEY_GET_
#undef RC_TRIE_KEY_AT_
#undef RC_TRIE_ADD_
#undef RC_TRIE_DELETE_

#undef RC_TRIE_EQUAL
#undef RC_TRIE_NAME
#undef RC_TRIE_KEY_TYPE
#undef RC_TRIE_VALUE_TYPE
#undef RC_TRIE_HASH
