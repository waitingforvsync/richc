/*
 * hash_multimap.h - template header: open-addressing hash multimap.
 *
 * A hash map that associates each key with an ordered sequence of values.
 * Internally the key set is stored in an open-addressing hash table (SoA
 * layout, linear probing, tombstone deletion, load factor < 3/4) and each
 * key's values are kept in a singly-linked list of nodes drawn from a flat
 * pool.  Multiple MMAP_NAME instances may share a single pool.
 *
 * Key lifecycle
 * -------------
 * A key is inserted into the hash table on the first add() for that key and
 * removed (tombstoned) by remove_all().  While a key is present its chain
 * always contains at least one live node.
 *
 * Node free-list
 * --------------
 * Each node stores { val, next }.  When alive, 'next' is the raw index of
 * the following node in this key's chain (RC_INDEX_NONE = end of chain).
 * When freed by remove_all(), the node's 'next' field is reused as the
 * free-list link.  'free_head' uses index+1 encoding: 0 means the free-list
 * is empty; n means the first free node is at nodes[n-1].  Freed nodes store
 * the same index+1 encoding in 'next'.  This makes the pool zero-initializable
 * with {0}.  New nodes are taken from the free-list before extending the pool.
 *
 * Memory layout (hash table portion)
 * ------------------------------------
 * All per-slot data is stored in a single arena allocation sliced into three
 * tightly-packed arrays.  For capacity N, the block layout is:
 *
 *   [0,           N)              uint8_t  states[N]
 *   [keys_off,    keys_off+N*K)  KEY      keys[N]    (K = sizeof(KEY))
 *   [heads_off,   heads_off+N*4) uint32_t heads[N]
 *
 * keys_off  = align_up(N, alignof(KEY))
 * heads_off = align_up(keys_off + N*K, alignof(uint32_t))
 *
 * heads[i] is the pool index of the first node in the chain for slot i,
 * valid only when states[i] == RC_HASH_MMAP_SLOT_OCCUPIED.
 *
 * Generated types
 * ---------------
 *   MMAP_NAME_node  { MMAP_VAL_T val; uint32_t next; }
 *
 *   MMAP_NAME_pool  { MMAP_NAME_node *nodes;
 *                     uint32_t len; uint32_t cap; uint32_t free_head; }
 *        Zero-initializable: {0} and NAME_pool_make() are equivalent.
 *        free_head uses index+1 encoding (0 = empty; see Node free-list above).
 *
 *   MMAP_NAME       { char *data; uint32_t count; uint32_t used;
 *                     uint32_t cap; MMAP_NAME_pool *pool; }
 *        Initialise with NAME_make(pool); do not zero-initialise directly.
 *
 * Generated functions (all static inline)
 * ----------------------------------------
 *
 *   MMAP_NAME_pool NAME_pool_make(void)
 *        Return an empty, valid pool.  Equivalent to {0}.
 *
 *   MMAP_NAME NAME_make(MMAP_NAME_pool *pool)
 *        Return an empty, valid multimap backed by pool.
 *        pool must not be NULL.
 *
 *   void NAME_reserve(MMAP_NAME *m, uint32_t min_count, rc_arena *a)
 *        Ensure the map can hold at least min_count distinct keys without
 *        triggering a rehash.  No-op if capacity is already sufficient.
 *
 *   void NAME_pool_reserve(MMAP_NAME_pool *pool, uint32_t min_nodes, rc_arena *a)
 *        Ensure the pool backing array has room for at least min_nodes nodes.
 *        No-op if capacity is already sufficient.
 *
 *   int  NAME_add(MMAP_NAME *m, MMAP_KEY_T key, MMAP_VAL_T val, rc_arena *a)
 *        Insert (key, val).  Returns 1 if key was new, 0 if key already had
 *        entries.  The new value is prepended to the key's chain.
 *        Arena may be NULL only if neither the hash table nor the pool need
 *        to grow.
 *
 *   int  NAME_remove_all(MMAP_NAME *m, MMAP_KEY_T key)
 *        Remove all values for key.  Returns 1 if key was present, 0 if
 *        absent.  All nodes for key are returned to the pool's free-list.
 *        No arena needed.
 *
 *   uint32_t NAME_find_head(const MMAP_NAME *m, MMAP_KEY_T key)
 *        Returns the pool index of the first node for key, or RC_INDEX_NONE
 *        if key is absent.
 *
 *   int  NAME_contains(const MMAP_NAME *m, MMAP_KEY_T key)
 *        Returns 1 if key is present, 0 if absent.
 *
 *   uint32_t NAME_next(const MMAP_NAME *m, uint32_t pos)
 *        Returns the smallest slot index >= pos holding a live key, or
 *        m->cap if none.  Start at pos=0; advance with NAME_next(m, i+1).
 *
 *   MMAP_KEY_T NAME_key_at(const MMAP_NAME *m, uint32_t i)
 *        Key at slot i.  Only valid when slot i is OCCUPIED.
 *
 *   uint32_t NAME_head_at(const MMAP_NAME *m, uint32_t i)
 *        Pool index of the first node for slot i.  Only valid when OCCUPIED.
 *
 *   MMAP_VAL_T *NAME_node_val(MMAP_NAME *m, uint32_t j)
 *        Pointer to the value of pool node j.
 *        Invalidated by any add that causes pool reallocation.
 *
 *   uint32_t NAME_node_next(const MMAP_NAME *m, uint32_t j)
 *        Index of the next node in the chain after j, or RC_INDEX_NONE if j
 *        is the last node.
 *
 * Iteration idioms
 * ----------------
 * All (key, value) pairs:
 *   for (uint32_t i = NAME_next(m, 0); i < m->cap; i = NAME_next(m, i+1)) {
 *       MMAP_KEY_T  k = NAME_key_at(m, i);
 *       for (uint32_t j = NAME_head_at(m, i); j != RC_INDEX_NONE; j = NAME_node_next(m, j)) {
 *           MMAP_VAL_T *v = NAME_node_val(m, j);
 *       }
 *   }
 *
 * All values for a specific key:
 *   for (uint32_t j = NAME_find_head(m, key); j != RC_INDEX_NONE; j = NAME_node_next(m, j)) {
 *       MMAP_VAL_T *v = NAME_node_val(m, j);
 *   }
 *
 * Define before including:
 *   MMAP_KEY_T       key type (required)
 *   MMAP_VAL_T       value type (required)
 *   MMAP_HASH(key)   hash expression; must expand to an integer type (required)
 *   MMAP_EQUAL(a, b) equality expression for two keys
 *                    (optional; default: (a) == (b))
 *   MMAP_NAME        map struct name
 *                    (optional; default: rc_mmap_##MMAP_KEY_T)
 *                    Must be provided when instantiating multiple multimaps
 *                    for the same key type.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Example (int → int):
 *   #define MMAP_KEY_T   int
 *   #define MMAP_VAL_T   int
 *   #define MMAP_HASH(k) rc_hash_i32(k)
 *   #include "richc/template/hash_multimap.h"
 *   // defines: rc_mmap_int, rc_mmap_int_pool, rc_mmap_int_node,
 *   //          rc_mmap_int_pool_make, rc_mmap_int_make,
 *   //          rc_mmap_int_add, rc_mmap_int_remove_all, ...
 */

