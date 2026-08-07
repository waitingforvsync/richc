/*
 * genpool.h - template header: generational free-list object pool.
 *
 * A genpool is a pool (see pool.h) whose slots carry a generation counter, so a
 * caller holds an rc_genpool_handle {index, generation} rather than a bare
 * index.  Freeing a slot increments its generation, invalidating every handle
 * issued for the slot's previous lifetime: a stale handle is *detected* (is_valid
 * false, at NULL, get/set/free assert) instead of silently aliasing the slot's
 * next occupant.  Use a genpool where handles outlive their elements (resource
 * tables, scene objects); use a plain pool where indices are managed strictly.
 *
 * Slot encoding
 * -------------
 * Each slot is the element value or a free-list link, plus the generation, which
 * lives OUTSIDE the union so it survives free:
 *
 *   struct { union { uint32_t next_free_; RC_GENPOOL_TYPE value; }; uint32_t gen; }
 *
 * A live slot is only ever read through `value`; a free slot only through
 * `next_free_`, which stores the next free slot's index + 1 (0 ends the list).
 * That off-by-one lets index 0 be an ordinary free-list member and makes a
 * zero-initialised pool a valid empty pool (first_free == 0 means "empty"), so
 * no make() call is needed for an empty pool.  RC_GENPOOL_TYPE smaller than
 * uint32_t rounds the union up to 4 bytes, and the generation adds 4 more (plus
 * any alignment padding the element type demands).
 *
 * alloc / free / generations
 * --------------------------
 * alloc pops the free-list head if there is one (zeroing the reused value,
 * preserving the generation), otherwise appends a fresh zeroed slot (generation
 * 0), and returns a handle carrying the slot's current generation.  free asserts
 * the handle is valid - trapping null, stale, and double-free - then increments
 * the slot's generation and pushes the slot onto the free list.  Because free
 * advances the generation, alloc(); free(h); is NOT byte-identical (unlike
 * pool.h) - that is the feature.  Trailing free slots are not trimmed - the
 * backing array holds its high-water mark until reset/deinit reclaim it
 * wholesale.
 *
 * Handles are the shared rc_genpool_handle type (richc/genpool_handle.h), not
 * typesafe per pool: a handle presented to the wrong genpool goes undetected
 * when index and generation happen to match there, just as pool indices are
 * plain uint32_t.  Wrap the handle in a one-member struct per pool where mixing
 * must be a compile error (the gfx layer's public handles do this).
 *
 * reset/deinit drop the slot array, so slots recreated afterwards restart at
 * generation 0: a handle issued before the reset can alias one issued after.
 * Same caveat class as pool.h index reuse; there is no epoch mechanism.  A
 * single slot's generation wraps after 2^32 frees, momentarily revalidating an
 * ancient handle; noted, not defended.
 *
 * Iteration
 * ---------
 * The generation does not encode liveness (it only advances on free), so as with
 * pool.h the only liveness information is the free list.  NAME_free_bitset
 * materialises it as a bitset of the dead slots (set bit == free);
 * NAME_handle_at reconstructs the current handle of a slot known to be live.
 * The genpool_foreach.h template combines the two to visit the live elements.
 *
 * Define before including:
 *   RC_GENPOOL_TYPE   element type (required)
 *   RC_GENPOOL_NAME   base name for the generated type and functions
 *                     (optional; default rc_genpool_<RC_GENPOOL_TYPE>).  The
 *                     default requires a single-identifier element type; pass an
 *                     explicit name for multi-token types.
 *
 * All macros defined before inclusion are undefined by this header.
 *
 * Generated type and functions (all static inline; H = rc_genpool_handle)
 * -----------------------------------------------------------------------
 *   RC_GENPOOL_NAME                                                the pool
 *
 *   RC_GENPOOL_NAME  NAME_make(uint32_t capacity, rc_arena *arena) // optional; { 0 } is a valid pool
 *   void             NAME_reserve(NAME *pool, uint32_t min_capacity, rc_arena *arena)
 *   void             NAME_reset(NAME *pool)                        // drop all elements, keep backing
 *   void             NAME_deinit(NAME *pool, rc_arena *arena)      // free backing + zero
 *   H                NAME_alloc(NAME *pool, rc_arena *arena)       // handle to a zeroed slot
 *   void             NAME_free(NAME *pool, H h)                    // asserts is_valid; bumps generation
 *   bool             NAME_is_valid(const NAME *pool, H h)          // non-null, in range, generation match
 *   RC_GENPOOL_TYPE  NAME_get(const NAME *pool, H h)               // asserts is_valid
 *   void             NAME_set(NAME *pool, H h, RC_GENPOOL_TYPE value)  // asserts is_valid
 *   RC_GENPOOL_TYPE *NAME_at(NAME *pool, H h)                      // NULL if not is_valid; see below
 *   const RC_GENPOOL_TYPE *NAME_at_const(const NAME *pool, H h)    // NULL likewise
 *   H                NAME_handle_at(const NAME *pool, uint32_t index)  // live slots only; see Iteration
 *   rc_bitset        NAME_free_bitset(const NAME *pool, rc_arena *arena)  // dead slots; see Iteration
 *
 * at returns a pointer into the backing array: with the intended
 * one-growable-per-arena usage the backing is the arena's latest allocation, so
 * it grows in place (the VM-backed arena preserves the address) and the pointer
 * survives growth; if the pool shares an arena with other growables, a growth
 * can relocate the backing and invalidate the pointer, so prefer the handle for
 * long-lived references.
 *
 * handle_at manufactures the handle a slot would currently validate against; on
 * a *free* slot that is the handle the next alloc will issue, which is_valid
 * would accept, so call it only for slots known to be live (asserts only that
 * the index is in range).
 *
 * Example:
 *   #define RC_GENPOOL_TYPE thing
 *   #include "richc/template/genpool.h"
 *
 *   rc_arena arena = rc_arena_make_default();
 *   rc_genpool_thing pool = {0};                              // a valid empty pool
 *
 *   rc_genpool_handle h = rc_genpool_thing_alloc(&pool, &arena);   // zeroed slot
 *   rc_genpool_thing_set(&pool, h, make_thing());             // or: *rc_genpool_thing_at(&pool, h) = ...
 *   thing t = rc_genpool_thing_get(&pool, h);
 *   rc_genpool_thing_free(&pool, h);                          // h is now stale...
 *   RC_ASSERT(!rc_genpool_thing_is_valid(&pool, h));          // ...and detectably so
 *   rc_genpool_thing_deinit(&pool, &arena);                   // free the backing, back to { 0 }
 */

