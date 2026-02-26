/*
 * math/vec4f.h - 4D single-precision float vector (rc_vec4f).
 *
 * All operations are inline; no heap allocation.
 *
 * Type
 * ----
 *   rc_vec4f  { float x, y, z, w; }
 *
 * Construction
 * ------------
 *   rc_vec4f_make(x, y, z, w)     — from components
 *   rc_vec4f_make_zero()          — (0, 0, 0, 0)
 *   rc_vec4f_make_unitx/y/z/w()   — axis unit vectors
 *   rc_vec4f_from_floats(p)       — from float[4]
 *   rc_vec4f_from_vec2f(v, z, w)  — extend 2D vector
 *   rc_vec4f_from_vec3f(v, w)     — extend 3D vector
 *
 * Conversion
 * ----------
 *   rc_vec4f_as_floats(a)         — pointer to x component (float[4])
 *
 * Arithmetic
 * ----------
 *   add, add3, add4, sub, scalar_mul, scalar_div, component_mul,
 *   component_min, component_max, component_floor, component_ceil,
 *   component_abs, lerp, dot, lengthsqr, length,
 *   normalize (asserts non-zero), normalize_safe,
 *   negate, is_nearly_equal, is_equal
 *
 * Container types (generated via array.h template)
 * -------------------------------------------------
 *   rc_view_vec4f, rc_span_vec4f, rc_array_vec4f
 */

#ifndef RC_MATH_VEC4F_H_
#define RC_MATH_VEC4F_H_

#include "richc/math/vec3f.h"

/* ---- type ---- */

typedef struct {
    float x, y, z, w;
} rc_vec4f;

/* ---- construction ---- */

static inline rc_vec4f rc_vec4f_make(float x, float y, float z, float w)
{
    return (rc_vec4f) {x, y, z, w};
}

static inline rc_vec4f rc_vec4f_make_zero(void)  { return (rc_vec4f) {0.0f, 0.0f, 0.0f, 0.0f}; }
static inline rc_vec4f rc_vec4f_make_unitx(void) { return (rc_vec4f) {1.0f, 0.0f, 0.0f, 0.0f}; }
static inline rc_vec4f rc_vec4f_make_unity(void) { return (rc_vec4f) {0.0f, 1.0f, 0.0f, 0.0f}; }
static inline rc_vec4f rc_vec4f_make_unitz(void) { return (rc_vec4f) {0.0f, 0.0f, 1.0f, 0.0f}; }
static inline rc_vec4f rc_vec4f_make_unitw(void) { return (rc_vec4f) {0.0f, 0.0f, 0.0f, 1.0f}; }

/* ---- construction from other types ---- */

/* Construct from a pointer to four consecutive float values. */
static inline rc_vec4f rc_vec4f_from_floats(const float *f)
{
    return (rc_vec4f) {f[0], f[1], f[2], f[3]};
}

/* Extend a 2D vector with explicit z and w components. */
static inline rc_vec4f rc_vec4f_from_vec2f(rc_vec2f v, float z, float w)
{
    return (rc_vec4f) {v.x, v.y, z, w};
}

/* Extend a 3D vector with an explicit w component. */
static inline rc_vec4f rc_vec4f_from_vec3f(rc_vec3f v, float w)
{
    return (rc_vec4f) {v.x, v.y, v.z, w};
}

/* ---- conversion ---- */

/* Pointer to the x component; may be used as a float[4]. */
static inline const float *rc_vec4f_as_floats(const rc_vec4f *a)
{
    return &a->x;
}

/* ---- arithmetic ---- */