#include "richc/template_util.h"

#ifndef MMAP_KEY_T
#  error "MMAP_KEY_T must be defined before including hash_multimap.h"
#endif
#ifndef MMAP_VAL_T
#  error "MMAP_VAL_T must be defined before including hash_multimap.h"
#endif
#ifndef MMAP_HASH
#  error "MMAP_HASH must be defined before including hash_multimap.h"
#endif

#ifndef MMAP_EQUAL
#  define MMAP_EQUAL(a, b) ((a) == (b))
#endif

#ifndef MMAP_NAME
#  define MMAP_NAME RC_CONCAT(rc_mmap_, MMAP_KEY_T)
#endif

/* ---- once-only helpers ---- */

#ifndef RC_HASH_MULTIMAP_H_
#define RC_HASH_MULTIMAP_H_
#include <stdalign.h>
#include <stdint.h>
#include <string.h>
#include "richc/arena.h"
/* Round n up to the nearest multiple of a; a must be a power of two. */
#define RC_MMAP_ALIGN_UP_(n, a) (((size_t)(n) + (a) - 1) & ~((a) - 1))
typedef enum {
    RC_HASH_MMAP_SLOT_EMPTY     = 0,
    RC_HASH_MMAP_SLOT_OCCUPIED  = 1,
    RC_HASH_MMAP_SLOT_TOMBSTONE = 2
} rc_hash_mmap_slot_state;
#endif /* RC_HASH_MULTIMAP_H_ */

