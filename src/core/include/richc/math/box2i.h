/*
 * math/box2i.h - 2D integer axis-aligned bounding box (rc_box2i).
 *
 * Half-open interval: the box is [min, max) - min is inclusive on both axes,
 * max is exclusive.  This matches pixel / tile grid conventions: a 10x10 box
 * from (0, 0) has max = (10, 10) and contains exactly 100 unit cells; the right
 * and bottom edges are not "inside" the box.
 *
 * Invariant: min is component-wise <= max.  Every constructor establishes it,
 * and all the queries assume it.  C cannot enforce it on a directly-initialised
 * rc_box2i, so a box built by hand must uphold it - we trust the caller.  The
 * corners are stored as internal members (trailing underscore); read them
 * through the accessors.
 *
 * Type
 * ----
 *   rc_box2i  { rc_vec2i min_; rc_vec2i max_; }
 *
 * Construction
 * ------------
 *   rc_box2i_make(a, b)                 - from two corner points; sorts components
 *   rc_box2i_make_with_margin(a, b, m)  - same, then expands each side by m;
 *                                         asserts no int32_t overflow
 *   rc_box2i_make_pos_size(pos, size)   - from top-left position and extent;
 *                                         equivalent to make(pos, pos + size)
 *
 * Accessors
 * ---------
 *   rc_box2i_min(a)     - minimum (inclusive) corner
 *   rc_box2i_max(a)     - maximum (exclusive) corner
 *   rc_box2i_size(a)    - extent as rc_vec2i: (max.x-min.x, max.y-min.y)
 *
 * Queries
 * -------
 *   rc_box2i_contains(a, b)       - true if a fully contains b
 *   rc_box2i_intersects(a, b)     - true if the two boxes overlap (touching edges do NOT count)
 *   rc_box2i_contains_point(a, p) - true if point p is inside a (min inclusive, max exclusive)
 *
 * Combination
 * -----------
 *   rc_box2i_union(a, b)          - smallest box containing both a and b
 *   rc_box2i_expand(a, p)         - smallest box containing a and point p
 */

#ifndef RC_MATH_BOX2I_H_
#define RC_MATH_BOX2I_H_

#include "richc/math/vec2i.h"

/* ---- type ---- */

typedef struct rc_box2i {
    rc_vec2i min_;
    rc_vec2i max_;
} rc_box2i;

/* ---- construction ---- */

/* Construct from two corner points; component-wise min/max are computed. */
static inline rc_box2i rc_box2i_make(rc_vec2i a, rc_vec2i b)
{
    return (rc_box2i) {
        .min_ = rc_vec2i_component_min(a, b),
        .max_ = rc_vec2i_component_max(a, b)
    };
}

/*
 * Construct from a top-left position and a size (width, height).
 * Equivalent to rc_box2i_make(pos, rc_vec2i_add(pos, size)).
 * size components should be non-negative for a well-formed box.
 */
static inline rc_box2i rc_box2i_make_pos_size(rc_vec2i pos, rc_vec2i size)
{
    return (rc_box2i) {.min_ = pos, .max_ = rc_vec2i_add(pos, size)};
}

/*
 * Construct from two corner points and expand each side by margin.
 * Asserts that the expansion does not overflow int32_t.
 */
static inline rc_box2i rc_box2i_make_with_margin(rc_vec2i a, rc_vec2i b, int32_t margin)
{
    rc_vec2i lo = rc_vec2i_component_min(a, b);
    rc_vec2i hi = rc_vec2i_component_max(a, b);
    RC_ASSERT((int64_t)lo.x - margin >= INT32_MIN && (int64_t)lo.y - margin >= INT32_MIN);
    RC_ASSERT((int64_t)hi.x + margin <= INT32_MAX && (int64_t)hi.y + margin <= INT32_MAX);
    return (rc_box2i) {
        .min_ = rc_vec2i_make(lo.x - margin, lo.y - margin),
        .max_ = rc_vec2i_make(hi.x + margin, hi.y + margin)
    };
}

/* ---- accessors ---- */

/* Minimum (inclusive) corner. */
static inline rc_vec2i rc_box2i_min(rc_box2i a) { return a.min_; }

/* Maximum (exclusive) corner. */
static inline rc_vec2i rc_box2i_max(rc_box2i a) { return a.max_; }

/*
 * Extent of the box as a vector: (max.x - min.x, max.y - min.y).
 * Returns {0, 0} for a zero-area box where min == max.
 */
static inline rc_vec2i rc_box2i_size(rc_box2i a)
{
    return rc_vec2i_sub(a.max_, a.min_);
}

/* ---- queries ---- */

/*
 * True if a fully contains b: a.min <= b.min and a.max >= b.max on both axes.
 * Comparing the half-open bounds directly is correct - if both boxes use the
 * same [min, max) convention, b fits inside a whenever a's span is at least as
 * wide.
 */
static inline bool rc_box2i_contains(rc_box2i a, rc_box2i b)
{
    return a.min_.x <= b.min_.x && a.min_.y <= b.min_.y
        && a.max_.x >= b.max_.x && a.max_.y >= b.max_.y;
}

/*
 * True if a and b have a non-empty intersection.
 * Two half-open intervals [a0,a1) and [b0,b1) overlap iff a0 < b1 && b0 < a1.
 * Touching edges (e.g. a.max.x == b.min.x) do NOT count as intersection.
 */
static inline bool rc_box2i_intersects(rc_box2i a, rc_box2i b)
{
    return a.min_.x < b.max_.x && a.min_.y < b.max_.y
        && a.max_.x > b.min_.x && a.max_.y > b.min_.y;
}

/* True if point p lies inside [min, max): min is inclusive, max is exclusive. */
static inline bool rc_box2i_contains_point(rc_box2i a, rc_vec2i p)
{
    return a.min_.x <= p.x && a.min_.y <= p.y
        && a.max_.x > p.x  && a.max_.y > p.y;
}

/* ---- combination ---- */

/* Smallest box that contains both a and b. */
static inline rc_box2i rc_box2i_union(rc_box2i a, rc_box2i b)
{
    return (rc_box2i) {
        .min_ = rc_vec2i_component_min(a.min_, b.min_),
        .max_ = rc_vec2i_component_max(a.max_, b.max_)
    };
}

/* Smallest box that contains a and the additional point p. */
static inline rc_box2i rc_box2i_expand(rc_box2i a, rc_vec2i p)
{
    return (rc_box2i) {
        .min_ = rc_vec2i_component_min(a.min_, p),
        .max_ = rc_vec2i_component_max(a.max_, p)
    };
}

#endif /* RC_MATH_BOX2I_H_ */
