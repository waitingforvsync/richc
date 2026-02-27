/*
 * math/vec2i.h - 2D integer vector (rc_vec2i).
 *
 * All operations are inline; no heap allocation.
 *
 * Type
 * ----
 *   rc_vec2i  { int32_t x, y; }
 *
 * Construction
 * ------------
 *   rc_vec2i_make(x, y)    — from components
 *   rc_vec2i_make_zero()   — (0, 0)
 *   rc_vec2i_make_unitx()  — (1, 0)
 *   rc_vec2i_make_unity()  — (0, 1)
 *   rc_vec2i_from_i32s(p)  — from int32_t[2]
 *
 * Conversion
 * ----------
 *   rc_vec2i_as_i32s(a)    — pointer to first component (int32_t[2])
 *
 * Arithmetic
 * ----------
 *   add, add3, add4, sub, scalar_mul, scalar_div, component_mul,
 *   component_min, component_max, perp, negate, is_equal
 *
 * Scalar results (int64_t, assert no overflow)
 * --------------------------------------------
 *   dot, wedge, lengthsqr
 *
 * Note: length is not provided because the exact integer result cannot be
 * represented in general.
 */

#ifndef RC_MATH_VEC2I_H_
#define RC_MATH_VEC2I_H_

#include <stdint.h>
#include <stdbool.h>
#include "richc/debug.h"
#include "richc/math/math.h"

/* ---- type ---- */

typedef struct {
    int32_t x, y;
} rc_vec2i;

/* ---- construction ---- */

static inline rc_vec2i rc_vec2i_make(int32_t x, int32_t y)
{
    return (rc_vec2i) {x, y};
}

static inline rc_vec2i rc_vec2i_make_zero(void)   { return (rc_vec2i) {0, 0}; }
static inline rc_vec2i rc_vec2i_make_unitx(void)  { return (rc_vec2i) {1, 0}; }
static inline rc_vec2i rc_vec2i_make_unity(void)  { return (rc_vec2i) {0, 1}; }

/* Construct from a pointer to two consecutive int32_t values. */
static inline rc_vec2i rc_vec2i_from_i32s(const int32_t *i)
{
    return (rc_vec2i) {i[0], i[1]};
}

/* ---- conversion ---- */

/* Pointer to the x component; may be used as an int32_t[2]. */
static inline const int32_t *rc_vec2i_as_i32s(const rc_vec2i *a)
{
    return &a->x;
}

/* ---- arithmetic ---- */

static inline rc_vec2i rc_vec2i_add(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {a.x + b.x, a.y + b.y};
}

static inline rc_vec2i rc_vec2i_add3(rc_vec2i a, rc_vec2i b, rc_vec2i c)
{
    return (rc_vec2i) {a.x + b.x + c.x, a.y + b.y + c.y};
}

static inline rc_vec2i rc_vec2i_add4(rc_vec2i a, rc_vec2i b, rc_vec2i c, rc_vec2i d)
{
    return (rc_vec2i) {a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y};
}

static inline rc_vec2i rc_vec2i_sub(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {a.x - b.x, a.y - b.y};
}

static inline rc_vec2i rc_vec2i_scalar_mul(rc_vec2i a, int32_t s)
{
    return (rc_vec2i) {a.x * s, a.y * s};
}

static inline rc_vec2i rc_vec2i_scalar_div(rc_vec2i a, int32_t s)
{
    RC_ASSERT(s != 0);
    return (rc_vec2i) {a.x / s, a.y / s};
}

/* Component-wise multiplication. */
static inline rc_vec2i rc_vec2i_component_mul(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {a.x * b.x, a.y * b.y};
}

/* Component-wise minimum. */
static inline rc_vec2i rc_vec2i_component_min(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        (a.x < b.x) ? a.x : b.x,
        (a.y < b.y) ? a.y : b.y
    };
}

/* Component-wise maximum. */
static inline rc_vec2i rc_vec2i_component_max(rc_vec2i a, rc_vec2i b)
{
    return (rc_vec2i) {
        (a.x > b.x) ? a.x : b.x,
        (a.y > b.y) ? a.y : b.y
    };
}

/* Dot product.  Asserts that the result does not overflow int64_t. */
static inline int64_t rc_vec2i_dot(rc_vec2i a, rc_vec2i b)
{
    int64_t p0 = (int64_t)a.x * b.x;
    int64_t p1 = (int64_t)a.y * b.y;
    RC_ASSERT(!rc_add_overflows_i64(p0, p1));
    return p0 + p1;
}

/* 2D wedge (cross) product: ax*by - bx*ay.  Asserts no int64_t overflow. */
static inline int64_t rc_vec2i_wedge(rc_vec2i a, rc_vec2i b)
{
    int64_t p0 = (int64_t)a.x * b.y;
    int64_t p1 = (int64_t)b.x * a.y;
    RC_ASSERT(!rc_sub_overflows_i64(p0, p1));
    return p0 - p1;
}

/* Counter-clockwise perpendicular: (-y, x). */
static inline rc_vec2i rc_vec2i_perp(rc_vec2i a)
{
    return (rc_vec2i) {-a.y, a.x};
}

/* Squared length.  Asserts that the result does not overflow int64_t. */
static inline int64_t rc_vec2i_lengthsqr(rc_vec2i a)
{
    return rc_vec2i_dot(a, a);
}

static inline rc_vec2i rc_vec2i_negate(rc_vec2i a)
{
    return (rc_vec2i) {-a.x, -a.y};
}

static inline bool rc_vec2i_is_equal(rc_vec2i a, rc_vec2i b)
{
    return a.x == b.x && a.y == b.y;
}

#endif /* RC_MATH_VEC2I_H_ */