/* ---- derived type names ---- */

#define MMAP_NODE_T_ RC_CONCAT(MMAP_NAME, _node)
#define MMAP_POOL_T_ RC_CONCAT(MMAP_NAME, _pool)

/* ---- per-type layout macros ---- */

/*
 * Byte offset of keys[] from the block base for a given capacity.
 * Keys begin immediately after states[], padded to KEY alignment.
 */
#define MMAP_KEYS_OFF_(cap) \
    RC_MMAP_ALIGN_UP_((size_t)(cap), alignof(MMAP_KEY_T))

/*
 * Byte offset of heads[] from the block base for a given capacity.
 * Heads begin immediately after keys[], padded to uint32_t alignment.
 */
#define MMAP_HEADS_OFF_(cap) \
    RC_MMAP_ALIGN_UP_(MMAP_KEYS_OFF_(cap) + (size_t)(cap) * sizeof(MMAP_KEY_T), \
                      alignof(uint32_t))

/* Total byte size of the SoA block for a given capacity. */
#define MMAP_BLOCK_SIZE_(cap) \
    (MMAP_HEADS_OFF_(cap) + (size_t)(cap) * sizeof(uint32_t))

/* ---- internal helper name macros ---- */

#define MMAP_REHASH_        RC_CONCAT(MMAP_NAME, _rehash_)
#define MMAP_ALLOC_NODE_    RC_CONCAT(MMAP_NAME, _alloc_node_)
#define MMAP_FREE_CHAIN_    RC_CONCAT(MMAP_NAME, _free_chain_)
#define MMAP_POOL_MAKE_     RC_CONCAT(MMAP_NAME, _pool_make)
#define MMAP_MAKE_          RC_CONCAT(MMAP_NAME, _make)
#define MMAP_RESERVE_       RC_CONCAT(MMAP_NAME, _reserve)
#define MMAP_POOL_RESERVE_  RC_CONCAT(MMAP_NAME, _pool_reserve)
#define MMAP_ADD_           RC_CONCAT(MMAP_NAME, _add)
#define MMAP_REMOVE_ALL_    RC_CONCAT(MMAP_NAME, _remove_all)
#define MMAP_FIND_HEAD_     RC_CONCAT(MMAP_NAME, _find_head)
#define MMAP_CONTAINS_      RC_CONCAT(MMAP_NAME, _contains)
#define MMAP_NEXT_          RC_CONCAT(MMAP_NAME, _next)
#define MMAP_KEY_AT_        RC_CONCAT(MMAP_NAME, _key_at)
#define MMAP_HEAD_AT_       RC_CONCAT(MMAP_NAME, _head_at)
#define MMAP_NODE_VAL_      RC_CONCAT(MMAP_NAME, _node_val)
#define MMAP_NODE_NEXT_     RC_CONCAT(MMAP_NAME, _node_next)

/* ---- generated types ---- */

/*
 * Pool node: one value entry in a key's chain.
 *
 * val  — the stored value.
 * next — when live: raw index of the next node in this key's chain, or
 *        RC_INDEX_NONE for the last node.  When on the free-list: the index+1
 *        value of the next free-list entry (0 = tail of the free-list).
 */
typedef struct {
    MMAP_VAL_T val;
    uint32_t   next;
} MMAP_NODE_T_;

/*
 * Node pool: flat arena-backed array of nodes.
 * Multiple MMAP_NAME instances can share one pool.
 * Zero-initializable: {0} is valid and equivalent to NAME_pool_make().
 *
 * nodes     — backing array (NULL when empty)
 * len       — number of positions ever allocated (live or freed)
 * cap       — capacity of the backing array
 * free_head — index+1 of first free node; 0 = empty free-list
 */
typedef struct {
    MMAP_NODE_T_ *nodes;
    uint32_t      len;
    uint32_t      cap;
    uint32_t      free_head;
} MMAP_POOL_T_;

/*
 * Hash multimap.
 *
 * data  — base of the SoA allocation (NULL for an empty map)
 * count — number of distinct live keys (OCCUPIED slots)
 * used  — live + tombstone slots; used for load-factor calculations
 * cap   — hash table slot count; always a power of two, or zero
 * pool  — pointer to the shared node pool; must not be NULL
 *
 * Derive the three arrays at runtime as:
 *   states = (uint8_t *)data
 *   keys   = (MMAP_KEY_T *)(data + MMAP_KEYS_OFF_(cap))
 *   heads  = (uint32_t *)(data + MMAP_HEADS_OFF_(cap))
 *
 * Initialise with NAME_make(pool); do not zero-initialise directly.
 */
