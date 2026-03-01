/*
 * hash_set.h - template header: open-addressing hash set (SoA layout).
 *
 * Implements a flat hash set using linear probing with tombstone deletion.
 * A zero-initialised SET_NAME struct is a valid empty set.
 *
 * The load factor (live + tombstone entries / capacity) is kept strictly
 * below 3/4 to bound average probe lengths.  Capacity is always a power
 * of two so slot indices can be computed with a cheap bitwise AND.
 *
 * Define before including:
 *   SET_KEY_T        key type (required)
 *   SET_HASH(key)    hash expression; must expand to an integer type (required)
 *   SET_EQUAL(a, b)  equality expression for two keys
 *                    (optional; default: (a) == (b))
 *   SET_NAME         set struct name
 *                    (optional; default: rc_set_##SET_KEY_T)
 *                    Must be provided when instantiating multiple sets
 *                    for the same key type.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Memory layout
 * -------------
 * All per-slot data is stored in a single arena allocation sliced into
 * two tightly-packed arrays.  For capacity N, the block layout is:
 *
 *   [0,          N)               uint8_t  states[N]
 *   [keys_off,   keys_off + N*K)  KEY      keys[N]    (K = sizeof(KEY))
 *
 * keys_off = align_up(N, alignof(KEY))
 *
 * State encoding: see rc_hash_slot_state (RC_SLOT_EMPTY, RC_SLOT_OCCUPIED,
 * RC_SLOT_TOMBSTONE).  Only the states portion of each block is zeroed on
 * allocation; keys are only read when state == RC_SLOT_OCCUPIED.
 *
 * Generated types
 * ---------------
 *   SET_NAME     { char *data; uint32_t count;
 *                  uint32_t used; uint32_t cap; }     — hash set
 *
 * Zero-initialise SET_NAME to obtain an empty set:
 *   rc_set_int s = {0};
 *
 * Generated functions (all static inline)
 * ----------------------------------------
 *
 *   void SET_NAME_reserve(SET_NAME *s, uint32_t min_count, rc_arena *a)
 *        Ensure the set can hold at least min_count live entries without
 *        triggering a rehash.  No-op if capacity is already sufficient.
 *        Asserts if a == NULL and a rehash is needed.
 *
 *   int  SET_NAME_add(SET_NAME *s, SET_KEY_T key, rc_arena *a)
 *        Insert key.  Returns 1 if key was new, 0 if already present.
 *        Arena may be NULL if capacity is sufficient.
 *
 *   int  SET_NAME_remove(SET_NAME *s, SET_KEY_T key)
 *        Remove key.  Returns 1 if key was present, 0 if absent.
 *        No arena needed.
 *
 *   int  SET_NAME_contains(SET_NAME *s, SET_KEY_T key)
 *        Returns 1 if key is present, 0 if absent.
 *
 *   uint32_t    SET_NAME_next(const SET_NAME *s, uint32_t pos)
 *        Returns the smallest index >= pos that holds a live entry,
 *        or s->cap if no such index exists.  Start iteration at pos=0;
 *        advance with pos = NAME_next(s, i+1).
 *
 *   SET_KEY_T   SET_NAME_key_at(const SET_NAME *s, uint32_t i)
 *        Returns the key stored at slot i.  Only valid when slot i is
 *        OCCUPIED (i.e. NAME_next returned i and i < s->cap).
 *
 * Iteration idiom:
 *   for (uint32_t i = NAME_next(s, 0); i < s->cap; i = NAME_next(s, i+1)) {
 *       SET_KEY_T k = NAME_key_at(s, i);
 *       // use k
 *   }
 *
 * SET_HASH conventions
 * --------------------
 *   SET_HASH(key)    — no context; must expand to an integer type
 *
 * SET_EQUAL conventions
 * ---------------------
 *   SET_EQUAL(a, b)  — true iff a and b are equal keys
 *   Default: (a) == (b)
 *
 * Example (int set):
 *   #define SET_KEY_T   int
 *   #define SET_HASH(k) ((uint32_t)(unsigned int)(k) * 2654435761u)
 *   #include "richc/template/hash_set.h"
 *   // defines: rc_set_int, rc_set_int_add, rc_set_int_remove,
 *   //          rc_set_int_contains, rc_set_int_reserve
 */