#ifndef RC_TEMPLATE_GENPOOL_H_
#define RC_TEMPLATE_GENPOOL_H_

#include "richc/arena.h"   // also provides RC_CONCAT, RC_ASSERT, <stdint.h>
#include "richc/bitset.h"
#include "richc/genpool_handle.h"

#endif /* RC_TEMPLATE_GENPOOL_H_ */

/* ===== per-type section ===== */

#ifndef RC_GENPOOL_TYPE
#  define RC_GENPOOL_TYPE int   // to keep intellisense happy
#  error "RC_GENPOOL_TYPE must be defined before including richc/template/genpool.h"
#endif

#ifndef RC_GENPOOL_NAME
#  define RC_GENPOOL_NAME RC_CONCAT(rc_genpool_, RC_GENPOOL_TYPE)
#endif

/* ---- derived type-name macros ---- */

#define RC_GENPOOL_SLOT_  RC_CONCAT(RC_GENPOOL_NAME, _slot)
#define RC_GENPOOL_ARRAY_ RC_CONCAT(rc_array_, RC_GENPOOL_SLOT_)

/* ---- internal array-call macros ---- */

#define RC_GENPOOL_ARRAY_MAKE_        RC_CONCAT(RC_GENPOOL_ARRAY_, _make)
#define RC_GENPOOL_ARRAY_RESERVE_     RC_CONCAT(RC_GENPOOL_ARRAY_, _reserve)
#define RC_GENPOOL_ARRAY_RESET_       RC_CONCAT(RC_GENPOOL_ARRAY_, _reset)
#define RC_GENPOOL_ARRAY_DEINIT_      RC_CONCAT(RC_GENPOOL_ARRAY_, _deinit)
#define RC_GENPOOL_ARRAY_PUSH_N_ZERO_ RC_CONCAT(RC_GENPOOL_ARRAY_, _push_n_zero)