typedef struct {
    char         *data;
    uint32_t      count;
    uint32_t      used;
    uint32_t      cap;
    MMAP_POOL_T_ *pool;
} MMAP_NAME;

/* ---- public: pool_make ---- */

/* Return an empty, valid pool.  Equivalent to {0}. */
static inline MMAP_POOL_T_ MMAP_POOL_MAKE_(void)
{
    return (MMAP_POOL_T_) { 0 };
}

/* ---- public: make ---- */

/* Return an empty, valid multimap backed by pool.  pool must not be NULL. */
static inline MMAP_NAME MMAP_MAKE_(MMAP_POOL_T_ *pool)
{
    RC_ASSERT(pool != NULL && "hash multimap: pool must not be NULL");
    return (MMAP_NAME) { NULL, 0, 0, 0, pool };
}

/* ---- internal: alloc_node_ ---- */

/*
 * Allocate one node from pool, pulling from the free-list when available or
 * extending the backing array otherwise.  Returns the new node index.
 * pool->nodes may be reallocated; raw pointers into it are invalidated.
 */
static inline uint32_t MMAP_ALLOC_NODE_(MMAP_POOL_T_ *pool, rc_arena *a)
{
    if (pool->free_head != 0) {
        uint32_t idx    = pool->free_head - 1;
        pool->free_head = pool->nodes[idx].next;
        return idx;
    }
    if (pool->len == pool->cap) {
        uint32_t old_cap = pool->cap;
        uint32_t new_cap = old_cap ? old_cap * 2 : 8;
        RC_ASSERT((size_t)new_cap * sizeof(MMAP_NODE_T_) <= UINT32_MAX &&
                     "hash multimap: pool allocation exceeds 4 GB");
        pool->nodes = rc_arena_realloc(a, pool->nodes,
                                        (uint32_t)((size_t)old_cap * sizeof(MMAP_NODE_T_)),
                                        (uint32_t)((size_t)new_cap * sizeof(MMAP_NODE_T_)));
        RC_ASSERT(pool->nodes != NULL && "hash multimap: pool OOM");
        pool->cap = new_cap;
    }
    return pool->len++;
}

/* ---- internal: free_chain_ ---- */

/*
 * Return all nodes in the chain starting at head_idx to the pool's free-list.
 * head_idx must not be UINT32_MAX.
 */
static inline void MMAP_FREE_CHAIN_(MMAP_POOL_T_ *pool, uint32_t head_idx)
{
    uint32_t j = head_idx;
    while (j != RC_INDEX_NONE) {
        uint32_t nxt        = pool->nodes[j].next;   /* raw live-chain index */
        pool->nodes[j].next = pool->free_head;        /* store index+1 encoding */
        pool->free_head     = j + 1;
        j = nxt;
    }
}

/* ---- internal: rehash_ ---- */

/*
 * Allocate a fresh SoA block of new_cap slots, zero the states array,
 * re-insert all live entries from the current block, and discard tombstones.
 * new_cap must be a power of two.
 */
