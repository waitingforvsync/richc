/*
 * hash_map.h - template header: open-addressing hash map (SoA layout).
 *
 * Implements a flat hash map using linear probing with tombstone deletion.
 * A zero-initialised MAP_NAME struct is a valid empty map.
 *
 * The load factor (live + tombstone entries / capacity) is kept strictly
 * below 3/4 to bound average probe lengths.  Capacity is always a power
 * of two so slot indices can be computed with a cheap bitwise AND.
 *
 * Define before including:
 *   MAP_KEY_T        key type (required)
 *   MAP_VAL_T        value type (required)
 *   MAP_HASH(key)    hash expression; must expand to an integer type (required)
 *   MAP_EQUAL(a, b)  equality expression for two keys
 *                    (optional; default: (a) == (b))
 *   MAP_NAME         map struct name
 *                    (optional; default: rc_map_##MAP_KEY_T)
 *                    Must be provided when instantiating multiple maps
 *                    for the same key type.
 *   MAP_KV_NAME      KV pair struct name
 *                    (optional; default: MAP_NAME##_kv)
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Memory layout
 * -------------
 * All per-slot data is stored in a single arena allocation sliced into
 * three tightly-packed arrays.  For capacity N, the block layout is:
 *
 *   [0,          N)               uint8_t  states[N]
 *   [keys_off,   keys_off + N*K)  KEY      keys[N]    (K = sizeof(KEY))
 *   [vals_off,   vals_off + N*V)  VAL      vals[N]    (V = sizeof(VAL))
 *
 * keys_off = align_up(N,          alignof(KEY))
 * vals_off = align_up(keys_off + N*K, alignof(VAL))
 *
 * State encoding: see rc_hash_slot_state (RC_HASH_MAP_SLOT_EMPTY, RC_HASH_MAP_SLOT_OCCUPIED,
 * RC_HASH_MAP_SLOT_TOMBSTONE).  Only the states portion of each block is zeroed on
 * allocation; keys and vals are only read when state == RC_HASH_MAP_SLOT_OCCUPIED.
 *
 * Generated types
 * ---------------
 *   MAP_KV_NAME  { MAP_KEY_T key; MAP_VAL_T val; }   — public KV pair
 *   MAP_NAME     { char *data; uint32_t count;
 *                  uint32_t used; uint32_t cap; }     — hash map
 *
 * Zero-initialise MAP_NAME to obtain an empty map:
 *   rc_map_int m = {0};
 *
 * Generated functions (all static inline)
 * ----------------------------------------
 *
 *   void NAME_reserve(MAP_NAME *m, uint32_t min_count, rc_arena *a)
 *        Ensure the map can hold at least min_count live entries without
 *        triggering a rehash.  No-op if capacity is already sufficient.
 *        Asserts if a == NULL and a rehash is needed.
 *
 *   int  NAME_add(MAP_NAME *m, MAP_KEY_T key, MAP_VAL_T val, rc_arena *a)
 *        Insert or update.  Returns 1 if key was new, 0 if an existing
 *        entry was updated.  Arena may be NULL if capacity is sufficient.
 *
 *   int  NAME_remove(MAP_NAME *m, MAP_KEY_T key)
 *        Remove key.  Returns 1 if key was present, 0 if absent.
 *        No arena needed.
 *
 *   MAP_VAL_T *NAME_find(MAP_NAME *m, MAP_KEY_T key)
 *        Returns a pointer to the stored value, or NULL if absent.
 *        Invalidated by any subsequent add that causes a rehash.
 *
 *   int  NAME_contains(MAP_NAME *m, MAP_KEY_T key)
 *        Returns 1 if key is present, 0 if absent.
 *
 *   uint32_t    NAME_next(const MAP_NAME *m, uint32_t pos)
 *        Returns the smallest index >= pos that holds a live entry,
 *        or m->cap if no such index exists.  Start iteration at pos=0;
 *        advance with pos = NAME_next(m, i+1).
 *
 *   MAP_KEY_T   NAME_key_at(const MAP_NAME *m, uint32_t i)
 *        Returns the key stored at slot i.  Only valid when slot i is
 *        OCCUPIED (i.e. NAME_next returned i and i < m->cap).
 *
 *   MAP_VAL_T * NAME_val_at(MAP_NAME *m, uint32_t i)
 *        Returns a pointer to the value stored at slot i.  Only valid
 *        when slot i is OCCUPIED.  Invalidated by any add that rehashes.
 *
 * Iteration idiom:
 *   for (uint32_t i = NAME_next(m, 0); i < m->cap; i = NAME_next(m, i+1)) {
 *       MAP_KEY_T k = NAME_key_at(m, i);
 *       MAP_VAL_T *v = NAME_val_at(m, i);
 *       // use k, v
 *   }
 *
 * MAP_HASH conventions
 * --------------------
 *   MAP_HASH(key)    — no context; must expand to an integer type
 *
 * MAP_EQUAL conventions
 * ---------------------
 *   MAP_EQUAL(a, b)  — true iff a and b are equal keys
 *   Default: (a) == (b)
 *
 * Example (int → int):
 *   #define MAP_KEY_T   int
 *   #define MAP_VAL_T   int
 *   #define MAP_HASH(k) ((uint32_t)(unsigned int)(k) * 2654435761u)
 *   #include "richc/hash_map.h"
 *   // defines: rc_map_int, rc_map_int_add, rc_map_int_remove,
 *   //          rc_map_int_find, rc_map_int_contains, rc_map_int_reserve
 */