/* ---- public function-name macros ---- */

#define RC_GENPOOL_MAKE_      RC_CONCAT(RC_GENPOOL_NAME, _make)
#define RC_GENPOOL_RESERVE_   RC_CONCAT(RC_GENPOOL_NAME, _reserve)
#define RC_GENPOOL_RESET_     RC_CONCAT(RC_GENPOOL_NAME, _reset)
#define RC_GENPOOL_DEINIT_    RC_CONCAT(RC_GENPOOL_NAME, _deinit)
#define RC_GENPOOL_IS_VALID_  RC_CONCAT(RC_GENPOOL_NAME, _is_valid)
#define RC_GENPOOL_GET_       RC_CONCAT(RC_GENPOOL_NAME, _get)
#define RC_GENPOOL_SET_       RC_CONCAT(RC_GENPOOL_NAME, _set)
#define RC_GENPOOL_AT_        RC_CONCAT(RC_GENPOOL_NAME, _at)
#define RC_GENPOOL_AT_CONST_  RC_CONCAT(RC_GENPOOL_NAME, _at_const)
#define RC_GENPOOL_ALLOC_     RC_CONCAT(RC_GENPOOL_NAME, _alloc)
#define RC_GENPOOL_FREE_      RC_CONCAT(RC_GENPOOL_NAME, _free)
#define RC_GENPOOL_HANDLE_AT_ RC_CONCAT(RC_GENPOOL_NAME, _handle_at)
#define RC_GENPOOL_FREE_BITSET_ RC_CONCAT(RC_GENPOOL_NAME, _free_bitset)

/* ---- generated types ---- */

/*
 * Slot: the element value or a free-list link, plus the generation.  The link
 * (and the pool's first_free head) stores the next slot's index PLUS ONE, with 0
 * meaning "none", exactly as in pool.h - so { 0 } is a valid empty pool.  The
 * generation lives outside the union: free bumps it and the free list never
 * touches it, so it survives the slot's dead period.
 */
typedef struct RC_GENPOOL_SLOT_ {
    union {
        uint32_t        next_free_;   // next free slot's index + 1, or 0 for end of list
        RC_GENPOOL_TYPE value;
    };
    uint32_t gen;   // incremented on free; a handle is valid while its gen matches
} RC_GENPOOL_SLOT_;

/* Backing store: a dense array of slots. */
#define RC_ARRAY_TYPE RC_GENPOOL_SLOT_
#define RC_ARRAY_NAME RC_GENPOOL_SLOT_
#include "richc/template/array.h"

/* Pool: the slot array plus the free-list head.  Zero-initialisable. */
typedef struct RC_GENPOOL_NAME {
    RC_GENPOOL_ARRAY_ items;
    uint32_t          first_free;   // head slot's index + 1, or 0 if the list is empty
} RC_GENPOOL_NAME;

/* ---- lifecycle ---- */

/*
 * Create a pool, optionally pre-reserving room for `capacity` slots (0 for none).
 * A zero-initialised pool is already a valid empty pool, so make() is only needed
 * to pre-reserve.
 */
static inline RC_GENPOOL_NAME RC_GENPOOL_MAKE_(uint32_t capacity, rc_arena *arena)
{
    return (RC_GENPOOL_NAME) {.items = RC_GENPOOL_ARRAY_MAKE_(capacity, arena)};
}

/* Ensure the backing array has room for at least min_capacity slots (exact). */
static inline void RC_GENPOOL_RESERVE_(RC_GENPOOL_NAME *pool, uint32_t min_capacity, rc_arena *arena)
{
    RC_GENPOOL_ARRAY_RESERVE_(&pool->items, min_capacity, arena);
}