static inline rc_vec4f rc_vec4f_add(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

static inline rc_vec4f rc_vec4f_add3(rc_vec4f a, rc_vec4f b, rc_vec4f c)
{
    return (rc_vec4f) {
        a.x + b.x + c.x,
        a.y + b.y + c.y,
        a.z + b.z + c.z,
        a.w + b.w + c.w
    };
}

static inline rc_vec4f rc_vec4f_add4(rc_vec4f a, rc_vec4f b, rc_vec4f c, rc_vec4f d)
{
    return (rc_vec4f) {
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z,
        a.w + b.w + c.w + d.w
    };
}

static inline rc_vec4f rc_vec4f_sub(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

static inline rc_vec4f rc_vec4f_scalar_mul(rc_vec4f a, float s)
{
    return (rc_vec4f) {a.x * s, a.y * s, a.z * s, a.w * s};
}

static inline rc_vec4f rc_vec4f_scalar_div(rc_vec4f a, float s)
{
    return (rc_vec4f) {a.x / s, a.y / s, a.z / s, a.w / s};
}

/* Component-wise multiplication. */
static inline rc_vec4f rc_vec4f_component_mul(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}

/* Component-wise minimum. */
static inline rc_vec4f rc_vec4f_component_min(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z), fminf(a.w, b.w)};
}

/* Component-wise maximum. */
static inline rc_vec4f rc_vec4f_component_max(rc_vec4f a, rc_vec4f b)
{
    return (rc_vec4f) {fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z), fmaxf(a.w, b.w)};
}

/* Component-wise floor. */
static inline rc_vec4f rc_vec4f_component_floor(rc_vec4f a)
{
    return (rc_vec4f) {floorf(a.x), floorf(a.y), floorf(a.z), floorf(a.w)};
}

/* Component-wise ceil. */
static inline rc_vec4f rc_vec4f_component_ceil(rc_vec4f a)
{
    return (rc_vec4f) {ceilf(a.x), ceilf(a.y), ceilf(a.z), ceilf(a.w)};
}

/* Component-wise absolute value. */
static inline rc_vec4f rc_vec4f_component_abs(rc_vec4f a)
{
    return (rc_vec4f) {fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w)};
}

/* Linear interpolation: a + (b - a) * t. */
static inline rc_vec4f rc_vec4f_lerp(rc_vec4f a, rc_vec4f b, float t)
{
    return (rc_vec4f) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    };
}

/* Dot product. */
static inline float rc_vec4f_dot(rc_vec4f a, rc_vec4f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

/* Squared length. */
static inline float rc_vec4f_lengthsqr(rc_vec4f a)
{
    return rc_vec4f_dot(a, a);
}

static inline float rc_vec4f_length(rc_vec4f a)
{
    return sqrtf(rc_vec4f_lengthsqr(a));
}

/* Normalize to unit length.  Asserts that the vector is non-zero. */
static inline rc_vec4f rc_vec4f_normalize(rc_vec4f a)
{
    float len = rc_vec4f_length(a);
    RC_ASSERT(len != 0.0f);
    return rc_vec4f_scalar_div(a, len);
}

/* Normalize if length >= tolerance; otherwise return zero. */
static inline rc_vec4f rc_vec4f_normalize_safe(rc_vec4f a, float tolerance)
{
    float len = rc_vec4f_length(a);
    return (len >= tolerance) ? rc_vec4f_scalar_div(a, len) : rc_vec4f_make_zero();
}

static inline rc_vec4f rc_vec4f_negate(rc_vec4f a)
{
    return (rc_vec4f) {-a.x, -a.y, -a.z, -a.w};
}

/* True if the squared distance between a and b is less than tolerance^2. */
static inline bool rc_vec4f_is_nearly_equal(rc_vec4f a, rc_vec4f b, float tolerance)
{
    return rc_vec4f_lengthsqr(rc_vec4f_sub(a, b)) < tolerance * tolerance;
}

/* Exact equality (no tolerance). */
static inline bool rc_vec4f_is_equal(rc_vec4f a, rc_vec4f b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

/* ---- container types ---- */

#define ARRAY_T    rc_vec4f
#define ARRAY_NAME rc_array_vec4f
#define ARRAY_VIEW rc_view_vec4f
#define ARRAY_SPAN rc_span_vec4f
#include "richc/template/array.h"

#endif /* RC_MATH_VEC4F_H_ */