#include "richc/template_util.h"

#ifndef MAP_KEY_T
#  error "MAP_KEY_T must be defined before including hash_map.h"
#endif
#ifndef MAP_VAL_T
#  error "MAP_VAL_T must be defined before including hash_map.h"
#endif
#ifndef MAP_HASH
#  error "MAP_HASH must be defined before including hash_map.h"
#endif

#ifndef MAP_EQUAL
#  define MAP_EQUAL(a, b) ((a) == (b))
#endif

#ifndef MAP_NAME
#  define MAP_NAME    RC_CONCAT(rc_map_, MAP_KEY_T)
#endif

#ifndef MAP_KV_NAME
#  define MAP_KV_NAME RC_CONCAT(MAP_NAME, _kv)
#endif

/* ---- once-only helpers ---- */

#ifndef RC_HASH_MAP_H_
#define RC_HASH_MAP_H_
#include <stdint.h>
#include <string.h>
#include "richc/arena.h"
/* Round n up to the nearest multiple of a; a must be a power of two. */
#define RC_MAP_ALIGN_UP_(n, a) (((size_t)(n) + (a) - 1) & ~((a) - 1))
typedef enum : uint8_t {
    RC_HASH_MAP_SLOT_EMPTY     = 0,
    RC_HASH_MAP_SLOT_OCCUPIED  = 1,
    RC_HASH_MAP_SLOT_TOMBSTONE = 2
} rc_hash_map_slot_state;
#endif

/* ---- per-type layout macros ---- */

/*
 * Byte offset of keys[] from the block base for a given capacity.
 * Keys begin immediately after states[], padded to KEY alignment.
 */
#define MAP_KEYS_OFF_(cap) \
    RC_MAP_ALIGN_UP_((size_t)(cap), alignof(MAP_KEY_T))

/*
 * Byte offset of vals[] from the block base for a given capacity.
 * Vals begin immediately after keys[], padded to VAL alignment.
 */
#define MAP_VALS_OFF_(cap) \
    RC_MAP_ALIGN_UP_(MAP_KEYS_OFF_(cap) + (size_t)(cap) * sizeof(MAP_KEY_T), \
                        alignof(MAP_VAL_T))

/* Total byte size of the SoA block for a given capacity. */
#define MAP_BLOCK_SIZE_(cap) \
    (MAP_VALS_OFF_(cap) + (size_t)(cap) * sizeof(MAP_VAL_T))

/* ---- internal helper name macros ---- */

