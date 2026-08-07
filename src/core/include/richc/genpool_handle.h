/*
 * genpool_handle.h - handle to a generational pool (genpool) slot.
 *
 * A genpool (template/genpool.h) hands out handles rather than bare indices: a
 * handle pairs the slot index with the slot's generation at issue time, so a
 * handle held past its element's free is detected (the pool bumps the slot's
 * generation on free) instead of silently aliasing the slot's next occupant.
 *
 * Encoding
 * --------
 *   rc_genpool_handle  { uint32_t index_; uint32_t gen_; }
 *
 * index_ stores the slot index PLUS ONE, with 0 meaning "no slot" - the same
 * off-by-one used by the pool free lists - so a zero-initialised handle is the
 * null handle:  rc_genpool_handle h = {0};
 *
 * The fields are internal (trailing underscore); inspect a handle only through
 * the accessors below.
 *
 * One handle type serves every genpool instantiation - handles are not typesafe
 * per pool, just as pool indices are plain uint32_t.  Where mixing handles from
 * different pools must be a compile error, wrap the handle in a one-member
 * struct per pool (the gfx layer's public handles do this).
 *
 * Operations
 * ----------
 *   rc_genpool_handle_make(index, gen)  - handle for slot index with generation gen
 *   rc_genpool_handle_is_null(h)        - is h the null handle?  (NOT a liveness
 *                                         check - a non-null handle may still be
 *                                         stale; ask the pool's is_valid)
 *   rc_genpool_handle_index(h)          - slot index (asserts h is not null)
 *   rc_genpool_handle_gen(h)            - generation at issue time
 *   rc_genpool_handle_equal(a, b)       - same slot and generation?
 */

#ifndef RC_GENPOOL_HANDLE_H_
#define RC_GENPOOL_HANDLE_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/macros.h"

/* Handle to a genpool slot; {0} is the null handle. */
typedef struct rc_genpool_handle {
    uint32_t index_;   // slot index + 1; 0 = null handle
    uint32_t gen_;     // slot generation at issue time
} rc_genpool_handle;

/* Construct the handle for slot `index` with generation `gen`. */
static inline rc_genpool_handle rc_genpool_handle_make(uint32_t index, uint32_t gen)
{
    return (rc_genpool_handle) {.index_ = index + 1, .gen_ = gen};
}

/* Is this the null handle?  Not a liveness check: a non-null handle may still be
 * stale - only the owning pool's is_valid can tell. */
static inline bool rc_genpool_handle_is_null(rc_genpool_handle handle)
{
    return handle.index_ == 0;
}

/* The slot index this handle refers to.  The null handle has no index. */
static inline uint32_t rc_genpool_handle_index(rc_genpool_handle handle)
{
    RC_ASSERT(!rc_genpool_handle_is_null(handle));
    return handle.index_ - 1;
}

/* The slot generation this handle was issued under. */
static inline uint32_t rc_genpool_handle_gen(rc_genpool_handle handle)
{
    return handle.gen_;
}

/* Do the handles refer to the same slot at the same generation? */
static inline bool rc_genpool_handle_equal(rc_genpool_handle a, rc_genpool_handle b)
{
    return a.index_ == b.index_ && a.gen_ == b.gen_;
}

#endif /* RC_GENPOOL_HANDLE_H_ */
