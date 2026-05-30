/*
 * pool.h - template header: index-stable free-list object pool.
 *
 * A pool hands out stable uint32_t indices for fixed-size elements, backed by an
 * rc_array (the array template does the growth).  Freed slots are recycled
 * through an in-band singly-linked free list, so an index handed out by alloc
 * stays valid for the element's lifetime regardless of other allocs and frees.
 *
 * Slot encoding
 * -------------
 * Each slot is a union of the element value and a free-list link:
 *
 *   union { uint32_t next_free_; RC_POOL_TYPE value; }
 *
 * A live slot is only ever read through `value`; a free slot only through
 * `next_free_`, which points at the next free slot (RC_INDEX_NONE ends the
 * list).  Every transition fully overwrites the slot, so the two members are
 * never mixed.  Because the link shares storage with the value, RC_POOL_TYPE
 * smaller than uint32_t rounds each slot up to 4 bytes.
 *
 * alloc / free
 * ------------
 * alloc pops the free-list head if there is one (zeroing the reused slot),
 * otherwise appends a fresh zeroed slot.  free pushes the slot onto the free
 * list - except when it is the final slot, which is popped instead so the
 * backing array does not accumulate a dead tail.  (The pop does not cascade into
 * earlier free-list entries, so holes in the middle persist.)
 *
 * Iteration
 * ---------
 * A freed slot is indistinguishable from a live one, so the pool offers no
 * iteration: walk [0, num) only when you have separate liveness information.
 *
 * Define before including:
 *   RC_POOL_TYPE   element type (required)
 *   RC_POOL_NAME   base name for the generated type and functions
 *                  (optional; default rc_pool_<RC_POOL_TYPE>).  The default
 *                  requires a single-identifier element type; pass an explicit
 *                  name for multi-token types.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Generated type and functions (all static inline)
 * -------------------------------------------------
 *   RC_POOL_NAME                                                  the pool
 *
 *   RC_POOL_NAME  NAME_make(uint32_t capacity, rc_arena *arena)
 *   void          NAME_reserve(NAME *pool, uint32_t min_capacity, rc_arena *arena)
 *   void          NAME_reset(NAME *pool)                          // drop all elements
 *   uint32_t      NAME_alloc(NAME *pool, rc_arena *arena)         // index of a zeroed slot
 *   void          NAME_free(NAME *pool, uint32_t index)
 *   RC_POOL_TYPE  NAME_get(const NAME *pool, uint32_t index)
 *   void          NAME_set(NAME *pool, uint32_t index, RC_POOL_TYPE value)
 *   RC_POOL_TYPE *NAME_at(NAME *pool, uint32_t index)             // pointer; see below
 *
 * alloc returns an INDEX, not a pointer.  The index is always stable.  at returns
 * a pointer into the backing array: with the intended one-growable-per-arena
 * usage the backing is the arena's latest allocation, so it grows in place (the
 * VM-backed arena preserves the address) and the pointer survives growth; if the
 * pool shares an arena with other growables, a growth can relocate the backing
 * and invalidate the pointer, so prefer the index for long-lived references.
 *
 * Example:
 *   #define RC_POOL_TYPE thing
 *   #include "richc/template/pool.h"
 *
 *   rc_arena arena = rc_arena_make_default();
 *   rc_pool_thing pool = rc_pool_thing_make(0, &arena);
 *
 *   uint32_t i = rc_pool_thing_alloc(&pool, &arena);   // stable index, zeroed slot
 *   rc_pool_thing_set(&pool, i, make_thing());         // or: *rc_pool_thing_at(&pool, i) = ...
 *   thing t = rc_pool_thing_get(&pool, i);
 *   rc_pool_thing_free(&pool, i);                      // i may be handed out again later
 */

#ifndef RC_TEMPLATE_POOL_H_
#define RC_TEMPLATE_POOL_H_

#include "richc/arena.h"   // also provides RC_CONCAT, RC_ASSERT, RC_INDEX_NONE, <stdint.h>

#endif /* RC_TEMPLATE_POOL_H_ */

/* ===== per-type section ===== */

#ifndef RC_POOL_TYPE
#  define RC_POOL_TYPE int   // to keep intellisense happy
#  error "RC_POOL_TYPE must be defined before including richc/template/pool.h"
#endif

#ifndef RC_POOL_NAME
#  define RC_POOL_NAME RC_CONCAT(rc_pool_, RC_POOL_TYPE)
#endif

/* ---- derived type-name macros ---- */

#define RC_POOL_SLOT_  RC_CONCAT(RC_POOL_NAME, _slot)
#define RC_POOL_ARRAY_ RC_CONCAT(rc_array_, RC_POOL_SLOT_)

/* ---- internal array-call macros ---- */

#define RC_POOL_ARRAY_MAKE_        RC_CONCAT(RC_POOL_ARRAY_, _make)
#define RC_POOL_ARRAY_RESERVE_     RC_CONCAT(RC_POOL_ARRAY_, _reserve)
#define RC_POOL_ARRAY_RESET_       RC_CONCAT(RC_POOL_ARRAY_, _reset)
#define RC_POOL_ARRAY_PUSH_N_ZERO_ RC_CONCAT(RC_POOL_ARRAY_, _push_n_zero)
#define RC_POOL_ARRAY_POP_N_       RC_CONCAT(RC_POOL_ARRAY_, _pop_n)

/* ---- public function-name macros ---- */