#define MAP_REHASH_   RC_CONCAT(MAP_NAME, _rehash_)
#define MAP_RESERVE_  RC_CONCAT(MAP_NAME, _reserve)
#define MAP_ADD_      RC_CONCAT(MAP_NAME, _add)
#define MAP_REMOVE_   RC_CONCAT(MAP_NAME, _remove)
#define MAP_FIND_     RC_CONCAT(MAP_NAME, _find)
#define MAP_CONTAINS_ RC_CONCAT(MAP_NAME, _contains)
#define MAP_NEXT_     RC_CONCAT(MAP_NAME, _next)
#define MAP_KEY_AT_   RC_CONCAT(MAP_NAME, _key_at)
#define MAP_VAL_AT_   RC_CONCAT(MAP_NAME, _val_at)

/* ---- generated types ---- */

/* Public KV pair: key and value without internal bookkeeping. */
typedef struct {
    MAP_KEY_T key;
    MAP_VAL_T val;
} MAP_KV_NAME;

/*
 * Hash map.
 *
 * data  — base of the SoA allocation (NULL for a zero-initialised empty map)
 * count — number of live (OCCUPIED) entries
 * used  — live + TOMBSTONE entries; used for load-factor calculations
 * cap   — slot count; always a power of two, or zero for an empty map
 *
 * Derive the three arrays at runtime as:
 *   states = (uint8_t *)data
 *   keys   = (MAP_KEY_T *)(data + MAP_KEYS_OFF_(cap))
 *   vals   = (MAP_VAL_T *)(data + MAP_VALS_OFF_(cap))
 *
 * Zero-initialise to get an empty, valid map.
 */
typedef struct {
    char    *data;
    uint32_t count;
    uint32_t used;
    uint32_t cap;
} MAP_NAME;

/* ---- internal: rehash ---- */

/*
 * Allocate a fresh SoA block of new_cap slots, zero the states array,
 * re-insert all live entries from the current block, and discard tombstones.
 * new_cap must be a power of two.
 */
static inline void MAP_REHASH_(MAP_NAME *map, uint32_t new_cap, rc_arena *a)
{
    RC_ASSERT((new_cap & (new_cap - 1)) == 0 &&
                 "hash map: new_cap must be a power of two");
    RC_ASSERT(a != NULL && "hash map: arena required for rehash");

    size_t block_size = MAP_BLOCK_SIZE_(new_cap);
    RC_ASSERT(block_size <= UINT32_MAX && "hash map: SoA block exceeds 4 GB");
    char *new_data = rc_arena_alloc(a, (uint32_t)block_size);
    RC_ASSERT(new_data != NULL && "hash map: arena OOM");

    /* Zero only the states array; keys/vals are only read when state == RC_HASH_MAP_SLOT_OCCUPIED. */
    memset(new_data, 0, new_cap);

    uint8_t   *new_states = (uint8_t *)new_data;
    MAP_KEY_T *new_keys   = (MAP_KEY_T *)(new_data + MAP_KEYS_OFF_(new_cap));
    MAP_VAL_T *new_vals   = (MAP_VAL_T *)(new_data + MAP_VALS_OFF_(new_cap));
    uint32_t   mask       = new_cap - 1;

    if (map->data) {
        uint8_t   *old_states = (uint8_t *)map->data;
        MAP_KEY_T *old_keys   = (MAP_KEY_T *)(map->data + MAP_KEYS_OFF_(map->cap));
        MAP_VAL_T *old_vals   = (MAP_VAL_T *)(map->data + MAP_VALS_OFF_(map->cap));

        for (uint32_t i = 0; i < map->cap; i++) {
            if (old_states[i] != RC_HASH_MAP_SLOT_OCCUPIED) continue;   /* skip EMPTY / TOMBSTONE */
            uint32_t j = (uint32_t)MAP_HASH(old_keys[i]) & mask;
            while (new_states[j] == RC_HASH_MAP_SLOT_OCCUPIED) j = (j + 1) & mask;
            new_states[j] = RC_HASH_MAP_SLOT_OCCUPIED;
            new_keys[j]   = old_keys[i];
            new_vals[j]   = old_vals[i];
        }
    }

    map->data = new_data;
    map->cap  = new_cap;
    map->used = map->count;   /* tombstones gone */
}

/* ---- public: reserve ---- */

/*
 * Ensure the map can hold at least min_count live entries without rehashing.
 * Finds the smallest power-of-two capacity c such that floor(c * 3/4) >=
 * min_count, then rehashes to c if c > current capacity.
 */