static inline void MMAP_REHASH_(MMAP_NAME *m, uint32_t new_cap, rc_arena *a)
{
    RC_ASSERT((new_cap & (new_cap - 1)) == 0 &&
                 "hash multimap: new_cap must be a power of two");
    RC_ASSERT(a != NULL && "hash multimap: arena required for rehash");

    size_t block_size = MMAP_BLOCK_SIZE_(new_cap);
    RC_ASSERT(block_size <= UINT32_MAX && "hash multimap: SoA block exceeds 4 GB");
    char *new_data = rc_arena_alloc(a, (uint32_t)block_size);
    RC_ASSERT(new_data != NULL && "hash multimap: arena OOM");

    /* Zero only the states array; keys/heads are only read when OCCUPIED. */
    memset(new_data, 0, new_cap);

    uint8_t    *new_states = (uint8_t *)new_data;
    MMAP_KEY_T *new_keys   = (MMAP_KEY_T *)(new_data + MMAP_KEYS_OFF_(new_cap));
    uint32_t   *new_heads  = (uint32_t *)(new_data + MMAP_HEADS_OFF_(new_cap));
    uint32_t    mask       = new_cap - 1;

    if (m->data) {
        uint8_t    *old_states = (uint8_t *)m->data;
        MMAP_KEY_T *old_keys   = (MMAP_KEY_T *)(m->data + MMAP_KEYS_OFF_(m->cap));
        uint32_t   *old_heads  = (uint32_t *)(m->data + MMAP_HEADS_OFF_(m->cap));

        for (uint32_t i = 0; i < m->cap; i++) {
            if (old_states[i] != RC_HASH_MMAP_SLOT_OCCUPIED) continue;
            uint32_t j = (uint32_t)MMAP_HASH(old_keys[i]) & mask;
            while (new_states[j] == RC_HASH_MMAP_SLOT_OCCUPIED) j = (j + 1) & mask;
            new_states[j] = RC_HASH_MMAP_SLOT_OCCUPIED;
            new_keys[j]   = old_keys[i];
            new_heads[j]  = old_heads[i];   /* pool indices are stable across rehash */
        }
    }

    m->data = new_data;
    m->cap  = new_cap;
    m->used = m->count;   /* tombstones gone */
}

/* ---- public: reserve ---- */

/*
 * Ensure the map can hold at least min_count distinct keys without rehashing.
 * Finds the smallest power-of-two capacity c such that floor(c * 3/4) >=
 * min_count, then rehashes to c if c > current capacity.
 */
static inline void MMAP_RESERVE_(MMAP_NAME *m, uint32_t min_count, rc_arena *a)
{
    if (min_count == 0) return;

    uint32_t new_cap = (m->cap > 8) ? m->cap : 8;
    while (new_cap - new_cap / 4 < min_count) {
        RC_ASSERT(new_cap <= UINT32_MAX / 2 &&
                     "hash multimap: capacity overflow");
        new_cap *= 2;
    }

    if (new_cap > m->cap)
        MMAP_REHASH_(m, new_cap, a);
}

/* ---- public: pool_reserve ---- */

/*
 * Ensure the pool backing array has room for at least min_nodes nodes without
 * reallocating.  No-op when capacity is already sufficient.
 */
static inline void MMAP_POOL_RESERVE_(MMAP_POOL_T_ *pool, uint32_t min_nodes,
                                       rc_arena *a)
{
    if (min_nodes <= pool->cap) return;
    RC_ASSERT(a != NULL && "hash multimap: arena must not be NULL");

    uint32_t new_cap = pool->cap ? pool->cap : 8;
    while (new_cap < min_nodes) {
        RC_ASSERT(new_cap <= UINT32_MAX / 2 &&
                     "hash multimap: pool capacity overflow");
        new_cap *= 2;
    }

    pool->nodes = rc_arena_realloc(a, pool->nodes,
                                    (uint32_t)((size_t)pool->cap * sizeof(MMAP_NODE_T_)),
                                    (uint32_t)((size_t)new_cap * sizeof(MMAP_NODE_T_)));
    RC_ASSERT(pool->nodes != NULL && "hash multimap: pool OOM");
    pool->cap = new_cap;
}

/* ---- public: add ---- */

/*
 * Insert (key, val).  Returns 1 if key was new, 0 if key already had entries.
 * The new value is prepended to the key's chain.
 *
 * If inserting into an EMPTY slot would push the load factor to or above 3/4,
 * the table is doubled and the probe is retried.  The outer loop runs at most
 * twice.  Arena may be NULL only if neither the table nor the pool need to grow.
 */