#include <stdint.h>
#include <string.h>
#include "richc/template_util.h"
#include "richc/arena.h"

#ifndef SET_KEY_T
#  error "SET_KEY_T must be defined before including hash_set.h"
#endif
#ifndef SET_HASH
#  error "SET_HASH must be defined before including hash_set.h"
#endif

#ifndef SET_EQUAL
#  define SET_EQUAL(a, b) ((a) == (b))
#endif

#ifndef SET_NAME
#  define SET_NAME RC_CONCAT(rc_set_, SET_KEY_T)
#endif

/* ---- once-only helpers ---- */

#ifndef RC_HASH_SET_H_
#define RC_HASH_SET_H_
/* Round n up to the nearest multiple of a; a must be a power of two. */
#define RC_SET_ALIGN_UP_(n, a) (((size_t)(n) + (a) - 1) & ~((a) - 1))
#endif

#ifndef RC_HASH_SLOT_STATE_H_
#define RC_HASH_SLOT_STATE_H_
typedef enum : uint8_t {
    RC_SLOT_EMPTY     = 0,
    RC_SLOT_OCCUPIED  = 1,
    RC_SLOT_TOMBSTONE = 2
} rc_hash_slot_state;
#endif

/* ---- per-type layout macros ---- */

/*
 * Byte offset of keys[] from the block base for a given capacity.
 * Keys begin immediately after states[], padded to KEY alignment.
 */
#define SET_KEYS_OFF_(cap) \
    RC_SET_ALIGN_UP_((size_t)(cap), alignof(SET_KEY_T))

/* Total byte size of the SoA block for a given capacity. */
#define SET_BLOCK_SIZE_(cap) \
    (SET_KEYS_OFF_(cap) + (size_t)(cap) * sizeof(SET_KEY_T))

/* ---- internal helper name macros ---- */

#define SET_REHASH_   RC_CONCAT(SET_NAME, _rehash_)
#define SET_RESERVE_  RC_CONCAT(SET_NAME, _reserve)
#define SET_ADD_      RC_CONCAT(SET_NAME, _add)
#define SET_REMOVE_   RC_CONCAT(SET_NAME, _remove)
#define SET_CONTAINS_ RC_CONCAT(SET_NAME, _contains)
#define SET_NEXT_     RC_CONCAT(SET_NAME, _next)
#define SET_KEY_AT_   RC_CONCAT(SET_NAME, _key_at)

/* ---- generated type ---- */

/*
 * Hash set.
 *
 * data  — base of the SoA allocation (NULL for a zero-initialised empty set)
 * count — number of live (OCCUPIED) entries
 * used  — live + TOMBSTONE entries; used for load-factor calculations
 * cap   — slot count; always a power of two, or zero for an empty set
 *
 * Derive the two arrays at runtime as:
 *   states = (uint8_t *)data
 *   keys   = (SET_KEY_T *)(data + SET_KEYS_OFF_(cap))
 *
 * Zero-initialise to get an empty, valid set.
 */
typedef struct {
    char    *data;
    uint32_t count;
    uint32_t used;
    uint32_t cap;
} SET_NAME;

/* ---- internal: rehash ---- */

/*
 * Allocate a fresh SoA block of new_cap slots, zero the states array,
 * re-insert all live entries from the current block, and discard tombstones.
 * new_cap must be a power of two.
 */