static inline void MAP_RESERVE_(MAP_NAME *map, uint32_t min_count, rc_arena *a)
{
    if (min_count == 0) return;

    uint32_t new_cap = (map->cap > 8) ? map->cap : 8;
    /* new_cap - new_cap/4 == floor(new_cap * 3/4) for powers of two >= 4. *
     * Keep doubling until that threshold is >= min_count.                 */
    while (new_cap - new_cap / 4 < min_count) {
        RC_ASSERT(new_cap <= UINT32_MAX / 2 &&
                     "hash map: capacity overflow");
        new_cap *= 2;
    }

    if (new_cap > map->cap)
        MAP_REHASH_(map, new_cap, a);
}

/* ---- public: add ---- */

/*
 * Insert or update.  Returns 1 if key was new, 0 if an existing value
 * was replaced.
 *
 * If inserting into an EMPTY slot would push the load factor to or above
 * 3/4, the block is doubled via MAP_REHASH_ and the probe is retried.
 * The outer loop runs at most twice.
 *
 * Arena may be NULL when capacity is already sufficient.
 */
static inline int MAP_ADD_(MAP_NAME *map, MAP_KEY_T key, MAP_VAL_T val,
                            rc_arena *a)
{
    if (!map->data)
        MAP_REHASH_(map, 8, a);

    for (;;) {
        uint8_t   *states = (uint8_t *)map->data;
        MAP_KEY_T *keys   = (MAP_KEY_T *)(map->data + MAP_KEYS_OFF_(map->cap));
        MAP_VAL_T *vals   = (MAP_VAL_T *)(map->data + MAP_VALS_OFF_(map->cap));
        uint32_t   mask   = map->cap - 1;
        uint32_t   i      = (uint32_t)MAP_HASH(key) & mask;
        uint32_t   tomb   = map->cap;   /* index of first tombstone seen, or cap */

        for (;;) {
            uint8_t s = states[i];
            if (s == RC_HASH_MAP_SLOT_OCCUPIED && MAP_EQUAL(keys[i], key)) {
                vals[i] = val;   /* update existing entry */
                return 0;
            }
            if (s == RC_HASH_MAP_SLOT_TOMBSTONE && tomb == map->cap) tomb = i;
            if (s == RC_HASH_MAP_SLOT_EMPTY) break;   /* EMPTY: key not in map */
            i = (i + 1) & mask;
        }

        /* Key absent.  i is the first EMPTY slot; tomb is the first
         * tombstone seen, or map->cap if none.                        */

        if (tomb != map->cap) {
            /* Reuse tombstone: used count unchanged, count increases. */
            states[tomb] = RC_HASH_MAP_SLOT_OCCUPIED;
            keys[tomb]   = key;
            vals[tomb]   = val;
            map->count++;
            return 1;
        }

        /* Would consume an EMPTY slot.  Check load-factor threshold.
         * Written as used+1 <= cap - cap/4 to avoid uint32_t overflow. */
        if (map->used + 1 <= map->cap - map->cap / 4) {
            states[i] = RC_HASH_MAP_SLOT_OCCUPIED;
            keys[i]   = key;
            vals[i]   = val;
            map->count++;
            map->used++;
            return 1;
        }

        /* Load factor would exceed 3/4: double capacity and re-probe. */
        RC_ASSERT(map->cap <= UINT32_MAX / 2 &&
                     "hash map: capacity overflow");
        MAP_REHASH_(map, map->cap * 2, a);
    }
}

/* ---- public: remove ---- */

/*
 * Remove key.  Returns 1 if it was present, 0 if absent.
 * Leaves a TOMBSTONE in the slot so existing probe chains remain intact.
 * No arena needed.
 */
