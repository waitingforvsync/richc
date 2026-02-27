/*
 * math/vec3f.h - 3D single-precision float vector (rc_vec3f).
 *
 * All operations are inline; no heap allocation.
 *
 * Type
 * ----
 *   rc_vec3f  { float x, y, z; }
 *
 * Construction
 * ------------
 *   rc_vec3f_make(x, y, z)     — from components
 *   rc_vec3f_make_zero()       — (0, 0, 0)
 *   rc_vec3f_make_unitx/y/z()  — axis unit vectors
 *   rc_vec3f_from_floats(p)    — from float[3]
 *   rc_vec3f_from_vec2f(v, z)  — extend 2D vector
 *   rc_vec3f_from_vec3i(v)     — cast from rc_vec3i
 *
 * Conversion
 * ----------
 *   rc_vec3f_as_floats(a)      — pointer to x component (float[3])
 *
 * Arithmetic
 * ----------
 *   add, add3, add4, sub, scalar_mul, scalar_div, component_mul,
 *   component_min, component_max, component_floor, component_ceil,
 *   component_abs, lerp, dot, cross, lengthsqr, length,
 *   normalize (asserts non-zero), normalize_safe,
 *   negate, is_nearly_equal, is_equal
 *
 */

#ifndef RC_MATH_VEC3F_H_
#define RC_MATH_VEC3F_H_

#include "richc/math/vec2f.h"
#include "richc/math/vec3i.h"

/* ---- type ---- */

typedef struct {
    float x, y, z;
} rc_vec3f;

/* ---- construction ---- */

static inline rc_vec3f rc_vec3f_make(float x, float y, float z)
{
    return (rc_vec3f) {x, y, z};
}

static inline rc_vec3f rc_vec3f_make_zero(void)  { return (rc_vec3f) {0.0f, 0.0f, 0.0f}; }
static inline rc_vec3f rc_vec3f_make_unitx(void) { return (rc_vec3f) {1.0f, 0.0f, 0.0f}; }
static inline rc_vec3f rc_vec3f_make_unity(void) { return (rc_vec3f) {0.0f, 1.0f, 0.0f}; }
static inline rc_vec3f rc_vec3f_make_unitz(void) { return (rc_vec3f) {0.0f, 0.0f, 1.0f}; }

/* ---- construction from other types ---- */

/* Construct from a pointer to three consecutive float values. */
static inline rc_vec3f rc_vec3f_from_floats(const float *f)
{
    return (rc_vec3f) {f[0], f[1], f[2]};
}

/* Extend a 2D vector with an explicit z component. */
static inline rc_vec3f rc_vec3f_from_vec2f(rc_vec2f v, float z)
{
    return (rc_vec3f) {v.x, v.y, z};
}

/* Cast integer vector to float. */
static inline rc_vec3f rc_vec3f_from_vec3i(rc_vec3i v)
{
    return (rc_vec3f) {(float)v.x, (float)v.y, (float)v.z};
}

/* ---- conversion ---- */

/* Pointer to the x component; may be used as a float[3]. */
static inline const float *rc_vec3f_as_floats(const rc_vec3f *a)
{
    return &a->x;
}

/* ---- arithmetic ---- */

static inline rc_vec3f rc_vec3f_add(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline rc_vec3f rc_vec3f_add3(rc_vec3f a, rc_vec3f b, rc_vec3f c)
{
    return (rc_vec3f) {a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z};
}

static inline rc_vec3f rc_vec3f_add4(rc_vec3f a, rc_vec3f b, rc_vec3f c, rc_vec3f d)
{
    return (rc_vec3f) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z
    };
}

static inline rc_vec3f rc_vec3f_sub(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline rc_vec3f rc_vec3f_scalar_mul(rc_vec3f a, float s)
{
    return (rc_vec3f) {a.x * s, a.y * s, a.z * s};
}

static inline rc_vec3f rc_vec3f_scalar_div(rc_vec3f a, float s)
{
    return (rc_vec3f) {a.x / s, a.y / s, a.z / s};
}

/* Component-wise multiplication. */
static inline rc_vec3f rc_vec3f_component_mul(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {a.x * b.x, a.y * b.y, a.z * b.z};
}

/* Component-wise minimum. */
static inline rc_vec3f rc_vec3f_component_min(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)};
}

/* Component-wise maximum. */
static inline rc_vec3f rc_vec3f_component_max(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)};
}

/* Component-wise floor. */
static inline rc_vec3f rc_vec3f_component_floor(rc_vec3f a)
{
    return (rc_vec3f) {floorf(a.x), floorf(a.y), floorf(a.z)};
}

/* Component-wise ceil. */
static inline rc_vec3f rc_vec3f_component_ceil(rc_vec3f a)
{
    return (rc_vec3f) {ceilf(a.x), ceilf(a.y), ceilf(a.z)};
}

/* Component-wise absolute value. */
static inline rc_vec3f rc_vec3f_component_abs(rc_vec3f a)
{
    return (rc_vec3f) {fabsf(a.x), fabsf(a.y), fabsf(a.z)};
}

/* Linear interpolation: a + (b - a) * t. */
static inline rc_vec3f rc_vec3f_lerp(rc_vec3f a, rc_vec3f b, float t)
{
    return (rc_vec3f) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

/* Dot product. */
static inline float rc_vec3f_dot(rc_vec3f a, rc_vec3f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* Cross product: a × b. */
static inline rc_vec3f rc_vec3f_cross(rc_vec3f a, rc_vec3f b)
{
    return (rc_vec3f) {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

/* Squared length. */
static inline float rc_vec3f_lengthsqr(rc_vec3f a)
{
    return rc_vec3f_dot(a, a);
}

static inline float rc_vec3f_length(rc_vec3f a)
{
    return sqrtf(rc_vec3f_lengthsqr(a));
}

/* Normalize to unit length.  Asserts that the vector is non-zero. */
static inline rc_vec3f rc_vec3f_normalize(rc_vec3f a)
{
    float len = rc_vec3f_length(a);
    RC_ASSERT(len != 0.0f);
    return rc_vec3f_scalar_div(a, len);
}

/* Normalize if length >= tolerance; otherwise return zero. */
static inline rc_vec3f rc_vec3f_normalize_safe(rc_vec3f a, float tolerance)
{
    float len = rc_vec3f_length(a);
    return (len >= tolerance) ? rc_vec3f_scalar_div(a, len) : rc_vec3f_make_zero();
}

static inline rc_vec3f rc_vec3f_negate(rc_vec3f a)
{
    return (rc_vec3f) {-a.x, -a.y, -a.z};
}

/* True if the squared distance between a and b is less than tolerance^2. */
static inline bool rc_vec3f_is_nearly_equal(rc_vec3f a, rc_vec3f b, float tolerance)
{
    return rc_vec3f_lengthsqr(rc_vec3f_sub(a, b)) < tolerance * tolerance;
}

/* Exact equality (no tolerance). */
static inline bool rc_vec3f_is_equal(rc_vec3f a, rc_vec3f b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

#endif /* RC_MATH_VEC3F_H_ */