static inline void SET_REHASH_(SET_NAME *set, uint32_t new_cap, rc_arena *a)
{
    RC_ASSERT((new_cap & (new_cap - 1)) == 0 &&
                 "hash set: new_cap must be a power of two");
    RC_ASSERT(a != NULL && "hash set: arena required for rehash");

    size_t block_size = SET_BLOCK_SIZE_(new_cap);
    RC_ASSERT(block_size <= UINT32_MAX && "hash set: SoA block exceeds 4 GB");
    char *new_data = rc_arena_alloc(a, (uint32_t)block_size);
    RC_ASSERT(new_data != NULL && "hash set: arena OOM");

    /* Zero only the states array; keys are only read when state == RC_SLOT_OCCUPIED. */
    memset(new_data, 0, new_cap);

    uint8_t   *new_states = (uint8_t *)new_data;
    SET_KEY_T *new_keys   = (SET_KEY_T *)(new_data + SET_KEYS_OFF_(new_cap));
    uint32_t   mask       = new_cap - 1;

    if (set->data) {
        uint8_t   *old_states = (uint8_t *)set->data;
        SET_KEY_T *old_keys   = (SET_KEY_T *)(set->data + SET_KEYS_OFF_(set->cap));

        for (uint32_t i = 0; i < set->cap; i++) {
            if (old_states[i] != RC_SLOT_OCCUPIED) continue;   /* skip EMPTY / TOMBSTONE */
            uint32_t j = (uint32_t)SET_HASH(old_keys[i]) & mask;
            while (new_states[j] == RC_SLOT_OCCUPIED) j = (j + 1) & mask;
            new_states[j] = RC_SLOT_OCCUPIED;
            new_keys[j]   = old_keys[i];
        }
    }

    set->data = new_data;
    set->cap  = new_cap;
    set->used = set->count;   /* tombstones gone */
}

/* ---- public: reserve ---- */

/*
 * Ensure the set can hold at least min_count live entries without rehashing.
 * Finds the smallest power-of-two capacity c such that floor(c * 3/4) >=
 * min_count, then rehashes to c if c > current capacity.
 */
static inline void SET_RESERVE_(SET_NAME *set, uint32_t min_count, rc_arena *a)
{
    if (min_count == 0) return;

    uint32_t new_cap = (set->cap > 8) ? set->cap : 8;
    /* new_cap - new_cap/4 == floor(new_cap * 3/4) for powers of two >= 4. *
     * Keep doubling until that threshold is >= min_count.                 */
    while (new_cap - new_cap / 4 < min_count) {
        RC_ASSERT(new_cap <= UINT32_MAX / 2 &&
                     "hash set: capacity overflow");
        new_cap *= 2;
    }

    if (new_cap > set->cap)
        SET_REHASH_(set, new_cap, a);
}

/* ---- public: add ---- */

/*
 * Insert key.  Returns 1 if key was new, 0 if already present.
 *
 * If inserting into an EMPTY slot would push the load factor to or above
 * 3/4, the block is doubled via SET_REHASH_ and the probe is retried.
 * The outer loop runs at most twice.
 *
 * Arena may be NULL when capacity is already sufficient.
 */
static inline int SET_ADD_(SET_NAME *set, SET_KEY_T key, rc_arena *a)
{
    if (!set->data)
        SET_REHASH_(set, 8, a);

    for (;;) {
        uint8_t   *states = (uint8_t *)set->data;
        SET_KEY_T *keys   = (SET_KEY_T *)(set->data + SET_KEYS_OFF_(set->cap));
        uint32_t   mask   = set->cap - 1;
        uint32_t   i      = (uint32_t)SET_HASH(key) & mask;
        uint32_t   tomb   = set->cap;   /* index of first tombstone seen, or cap */

        for (;;) {
            uint8_t s = states[i];
            if (s == RC_SLOT_OCCUPIED && SET_EQUAL(keys[i], key))
                return 0;              /* already present */
            if (s == RC_SLOT_TOMBSTONE && tomb == set->cap) tomb = i;
            if (s == RC_SLOT_EMPTY) break;   /* EMPTY: key not in set */
            i = (i + 1) & mask;
        }

        /* Key absent.  i is the first EMPTY slot; tomb is the first
         * tombstone seen, or set->cap if none.                        */

        if (tomb != set->cap) {
            /* Reuse tombstone: used count unchanged, count increases. */
            states[tomb] = RC_SLOT_OCCUPIED;
            keys[tomb]   = key;
            set->count++;
            return 1;
        }

        /* Would consume an EMPTY slot.  Check load-factor threshold.
         * Written as used+1 <= cap - cap/4 to avoid uint32_t overflow. */
        if (set->used + 1 <= set->cap - set->cap / 4) {
            states[i] = RC_SLOT_OCCUPIED;
            keys[i]   = key;
            set->count++;
            set->used++;
            return 1;
        }

        /* Load factor would exceed 3/4: double capacity and re-probe. */
        RC_ASSERT(set->cap <= UINT32_MAX / 2 &&
                     "hash set: capacity overflow");
        SET_REHASH_(set, set->cap * 2, a);
    }
}