static inline int MMAP_ADD_(MMAP_NAME *m, MMAP_KEY_T key, MMAP_VAL_T val,
                              rc_arena *a)
{
    RC_ASSERT(m->pool != NULL && "hash multimap: pool must not be NULL");

    if (!m->data)
        MMAP_REHASH_(m, 8, a);

    for (;;) {
        uint8_t    *states = (uint8_t *)m->data;
        MMAP_KEY_T *keys   = (MMAP_KEY_T *)(m->data + MMAP_KEYS_OFF_(m->cap));
        uint32_t   *heads  = (uint32_t *)(m->data + MMAP_HEADS_OFF_(m->cap));
        uint32_t    mask   = m->cap - 1;
        uint32_t    i      = (uint32_t)MMAP_HASH(key) & mask;
        uint32_t    tomb   = m->cap;   /* index of first tombstone seen, or cap */

        for (;;) {
            uint8_t s = states[i];
            if (s == RC_HASH_MMAP_SLOT_OCCUPIED && MMAP_EQUAL(keys[i], key)) {
                /* Key exists: prepend new node to its chain.
                 * MMAP_ALLOC_NODE_ may realloc pool->nodes but not m->data,
                 * so heads[i] remains valid after the call.                  */
                uint32_t node_idx = MMAP_ALLOC_NODE_(m->pool, a);
                m->pool->nodes[node_idx].val  = val;
                m->pool->nodes[node_idx].next = heads[i];
                heads[i] = node_idx;
                return 0;
            }
            if (s == RC_HASH_MMAP_SLOT_TOMBSTONE && tomb == m->cap) tomb = i;
            if (s == RC_HASH_MMAP_SLOT_EMPTY) break;
            i = (i + 1) & mask;
        }

        /* Key absent.  i is the first EMPTY slot; tomb is the first tombstone
         * seen, or m->cap if none.                                            */

        if (tomb != m->cap) {
            /* Reuse tombstone: used count unchanged, count increases. */
            uint32_t node_idx = MMAP_ALLOC_NODE_(m->pool, a);
            m->pool->nodes[node_idx].val  = val;
            m->pool->nodes[node_idx].next = RC_INDEX_NONE;
            states[tomb] = RC_HASH_MMAP_SLOT_OCCUPIED;
            keys[tomb]   = key;
            heads[tomb]  = node_idx;
            m->count++;
            return 1;
        }

        /* Would consume an EMPTY slot.  Check load-factor threshold.
         * Written as used+1 <= cap - cap/4 to avoid uint32_t overflow. */
        if (m->used + 1 <= m->cap - m->cap / 4) {
            uint32_t node_idx = MMAP_ALLOC_NODE_(m->pool, a);
            m->pool->nodes[node_idx].val  = val;
            m->pool->nodes[node_idx].next = RC_INDEX_NONE;
            states[i] = RC_HASH_MMAP_SLOT_OCCUPIED;
            keys[i]   = key;
            heads[i]  = node_idx;
            m->count++;
            m->used++;
            return 1;
        }

        /* Load factor would exceed 3/4: double capacity and re-probe. */
        RC_ASSERT(m->cap <= UINT32_MAX / 2 &&
                     "hash multimap: capacity overflow");
        MMAP_REHASH_(m, m->cap * 2, a);
    }
}

/* ---- public: remove_all ---- */

/*
 * Remove all values for key.  Returns 1 if key was present, 0 if absent.
 * All nodes for key are returned to the pool's free-list.  No arena needed.
 * Leaves a TOMBSTONE in the slot so existing probe chains remain intact.
 */
static inline int MMAP_REMOVE_ALL_(MMAP_NAME *m, MMAP_KEY_T key)
{
    RC_ASSERT(m->pool != NULL && "hash multimap: pool must not be NULL");
    if (!m->data) return 0;

    uint8_t    *states = (uint8_t *)m->data;
    MMAP_KEY_T *keys   = (MMAP_KEY_T *)(m->data + MMAP_KEYS_OFF_(m->cap));
    uint32_t   *heads  = (uint32_t *)(m->data + MMAP_HEADS_OFF_(m->cap));
    uint32_t    mask   = m->cap - 1;
    uint32_t    i      = (uint32_t)MMAP_HASH(key) & mask;

    for (;;) {
        uint8_t s = states[i];
        if (s == RC_HASH_MMAP_SLOT_EMPTY) return 0;
        if (s == RC_HASH_MMAP_SLOT_OCCUPIED && MMAP_EQUAL(keys[i], key)) {
            MMAP_FREE_CHAIN_(m->pool, heads[i]);
            states[i] = RC_HASH_MMAP_SLOT_TOMBSTONE;
            m->count--;
            return 1;
        }
        i = (i + 1) & mask;
    }
}

/* ---- public: find_head ---- */

/*
 * Returns the pool index of the first node for key, or RC_INDEX_NONE if absent.
 */