/* Drop all elements; keeps the backing allocation.  Recreated slots restart at
 * generation 0, so handles issued before the reset can alias handles issued
 * after it (see the file header). */
static inline void RC_GENPOOL_RESET_(RC_GENPOOL_NAME *pool)
{
    RC_GENPOOL_ARRAY_RESET_(&pool->items);
    pool->first_free = 0;   // empty free list
}

/* Free the backing allocation (best-effort) and zero the pool to a valid empty
 * state.  The same generation-restart caveat as reset applies. */
static inline void RC_GENPOOL_DEINIT_(RC_GENPOOL_NAME *pool, rc_arena *arena)
{
    RC_GENPOOL_ARRAY_DEINIT_(&pool->items, arena);
    pool->first_free = 0;
}

/* ---- access ---- */

/*
 * Does the handle refer to a live element of this pool?  False for the null
 * handle, an out-of-range index, and a stale generation.  A true result only
 * means the slot currently validates against this handle - see the file header
 * for the cross-pool and post-reset caveats.
 */
static inline bool RC_GENPOOL_IS_VALID_(const RC_GENPOOL_NAME *pool, rc_genpool_handle handle)
{
    if (rc_genpool_handle_is_null(handle)) {
        return false;
    }
    uint32_t index = rc_genpool_handle_index(handle);
    return index < pool->items.num
        && pool->items.data[index].gen == rc_genpool_handle_gen(handle);
}

/* The element the handle refers to, by value.  Asserts the handle is valid. */
static inline RC_GENPOOL_TYPE RC_GENPOOL_GET_(const RC_GENPOOL_NAME *pool, rc_genpool_handle handle)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    return pool->items.data[rc_genpool_handle_index(handle)].value;
}

/* Store value in the element the handle refers to.  Asserts the handle is valid. */
static inline void RC_GENPOOL_SET_(RC_GENPOOL_NAME *pool, rc_genpool_handle handle, RC_GENPOOL_TYPE value)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    pool->items.data[rc_genpool_handle_index(handle)].value = value;
}

/* Pointer to the element the handle refers to, or NULL when the handle is not
 * valid (null, out of range, or stale) - the non-trapping accessor.  The pointer
 * survives in-place growth (the intended one-growable-per-arena case); see the
 * file header for the relocation caveat. */
static inline RC_GENPOOL_TYPE *RC_GENPOOL_AT_(RC_GENPOOL_NAME *pool, rc_genpool_handle handle)
{
    if (!RC_GENPOOL_IS_VALID_(pool, handle)) {
        return NULL;
    }
    return &pool->items.data[rc_genpool_handle_index(handle)].value;
}

/* Read-only pointer to the element the handle refers to, or NULL when the handle
 * is not valid, for access through a const pool.  Same growth/relocation caveat
 * as at(); use it when the pool is only being read. */
static inline const RC_GENPOOL_TYPE *RC_GENPOOL_AT_CONST_(const RC_GENPOOL_NAME *pool, rc_genpool_handle handle)
{
    if (!RC_GENPOOL_IS_VALID_(pool, handle)) {
        return NULL;
    }
    return &pool->items.data[rc_genpool_handle_index(handle)].value;
}

/* ---- allocation ---- */

/*
 * Acquire a slot and return its handle; the slot's value is zeroed.  Reuses a
 * freed slot when one is available (the handle carries the slot's advanced
 * generation, so handles from the previous lifetime stay stale), otherwise
 * appends a fresh slot at generation 0, growing the backing array (in place when
 * it is the arena's latest allocation, otherwise relocating it - see the file
 * header on `at` pointer stability).
 */
static inline rc_genpool_handle RC_GENPOOL_ALLOC_(RC_GENPOOL_NAME *pool, rc_arena *arena)
{
    if (pool->first_free == 0) {   // free list empty: append a fresh slot (value and gen zero)
        uint32_t index = RC_GENPOOL_ARRAY_PUSH_N_ZERO_(&pool->items, 1, arena);
        return rc_genpool_handle_make(index, 0);
    }
    uint32_t index = pool->first_free - 1;               // decode the + 1
    RC_GENPOOL_SLOT_ *slot = &pool->items.data[index];
    pool->first_free = slot->next_free_;                 // pop: head <- its link
    slot->value = (RC_GENPOOL_TYPE) {0};                 // zero the reused value; gen is preserved
    return rc_genpool_handle_make(index, slot->gen);
}