/* ---- public: remove ---- */

/*
 * Remove key.  Returns 1 if it was present, 0 if absent.
 * Leaves a TOMBSTONE in the slot so existing probe chains remain intact.
 * No arena needed.
 */
static inline int SET_REMOVE_(SET_NAME *set, SET_KEY_T key)
{
    if (!set->data) return 0;

    uint8_t   *states = (uint8_t *)set->data;
    SET_KEY_T *keys   = (SET_KEY_T *)(set->data + SET_KEYS_OFF_(set->cap));
    uint32_t   mask   = set->cap - 1;
    uint32_t   i      = (uint32_t)SET_HASH(key) & mask;

    for (;;) {
        uint8_t s = states[i];
        if (s == RC_SLOT_EMPTY) return 0;   /* EMPTY: not in set */
        if (s == RC_SLOT_OCCUPIED && SET_EQUAL(keys[i], key)) {
            states[i] = RC_SLOT_TOMBSTONE;  /* mark TOMBSTONE */
            set->count--;
            return 1;
        }
        i = (i + 1) & mask;
    }
}

/* ---- public: contains ---- */

/* Returns 1 if key is present, 0 if absent. */
static inline int SET_CONTAINS_(SET_NAME *set, SET_KEY_T key)
{
    if (!set->data) return 0;

    uint8_t   *states = (uint8_t *)set->data;
    SET_KEY_T *keys   = (SET_KEY_T *)(set->data + SET_KEYS_OFF_(set->cap));
    uint32_t   mask   = set->cap - 1;
    uint32_t   i      = (uint32_t)SET_HASH(key) & mask;

    for (;;) {
        uint8_t s = states[i];
        if (s == RC_SLOT_EMPTY) return 0;
        if (s == RC_SLOT_OCCUPIED && SET_EQUAL(keys[i], key)) return 1;
        i = (i + 1) & mask;
    }
}

/* ---- public: next ---- */

/*
 * Returns the smallest index >= pos that holds a live (OCCUPIED) entry,
 * or set->cap if no such index exists.
 *
 * Use pos=0 to begin iteration; advance with NAME_next(set, i+1).
 * When set->data is NULL the set is empty and cap is 0, so the caller's
 * loop condition (i < set->cap) is immediately false.
 */
static inline uint32_t SET_NEXT_(const SET_NAME *set, uint32_t pos)
{
    if (!set->data) return 0;
    uint8_t *states = (uint8_t *)set->data;
    while (pos < set->cap && states[pos] != RC_SLOT_OCCUPIED)
        pos++;
    return pos;
}

/* ---- public: key_at ---- */

/*
 * Returns the key stored at slot i.
 * Only valid when slot i is OCCUPIED (i.e. SET_NEXT_ returned i and
 * i < set->cap).
 */
static inline SET_KEY_T SET_KEY_AT_(const SET_NAME *set, uint32_t i)
{
    SET_KEY_T *keys = (SET_KEY_T *)(set->data + SET_KEYS_OFF_(set->cap));
    return keys[i];
}

/* ---- cleanup ---- */

#undef SET_KEYS_OFF_
#undef SET_BLOCK_SIZE_
#undef SET_REHASH_
#undef SET_RESERVE_
#undef SET_ADD_
#undef SET_REMOVE_
#undef SET_CONTAINS_
#undef SET_NEXT_
#undef SET_KEY_AT_

#undef SET_EQUAL
#undef SET_NAME
#undef SET_KEY_T
#undef SET_HASH
