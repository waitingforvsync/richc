/*
 * math/aabb2f.h - 2D axis-aligned bounding box (rc_aabb2f).
 *
 * Half-open interval: the box represents [min, max) — min is inclusive on
 * both axes, max is exclusive on both axes.  This matches pixel / tile grid
 * conventions: a 10×10 box from (0,0) has max=(10,10) and contains exactly
 * 100 unit cells; the right and bottom edges are not "inside" the box.
 *
 * All constructors ensure min <= max component-wise.
 *
 * Type
 * ----
 *   rc_aabb2f  { rc_vec2f min; rc_vec2f max; }
 *
 * Construction
 * ------------
 *   rc_aabb2f_make(a, b)                 — from two corner points; sorts components
 *   rc_aabb2f_make_with_margin(a, b, m)  — same, then expands each side by m
 *
 * Queries
 * -------
 *   rc_aabb2f_contains(a, b)       — true if a fully contains b  [a.min,a.max) ⊇ [b.min,b.max)
 *   rc_aabb2f_intersects(a, b)     — true if the two boxes overlap (touching edges do NOT count)
 *   rc_aabb2f_contains_point(a, p) — true if point p is inside a (min inclusive, max exclusive)
 *
 * Combination
 * -----------
 *   rc_aabb2f_union(a, b)          — smallest box containing both a and b
 *   rc_aabb2f_expand(a, p)         — smallest box containing a and point p
 */

#ifndef RC_MATH_AABB2F_H_
#define RC_MATH_AABB2F_H_

#include "richc/math/vec2f.h"

/* ---- type ---- */

typedef struct {
    rc_vec2f min;
    rc_vec2f max;
} rc_aabb2f;

/* ---- construction ---- */

/* Construct from two corner points; component-wise min/max are computed. */
static inline rc_aabb2f rc_aabb2f_make(rc_vec2f a, rc_vec2f b)
{
    return (rc_aabb2f) {
        rc_vec2f_component_min(a, b),
        rc_vec2f_component_max(a, b)
    };
}

/* Construct from two corner points and expand each side by margin. */
static inline rc_aabb2f rc_aabb2f_make_with_margin(rc_vec2f a, rc_vec2f b, float margin)
{
    rc_vec2f mv = {margin, margin};
    return (rc_aabb2f) {
        rc_vec2f_sub(rc_vec2f_component_min(a, b), mv),
        rc_vec2f_add(rc_vec2f_component_max(a, b), mv)
    };
}

/* ---- queries ---- */

/*
 * True if a fully contains b: a.min <= b.min and a.max >= b.max on both axes.
 * Comparing the half-open bounds directly is correct — if both boxes use the
 * same [min, max) convention, b fits inside a whenever a's span is at least
 * as wide.
 */
static inline bool rc_aabb2f_contains(rc_aabb2f a, rc_aabb2f b)
{
    return a.min.x <= b.min.x && a.min.y <= b.min.y
        && a.max.x >= b.max.x && a.max.y >= b.max.y;
}

/*
 * True if a and b have a non-empty intersection.
 * Two half-open intervals [a0,a1) and [b0,b1) overlap iff a0 < b1 && b0 < a1.
 * Touching edges (e.g. a.max.x == b.min.x) do NOT count as intersection.
 */
static inline bool rc_aabb2f_intersects(rc_aabb2f a, rc_aabb2f b)
{
    return a.min.x < b.max.x && a.min.y < b.max.y
        && a.max.x > b.min.x && a.max.y > b.min.y;
}

/*
 * True if point p lies inside [min, max): min is inclusive, max is exclusive.
 */
static inline bool rc_aabb2f_contains_point(rc_aabb2f a, rc_vec2f p)
{
    return a.min.x <= p.x && a.min.y <= p.y
        && a.max.x > p.x  && a.max.y > p.y;
}

/* ---- combination ---- */

/* Smallest box that contains both a and b. */
static inline rc_aabb2f rc_aabb2f_union(rc_aabb2f a, rc_aabb2f b)
{
    return (rc_aabb2f) {
        rc_vec2f_component_min(a.min, b.min),
        rc_vec2f_component_max(a.max, b.max)
    };
}

/* Smallest box that contains a and the additional point p. */
static inline rc_aabb2f rc_aabb2f_expand(rc_aabb2f a, rc_vec2f p)
{
    return (rc_aabb2f) {
        rc_vec2f_component_min(a.min, p),
        rc_vec2f_component_max(a.max, p)
    };
}

#endif /* RC_MATH_AABB2F_H_ */