static inline int MAP_REMOVE_(MAP_NAME *map, MAP_KEY_T key)
{
    if (!map->data) return 0;

    uint8_t   *states = (uint8_t *)map->data;
    MAP_KEY_T *keys   = (MAP_KEY_T *)(map->data + MAP_KEYS_OFF_(map->cap));
    uint32_t   mask   = map->cap - 1;
    uint32_t   i      = (uint32_t)MAP_HASH(key) & mask;

    for (;;) {
        uint8_t s = states[i];
        if (s == RC_HASH_MAP_SLOT_EMPTY) return 0;   /* EMPTY: not in map */
        if (s == RC_HASH_MAP_SLOT_OCCUPIED && MAP_EQUAL(keys[i], key)) {
            states[i] = RC_HASH_MAP_SLOT_TOMBSTONE;  /* mark TOMBSTONE */
            map->count--;
            return 1;
        }
        i = (i + 1) & mask;
    }
}

/* ---- public: find ---- */

/*
 * Returns a pointer to the stored value for key, or NULL if absent.
 * The pointer is invalidated by any subsequent add that causes a rehash.
 */
static inline MAP_VAL_T *MAP_FIND_(MAP_NAME *map, MAP_KEY_T key)
{
    if (!map->data) return NULL;

    uint8_t   *states = (uint8_t *)map->data;
    MAP_KEY_T *keys   = (MAP_KEY_T *)(map->data + MAP_KEYS_OFF_(map->cap));
    MAP_VAL_T *vals   = (MAP_VAL_T *)(map->data + MAP_VALS_OFF_(map->cap));
    uint32_t   mask   = map->cap - 1;
    uint32_t   i      = (uint32_t)MAP_HASH(key) & mask;

    for (;;) {
        uint8_t s = states[i];
        if (s == RC_HASH_MAP_SLOT_EMPTY) return NULL;
        if (s == RC_HASH_MAP_SLOT_OCCUPIED && MAP_EQUAL(keys[i], key))
            return &vals[i];
        i = (i + 1) & mask;
    }
}

/* ---- public: contains ---- */

/* Returns 1 if key is present, 0 if absent. */
static inline int MAP_CONTAINS_(MAP_NAME *map, MAP_KEY_T key)
{
    return MAP_FIND_(map, key) != NULL;
}

/* ---- public: next ---- */

/*
 * Returns the smallest index >= pos that holds a live (OCCUPIED) entry,
 * or map->cap if no such index exists.
 *
 * Use pos=0 to begin iteration; advance with NAME_next(map, i+1).
 * When map->data is NULL the map is empty and cap is 0, so the caller's
 * loop condition (i < map->cap) is immediately false.
 */
static inline uint32_t MAP_NEXT_(const MAP_NAME *map, uint32_t pos)
{
    if (!map->data) return 0;
    uint8_t *states = (uint8_t *)map->data;
    while (pos < map->cap && states[pos] != RC_HASH_MAP_SLOT_OCCUPIED)
        pos++;
    return pos;
}

/* ---- public: key_at ---- */

/*
 * Returns the key stored at slot i.
 * Only valid when slot i is OCCUPIED (i.e. MAP_NEXT_ returned i and
 * i < map->cap).
 */
static inline MAP_KEY_T MAP_KEY_AT_(const MAP_NAME *map, uint32_t i)
{
    MAP_KEY_T *keys = (MAP_KEY_T *)(map->data + MAP_KEYS_OFF_(map->cap));
    return keys[i];
}

/* ---- public: val_at ---- */

/*
 * Returns a pointer to the value stored at slot i.
 * Only valid when slot i is OCCUPIED.
 * Invalidated by any subsequent add that causes a rehash.
 */
static inline MAP_VAL_T *MAP_VAL_AT_(MAP_NAME *map, uint32_t i)
{
    MAP_VAL_T *vals = (MAP_VAL_T *)(map->data + MAP_VALS_OFF_(map->cap));
    return &vals[i];
}

/* ---- cleanup ---- */

#undef MAP_KEYS_OFF_
#undef MAP_VALS_OFF_
#undef MAP_BLOCK_SIZE_
#undef MAP_REHASH_
#undef MAP_RESERVE_
#undef MAP_ADD_
#undef MAP_REMOVE_
#undef MAP_FIND_
#undef MAP_CONTAINS_
#undef MAP_NEXT_
#undef MAP_KEY_AT_
#undef MAP_VAL_AT_

#undef MAP_EQUAL
#undef MAP_NAME
#undef MAP_KV_NAME
#undef MAP_KEY_T
#undef MAP_VAL_T
#undef MAP_HASH
