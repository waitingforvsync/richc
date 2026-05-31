/*
 * math/box2f.h - 2D float axis-aligned bounding box (rc_box2f).
 *
 * Half-open interval: the box is [min, max) - min is inclusive on both axes,
 * max is exclusive.  This matches pixel / tile grid conventions: a 10x10 box
 * from (0, 0) has max = (10, 10) and the right and bottom edges are not
 * "inside" the box.
 *
 * Invariant: min is component-wise <= max.  Every constructor establishes it,
 * and all the queries assume it.  C cannot enforce it on a directly-initialised
 * rc_box2f, so a box built by hand must uphold it - we trust the caller.  The
 * corners are stored as internal members (trailing underscore); read them
 * through the accessors.
 *
 * Type
 * ----
 *   rc_box2f  { rc_vec2f min_; rc_vec2f max_; }
 *
 * Construction
 * ------------
 *   rc_box2f_make(a, b)                 - from two corner points; sorts components
 *   rc_box2f_make_pos_size(pos, size)   - from top-left position and extent;
 *                                         equivalent to make(pos, pos + size)
 *   rc_box2f_make_with_margin(a, b, m)  - sorted corners expanded by m on each side
 *
 * Accessors
 * ---------
 *   rc_box2f_min(a)     - minimum (inclusive) corner
 *   rc_box2f_max(a)     - maximum (exclusive) corner
 *   rc_box2f_size(a)    - extent as rc_vec2f: (max.x-min.x, max.y-min.y)
 *
 * Queries
 * -------
 *   rc_box2f_contains(a, b)       - true if a fully contains b
 *   rc_box2f_intersects(a, b)     - true if the two boxes overlap (touching edges do NOT count)
 *   rc_box2f_contains_point(a, p) - true if point p is inside a (min inclusive, max exclusive)
 *
 * Combination
 * -----------
 *   rc_box2f_union(a, b)          - smallest box containing both a and b
 *   rc_box2f_expand(a, p)         - smallest box containing a and point p
 */

#ifndef RC_MATH_BOX2F_H_
#define RC_MATH_BOX2F_H_

#include "richc/math/vec2f.h"

/* ---- type ---- */

typedef struct rc_box2f {
    rc_vec2f min_;
    rc_vec2f max_;
} rc_box2f;

/* ---- construction ---- */

/* Construct from two corner points; component-wise min/max are computed. */
static inline rc_box2f rc_box2f_make(rc_vec2f a, rc_vec2f b)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_component_min(a, b),
        .max_ = rc_vec2f_component_max(a, b)
    };
}

/*
 * Construct from a top-left position and a size (width, height).
 * Equivalent to rc_box2f_make(pos, rc_vec2f_add(pos, size)).
 * size components should be non-negative for a well-formed box.
 */
static inline rc_box2f rc_box2f_make_pos_size(rc_vec2f pos, rc_vec2f size)
{
    return (rc_box2f) {.min_ = pos, .max_ = rc_vec2f_add(pos, size)};
}

/* Construct from two corner points and expand each side by margin. */
static inline rc_box2f rc_box2f_make_with_margin(rc_vec2f a, rc_vec2f b, float margin)
{
    rc_vec2f m = rc_vec2f_make(margin, margin);
    return (rc_box2f) {
        .min_ = rc_vec2f_sub(rc_vec2f_component_min(a, b), m),
        .max_ = rc_vec2f_add(rc_vec2f_component_max(a, b), m)
    };
}

/* ---- accessors ---- */

/* Minimum (inclusive) corner. */
static inline rc_vec2f rc_box2f_min(rc_box2f a) { return a.min_; }

/* Maximum (exclusive) corner. */
static inline rc_vec2f rc_box2f_max(rc_box2f a) { return a.max_; }

/*
 * Extent of the box as a vector: (max.x - min.x, max.y - min.y).
 * Returns {0, 0} for a zero-area box where min == max.
 */
static inline rc_vec2f rc_box2f_size(rc_box2f a)
{
    return rc_vec2f_sub(a.max_, a.min_);
}

/* ---- queries ---- */

/*
 * True if a fully contains b: a.min <= b.min and a.max >= b.max on both axes.
 * Comparing the half-open bounds directly is correct - if both boxes use the
 * same [min, max) convention, b fits inside a whenever a's span is at least as
 * wide.
 */
static inline bool rc_box2f_contains(rc_box2f a, rc_box2f b)
{
    return a.min_.x <= b.min_.x && a.min_.y <= b.min_.y
        && a.max_.x >= b.max_.x && a.max_.y >= b.max_.y;
}

/*
 * True if a and b have a non-empty intersection.
 * Two half-open intervals [a0,a1) and [b0,b1) overlap iff a0 < b1 && b0 < a1.
 * Touching edges (e.g. a.max.x == b.min.x) do NOT count as intersection.
 */
static inline bool rc_box2f_intersects(rc_box2f a, rc_box2f b)
{
    return a.min_.x < b.max_.x && a.min_.y < b.max_.y
        && a.max_.x > b.min_.x && a.max_.y > b.min_.y;
}

/* True if point p lies inside [min, max): min is inclusive, max is exclusive. */
static inline bool rc_box2f_contains_point(rc_box2f a, rc_vec2f p)
{
    return a.min_.x <= p.x && a.min_.y <= p.y
        && a.max_.x > p.x  && a.max_.y > p.y;
}

/* ---- combination ---- */

/* Smallest box that contains both a and b. */
static inline rc_box2f rc_box2f_union(rc_box2f a, rc_box2f b)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_component_min(a.min_, b.min_),
        .max_ = rc_vec2f_component_max(a.max_, b.max_)
    };
}

/* Smallest box that contains a and the additional point p. */
static inline rc_box2f rc_box2f_expand(rc_box2f a, rc_vec2f p)
{
    return (rc_box2f) {
        .min_ = rc_vec2f_component_min(a.min_, p),
        .max_ = rc_vec2f_component_max(a.max_, p)
    };
}

#endif /* RC_MATH_BOX2F_H_ */