/*
 * Return the element's slot to the pool: bump the slot's generation - staling
 * every handle issued for this lifetime, including `handle` itself - and push
 * the slot onto the free list.  Asserts the handle is valid, so a null, stale,
 * or double-freed handle traps here rather than corrupting the free list.  The
 * backing array is not shrunk, even when the slot is the final one (trailing
 * free slots are reclaimed by reset/deinit).
 */
static inline void RC_GENPOOL_FREE_(RC_GENPOOL_NAME *pool, rc_genpool_handle handle)
{
    RC_ASSERT(RC_GENPOOL_IS_VALID_(pool, handle));
    uint32_t index = rc_genpool_handle_index(handle);
    RC_GENPOOL_SLOT_ *slot = &pool->items.data[index];
    slot->gen += 1;   // invalidate every outstanding handle to this slot
    // push onto the free list: link to the old head, become the new head.
    // both are stored as index + 1, so 0 stays the empty/end marker.
    slot->next_free_ = pool->first_free;
    pool->first_free = index + 1;
}

/* ---- iteration support ---- */

/*
 * The handle slot `index` currently validates against.  For a live slot that is
 * the handle alloc issued; for a *free* slot it is the handle the next alloc
 * will issue (the generation is already bumped), which is_valid would wrongly
 * accept - so call this only for slots known to be live, e.g. the clear bits of
 * free_bitset (genpool_foreach.h does exactly that).  Asserts only that the
 * index is in range.
 */
static inline rc_genpool_handle RC_GENPOOL_HANDLE_AT_(const RC_GENPOOL_NAME *pool, uint32_t index)
{
    RC_ASSERT(index < pool->items.num);
    return rc_genpool_handle_make(index, pool->items.data[index].gen);
}

/*
 * Build a bitset whose set bits are exactly the free-list (dead) slots, sized to
 * items.num and allocated from `arena`; a clear bit is therefore a live slot.
 * Intended for a scratch arena - see genpool_foreach.h, which consumes it to
 * visit the live elements.  An empty pool yields an empty bitset.
 */
static inline rc_bitset RC_GENPOOL_FREE_BITSET_(const RC_GENPOOL_NAME *pool, rc_arena *arena)
{
    rc_bitset dead = {0};
    rc_bitset_resize(&dead, pool->items.num, arena);   // fresh bitset -> all bits 0
    for (uint32_t link = pool->first_free; link != 0; ) {
        uint32_t index = link - 1;                      // decode the + 1
        rc_bitset_set(&dead, index);
        link = pool->items.data[index].next_free_;
    }
    return dead;
}

/* ---- cleanup ---- */

#undef RC_GENPOOL_SLOT_
#undef RC_GENPOOL_ARRAY_
#undef RC_GENPOOL_ARRAY_MAKE_
#undef RC_GENPOOL_ARRAY_RESERVE_
#undef RC_GENPOOL_ARRAY_RESET_
#undef RC_GENPOOL_ARRAY_DEINIT_
#undef RC_GENPOOL_ARRAY_PUSH_N_ZERO_
#undef RC_GENPOOL_MAKE_
#undef RC_GENPOOL_RESERVE_
#undef RC_GENPOOL_RESET_
#undef RC_GENPOOL_DEINIT_
#undef RC_GENPOOL_IS_VALID_
#undef RC_GENPOOL_GET_
#undef RC_GENPOOL_SET_
#undef RC_GENPOOL_AT_
#undef RC_GENPOOL_AT_CONST_
#undef RC_GENPOOL_ALLOC_
#undef RC_GENPOOL_FREE_
#undef RC_GENPOOL_HANDLE_AT_
#undef RC_GENPOOL_FREE_BITSET_

#undef RC_GENPOOL_NAME
#undef RC_GENPOOL_TYPE