static inline uint32_t MMAP_FIND_HEAD_(const MMAP_NAME *m, MMAP_KEY_T key)
{
    if (!m->data) return RC_INDEX_NONE;

    const uint8_t    *states = (const uint8_t *)m->data;
    const MMAP_KEY_T *keys   = (const MMAP_KEY_T *)(m->data + MMAP_KEYS_OFF_(m->cap));
    const uint32_t   *heads  = (const uint32_t *)(m->data + MMAP_HEADS_OFF_(m->cap));
    uint32_t          mask   = m->cap - 1;
    uint32_t          i      = (uint32_t)MMAP_HASH(key) & mask;

    for (;;) {
        uint8_t s = states[i];
        if (s == RC_HASH_MMAP_SLOT_EMPTY) return RC_INDEX_NONE;
        if (s == RC_HASH_MMAP_SLOT_OCCUPIED && MMAP_EQUAL(keys[i], key))
            return heads[i];
        i = (i + 1) & mask;
    }
}

/* ---- public: contains ---- */

/* Returns 1 if key is present, 0 if absent. */
static inline int MMAP_CONTAINS_(const MMAP_NAME *m, MMAP_KEY_T key)
{
    return MMAP_FIND_HEAD_(m, key) != RC_INDEX_NONE;
}

/* ---- public: next ---- */

/*
 * Returns the smallest slot index >= pos holding a live key, or m->cap if
 * no such slot exists.
 *
 * Use pos=0 to begin iteration; advance with NAME_next(m, i+1).
 * When m->data is NULL the map is empty and cap is 0, so the caller's loop
 * condition (i < m->cap) is immediately false.
 */
static inline uint32_t MMAP_NEXT_(const MMAP_NAME *m, uint32_t pos)
{
    if (!m->data) return 0;
    const uint8_t *states = (const uint8_t *)m->data;
    while (pos < m->cap && states[pos] != RC_HASH_MMAP_SLOT_OCCUPIED)
        pos++;
    return pos;
}

/* ---- public: key_at ---- */

/*
 * Returns the key stored at slot i.
 * Only valid when slot i is OCCUPIED (i.e. NAME_next returned i and
 * i < m->cap).
 */
static inline MMAP_KEY_T MMAP_KEY_AT_(const MMAP_NAME *m, uint32_t i)
{
    const MMAP_KEY_T *keys = (const MMAP_KEY_T *)(m->data + MMAP_KEYS_OFF_(m->cap));
    return keys[i];
}

/* ---- public: head_at ---- */

/*
 * Returns the pool index of the first node for slot i.
 * Only valid when slot i is OCCUPIED.
 */
static inline uint32_t MMAP_HEAD_AT_(const MMAP_NAME *m, uint32_t i)
{
    const uint32_t *heads = (const uint32_t *)(m->data + MMAP_HEADS_OFF_(m->cap));
    return heads[i];
}

/* ---- public: node_val ---- */

/*
 * Returns a pointer to the value of pool node j.
 * Invalidated by any add that causes pool reallocation.
 */
static inline MMAP_VAL_T *MMAP_NODE_VAL_(MMAP_NAME *m, uint32_t j)
{
    return &m->pool->nodes[j].val;
}

/* ---- public: node_next ---- */

/*
 * Returns the raw index of the next node in the chain after j, or
 * RC_INDEX_NONE if j is the last node.
 */
static inline uint32_t MMAP_NODE_NEXT_(const MMAP_NAME *m, uint32_t j)
{
    return m->pool->nodes[j].next;
}

/* ---- cleanup ---- */

#undef MMAP_KEYS_OFF_
#undef MMAP_HEADS_OFF_
#undef MMAP_BLOCK_SIZE_
#undef MMAP_NODE_T_
#undef MMAP_POOL_T_
#undef MMAP_REHASH_
#undef MMAP_ALLOC_NODE_
#undef MMAP_FREE_CHAIN_
#undef MMAP_POOL_MAKE_
#undef MMAP_MAKE_
#undef MMAP_RESERVE_
#undef MMAP_POOL_RESERVE_
#undef MMAP_ADD_
#undef MMAP_REMOVE_ALL_
#undef MMAP_FIND_HEAD_
#undef MMAP_CONTAINS_
#undef MMAP_NEXT_
#undef MMAP_KEY_AT_
#undef MMAP_HEAD_AT_
#undef MMAP_NODE_VAL_
#undef MMAP_NODE_NEXT_

#undef MMAP_EQUAL
#undef MMAP_NAME
#undef MMAP_KEY_T
#undef MMAP_VAL_T
#undef MMAP_HASH