#define RC_POOL_MAKE_    RC_CONCAT(RC_POOL_NAME, _make)
#define RC_POOL_RESERVE_ RC_CONCAT(RC_POOL_NAME, _reserve)
#define RC_POOL_RESET_   RC_CONCAT(RC_POOL_NAME, _reset)
#define RC_POOL_GET_     RC_CONCAT(RC_POOL_NAME, _get)
#define RC_POOL_SET_     RC_CONCAT(RC_POOL_NAME, _set)
#define RC_POOL_AT_      RC_CONCAT(RC_POOL_NAME, _at)
#define RC_POOL_ALLOC_   RC_CONCAT(RC_POOL_NAME, _alloc)
#define RC_POOL_FREE_    RC_CONCAT(RC_POOL_NAME, _free)

/* ---- generated types ---- */

/* Slot: the element value, or a free-list link when the slot is free. */
typedef union RC_POOL_SLOT_ {
    uint32_t     next_free_;
    RC_POOL_TYPE value;
} RC_POOL_SLOT_;

/* Backing store: a dense array of slots. */
#define RC_ARRAY_TYPE RC_POOL_SLOT_
#define RC_ARRAY_NAME RC_POOL_SLOT_
#include "richc/template/array.h"

/* Pool: the slot array plus the free-list head (RC_INDEX_NONE when empty). */
typedef struct RC_POOL_NAME {
    RC_POOL_ARRAY_ items;
    uint32_t       first_free;
} RC_POOL_NAME;

/* ---- lifecycle ---- */

/* Create a pool, optionally pre-reserving room for `capacity` slots (0 for none). */
static inline RC_POOL_NAME RC_POOL_MAKE_(uint32_t capacity, rc_arena *arena)
{
    return (RC_POOL_NAME){
        .items      = RC_POOL_ARRAY_MAKE_(capacity, arena),
        .first_free = RC_INDEX_NONE
    };
}

/* Ensure the backing array has room for at least min_capacity slots (exact). */
static inline void RC_POOL_RESERVE_(RC_POOL_NAME *pool, uint32_t min_capacity, rc_arena *arena)
{
    RC_POOL_ARRAY_RESERVE_(&pool->items, min_capacity, arena);
}

/* Drop all elements (indices become invalid); keeps the backing allocation. */
static inline void RC_POOL_RESET_(RC_POOL_NAME *pool)
{
    RC_POOL_ARRAY_RESET_(&pool->items);
    pool->first_free = RC_INDEX_NONE;
}

/* ---- access ---- */

/* The element at index, by value. */
static inline RC_POOL_TYPE RC_POOL_GET_(const RC_POOL_NAME *pool, uint32_t index)
{
    RC_ASSERT(index < pool->items.num);
    return pool->items.data[index].value;
}

/* Store value at index. */
static inline void RC_POOL_SET_(RC_POOL_NAME *pool, uint32_t index, RC_POOL_TYPE value)
{
    RC_ASSERT(index < pool->items.num);
    pool->items.data[index].value = value;
}

/* Pointer to the element at index.  Survives in-place growth (the intended
 * one-growable-per-arena case); see the file header for the relocation caveat. */
static inline RC_POOL_TYPE *RC_POOL_AT_(RC_POOL_NAME *pool, uint32_t index)
{
    RC_ASSERT(index < pool->items.num);
    return &pool->items.data[index].value;
}

/* ---- allocation ---- */

/*
 * Acquire a slot and return its index; the slot's value is zeroed.  Reuses a
 * freed slot when one is available, otherwise appends a fresh slot, growing the
 * backing array (in place when it is the arena's latest allocation, otherwise
 * relocating it - see the file header on `at` pointer stability).
 */
static inline uint32_t RC_POOL_ALLOC_(RC_POOL_NAME *pool, rc_arena *arena)
{
    if (pool->first_free == RC_INDEX_NONE) {
        return RC_POOL_ARRAY_PUSH_N_ZERO_(&pool->items, 1, arena);
    }
    uint32_t index = pool->first_free;
    pool->first_free = pool->items.data[index].next_free_;
    RC_POOL_SET_(pool, index, (RC_POOL_TYPE){0});   // zero the reused slot
    return index;
}

/*
 * Return a slot to the pool.  Freeing the final slot pops it (so the backing
 * does not keep a dead tail); any other slot is pushed onto the free list.
 * Freeing an already-free or out-of-range index is a caller error.
 */
static inline void RC_POOL_FREE_(RC_POOL_NAME *pool, uint32_t index)
{
    RC_ASSERT(index < pool->items.num);
    if (index == pool->items.num - 1) {
        RC_POOL_ARRAY_POP_N_(&pool->items, 1);
    }
    else {
        pool->items.data[index].next_free_ = pool->first_free;
        pool->first_free = index;
    }
}

/* ---- cleanup ---- */

#undef RC_POOL_SLOT_
#undef RC_POOL_ARRAY_
#undef RC_POOL_ARRAY_MAKE_
#undef RC_POOL_ARRAY_RESERVE_
#undef RC_POOL_ARRAY_RESET_
#undef RC_POOL_ARRAY_PUSH_N_ZERO_
#undef RC_POOL_ARRAY_POP_N_
#undef RC_POOL_MAKE_
#undef RC_POOL_RESERVE_
#undef RC_POOL_RESET_
#undef RC_POOL_GET_
#undef RC_POOL_SET_
#undef RC_POOL_AT_
#undef RC_POOL_ALLOC_
#undef RC_POOL_FREE_

#undef RC_POOL_NAME
#undef RC_POOL_TYPE
