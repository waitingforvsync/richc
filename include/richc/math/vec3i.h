/*
 * math/vec3i.h - 3D integer vector (rc_vec3i).
 *
 * All operations are inline; no heap allocation.
 *
 * Type
 * ----
 *   rc_vec3i  { int32_t x, y, z; }
 *
 * Construction
 * ------------
 *   rc_vec3i_make(x, y, z)     — from components
 *   rc_vec3i_make_zero()       — (0, 0, 0)
 *   rc_vec3i_make_unitx/y/z()  — axis unit vectors
 *   rc_vec3i_from_i32s(p)      — from int32_t[3]
 *   rc_vec3i_from_vec2i(v, z)  — extend 2D integer vector
 *
 * Conversion
 * ----------
 *   rc_vec3i_as_i32s(a)        — pointer to first component (int32_t[3])
 *
 * Arithmetic
 * ----------
 *   add, add3, add4, sub, scalar_mul, scalar_div, component_mul,
 *   component_min, component_max, negate, is_equal
 *
 * Scalar results (int64_t, assert no overflow)
 * --------------------------------------------
 *   dot, lengthsqr
 *
 * Vector results with range assertion
 * ------------------------------------
 *   cross — each component is computed in int64_t; asserts it fits in int32_t
 *
 * Note: length is not provided because the exact integer result cannot be
 * represented in general.
 */

#ifndef RC_MATH_VEC3I_H_
#define RC_MATH_VEC3I_H_

#include "richc/debug.h"
#include "richc/math/math.h"
#include "richc/math/vec2i.h"

/* ---- type ---- */

typedef struct {
    int32_t x, y, z;
} rc_vec3i;

/* ---- construction ---- */

static inline rc_vec3i rc_vec3i_make(int32_t x, int32_t y, int32_t z)
{
    return (rc_vec3i) {x, y, z};
}

static inline rc_vec3i rc_vec3i_make_zero(void)  { return (rc_vec3i) {0, 0, 0}; }
static inline rc_vec3i rc_vec3i_make_unitx(void) { return (rc_vec3i) {1, 0, 0}; }
static inline rc_vec3i rc_vec3i_make_unity(void) { return (rc_vec3i) {0, 1, 0}; }
static inline rc_vec3i rc_vec3i_make_unitz(void) { return (rc_vec3i) {0, 0, 1}; }

/* Construct from a pointer to three consecutive int32_t values. */
static inline rc_vec3i rc_vec3i_from_i32s(const int32_t *i)
{
    return (rc_vec3i) {i[0], i[1], i[2]};
}

/* Extend a 2D integer vector with an explicit z component. */
static inline rc_vec3i rc_vec3i_from_vec2i(rc_vec2i v, int32_t z)
{
    return (rc_vec3i) {v.x, v.y, z};
}

/* ---- conversion ---- */

/* Pointer to the x component; may be used as an int32_t[3]. */
static inline const int32_t *rc_vec3i_as_i32s(const rc_vec3i *a)
{
    return &a->x;
}

/* ---- arithmetic ---- */

static inline rc_vec3i rc_vec3i_add(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline rc_vec3i rc_vec3i_add3(rc_vec3i a, rc_vec3i b, rc_vec3i c)
{
    return (rc_vec3i) {a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z};
}

static inline rc_vec3i rc_vec3i_add4(rc_vec3i a, rc_vec3i b, rc_vec3i c, rc_vec3i d)
{
    return (rc_vec3i) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z
    };
}

static inline rc_vec3i rc_vec3i_sub(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline rc_vec3i rc_vec3i_scalar_mul(rc_vec3i a, int32_t s)
{
    return (rc_vec3i) {a.x * s, a.y * s, a.z * s};
}

static inline rc_vec3i rc_vec3i_scalar_div(rc_vec3i a, int32_t s)
{
    RC_ASSERT(s != 0);
    return (rc_vec3i) {a.x / s, a.y / s, a.z / s};
}

/* Component-wise multiplication. */
static inline rc_vec3i rc_vec3i_component_mul(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {a.x * b.x, a.y * b.y, a.z * b.z};
}

/* Component-wise minimum. */
static inline rc_vec3i rc_vec3i_component_min(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        (a.x < b.x) ? a.x : b.x,
        (a.y < b.y) ? a.y : b.y,
        (a.z < b.z) ? a.z : b.z
    };
}

/* Component-wise maximum. */
static inline rc_vec3i rc_vec3i_component_max(rc_vec3i a, rc_vec3i b)
{
    return (rc_vec3i) {
        (a.x > b.x) ? a.x : b.x,
        (a.y > b.y) ? a.y : b.y,
        (a.z > b.z) ? a.z : b.z
    };
}

/* Dot product.  Asserts that the result does not overflow int64_t. */
static inline int64_t rc_vec3i_dot(rc_vec3i a, rc_vec3i b)
{
    int64_t p0 = (int64_t)a.x * b.x;
    int64_t p1 = (int64_t)a.y * b.y;
    int64_t p2 = (int64_t)a.z * b.z;
    RC_ASSERT(!rc_add_overflows_i64(p0, p1));
    RC_ASSERT(!rc_add_overflows_i64(p0 + p1, p2));
    return p0 + p1 + p2;
}

/*
 * Cross product: a × b.  Each component is computed in int64_t; products of
 * two int32_t values never overflow int64_t, and the subsequent subtraction
 * also stays within int64_t range for all int32_t inputs.  Asserts that each
 * result component fits in int32_t before returning.
 */
static inline rc_vec3i rc_vec3i_cross(rc_vec3i a, rc_vec3i b)
{
    int64_t cx = (int64_t)a.y * b.z - (int64_t)a.z * b.y;
    int64_t cy = (int64_t)a.z * b.x - (int64_t)a.x * b.z;
    int64_t cz = (int64_t)a.x * b.y - (int64_t)a.y * b.x;
    RC_ASSERT(cx >= INT32_MIN && cx <= INT32_MAX);
    RC_ASSERT(cy >= INT32_MIN && cy <= INT32_MAX);
    RC_ASSERT(cz >= INT32_MIN && cz <= INT32_MAX);
    return (rc_vec3i) {(int32_t)cx, (int32_t)cy, (int32_t)cz};
}

/* Squared length.  Asserts that the result does not overflow int64_t. */
static inline int64_t rc_vec3i_lengthsqr(rc_vec3i a)
{
    return rc_vec3i_dot(a, a);
}

static inline rc_vec3i rc_vec3i_negate(rc_vec3i a)
{
    return (rc_vec3i) {-a.x, -a.y, -a.z};
}

static inline bool rc_vec3i_is_equal(rc_vec3i a, rc_vec3i b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

#endif /* RC_MATH_VEC3I_H_ */
