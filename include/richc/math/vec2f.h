/*
 * math/vec2f.h - 2D single-precision float vector (rc_vec2f).
 *
 * All operations are inline; no heap allocation.
 *
 * Type
 * ----
 *   rc_vec2f  { float x, y; }
 *
 * Construction
 * ------------
 *   rc_vec2f_make(x, y)          — from components
 *   rc_vec2f_make_zero()         — (0, 0)
 *   rc_vec2f_make_unitx()        — (1, 0)
 *   rc_vec2f_make_unity()        — (0, 1)
 *   rc_vec2f_make_sincos(angle)  — (sin, cos) — useful for rotations stored as
 *                                   the imaginary and real parts of a unit complex
 *   rc_vec2f_make_cossin(angle)  — (cos, sin)
 *   rc_vec2f_from_floats(p)      — from float[2]
 *   rc_vec2f_from_vec2i(v)       — cast from rc_vec2i
 *
 * Conversion
 * ----------
 *   rc_vec2f_as_floats(a)        — pointer to x component (float[2])
 *
 * Arithmetic
 * ----------
 *   add, add3, add4, sub, scalar_mul, scalar_div, component_mul,
 *   component_min, component_max, component_floor, component_ceil,
 *   component_abs, lerp, dot, wedge, perp,
 *   lengthsqr, length, normalize (asserts non-zero), normalize_safe,
 *   negate, is_nearly_equal, is_equal
 *
 * Container types (generated via array.h template)
 * -------------------------------------------------
 *   rc_view_vec2f, rc_span_vec2f, rc_array_vec2f
 */

#ifndef RC_MATH_VEC2F_H_
#define RC_MATH_VEC2F_H_

#include <math.h>
#include "richc/debug.h"
#include "richc/math/vec2i.h"

/* ---- type ---- */

typedef struct {
    float x, y;
} rc_vec2f;

/* ---- construction ---- */

static inline rc_vec2f rc_vec2f_make(float x, float y)
{
    return (rc_vec2f) {x, y};
}

static inline rc_vec2f rc_vec2f_make_zero(void)  { return (rc_vec2f) {0.0f, 0.0f}; }
static inline rc_vec2f rc_vec2f_make_unitx(void) { return (rc_vec2f) {1.0f, 0.0f}; }
static inline rc_vec2f rc_vec2f_make_unity(void) { return (rc_vec2f) {0.0f, 1.0f}; }

/* (sin(angle), cos(angle)) — the imaginary then real parts of a unit complex. */
static inline rc_vec2f rc_vec2f_make_sincos(float angle)
{
    return (rc_vec2f) {sinf(angle), cosf(angle)};
}

/* (cos(angle), sin(angle)) — x-aligned basis. */
static inline rc_vec2f rc_vec2f_make_cossin(float angle)
{
    return (rc_vec2f) {cosf(angle), sinf(angle)};
}

/* ---- construction from other types ---- */

/* Construct from a pointer to two consecutive float values. */
static inline rc_vec2f rc_vec2f_from_floats(const float *f)
{
    return (rc_vec2f) {f[0], f[1]};
}

/* Cast integer vector to float. */
static inline rc_vec2f rc_vec2f_from_vec2i(rc_vec2i v)
{
    return (rc_vec2f) {(float)v.x, (float)v.y};
}

/* ---- conversion ---- */

/* Pointer to the x component; may be used as a float[2]. */
static inline const float *rc_vec2f_as_floats(const rc_vec2f *a)
{
    return &a->x;
}

/* ---- arithmetic ---- */

static inline rc_vec2f rc_vec2f_add(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {a.x + b.x, a.y + b.y};
}

static inline rc_vec2f rc_vec2f_add3(rc_vec2f a, rc_vec2f b, rc_vec2f c)
{
    return (rc_vec2f) {a.x + b.x + c.x, a.y + b.y + c.y};
}

static inline rc_vec2f rc_vec2f_add4(rc_vec2f a, rc_vec2f b, rc_vec2f c, rc_vec2f d)
{
    return (rc_vec2f) {a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y};
}

static inline rc_vec2f rc_vec2f_sub(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {a.x - b.x, a.y - b.y};
}

static inline rc_vec2f rc_vec2f_scalar_mul(rc_vec2f a, float s)
{
    return (rc_vec2f) {a.x * s, a.y * s};
}

static inline rc_vec2f rc_vec2f_scalar_div(rc_vec2f a, float s)
{
    return (rc_vec2f) {a.x / s, a.y / s};
}

/* Component-wise multiplication. */
static inline rc_vec2f rc_vec2f_component_mul(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {a.x * b.x, a.y * b.y};
}

/* Component-wise minimum. */
static inline rc_vec2f rc_vec2f_component_min(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {fminf(a.x, b.x), fminf(a.y, b.y)};
}

/* Component-wise maximum. */
static inline rc_vec2f rc_vec2f_component_max(rc_vec2f a, rc_vec2f b)
{
    return (rc_vec2f) {fmaxf(a.x, b.x), fmaxf(a.y, b.y)};
}

/* Component-wise floor. */
static inline rc_vec2f rc_vec2f_component_floor(rc_vec2f a)
{
    return (rc_vec2f) {floorf(a.x), floorf(a.y)};
}

/* Component-wise ceil. */
static inline rc_vec2f rc_vec2f_component_ceil(rc_vec2f a)
{
    return (rc_vec2f) {ceilf(a.x), ceilf(a.y)};
}

/* Component-wise absolute value. */
static inline rc_vec2f rc_vec2f_component_abs(rc_vec2f a)
{
    return (rc_vec2f) {fabsf(a.x), fabsf(a.y)};
}

/* Linear interpolation: a + (b - a) * t. */
static inline rc_vec2f rc_vec2f_lerp(rc_vec2f a, rc_vec2f b, float t)
{
    return (rc_vec2f) {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t
    };
}

/* Dot product. */
static inline float rc_vec2f_dot(rc_vec2f a, rc_vec2f b)
{
    return a.x * b.x + a.y * b.y;
}

/* 2D wedge (cross) product: ax*by - bx*ay. */
static inline float rc_vec2f_wedge(rc_vec2f a, rc_vec2f b)
{
    return a.x * b.y - b.x * a.y;
}

/* Counter-clockwise perpendicular: (-y, x). */
static inline rc_vec2f rc_vec2f_perp(rc_vec2f a)
{
    return (rc_vec2f) {-a.y, a.x};
}

/* Squared length. */
static inline float rc_vec2f_lengthsqr(rc_vec2f a)
{
    return rc_vec2f_dot(a, a);
}

static inline float rc_vec2f_length(rc_vec2f a)
{
    return sqrtf(rc_vec2f_lengthsqr(a));
}

/* Normalize to unit length.  Asserts that the vector is non-zero. */
static inline rc_vec2f rc_vec2f_normalize(rc_vec2f a)
{
    float len = rc_vec2f_length(a);
    RC_ASSERT(len != 0.0f);
    return rc_vec2f_scalar_div(a, len);
}

/* Normalize if length >= tolerance; otherwise return zero. */
static inline rc_vec2f rc_vec2f_normalize_safe(rc_vec2f a, float tolerance)
{
    float len = rc_vec2f_length(a);
    return (len >= tolerance) ? rc_vec2f_scalar_div(a, len) : rc_vec2f_make_zero();
}

static inline rc_vec2f rc_vec2f_negate(rc_vec2f a)
{
    return (rc_vec2f) {-a.x, -a.y};
}

/* True if the squared distance between a and b is less than tolerance^2. */
static inline bool rc_vec2f_is_nearly_equal(rc_vec2f a, rc_vec2f b, float tolerance)
{
    return rc_vec2f_lengthsqr(rc_vec2f_sub(a, b)) < tolerance * tolerance;
}

/* Exact equality (no tolerance). */
static inline bool rc_vec2f_is_equal(rc_vec2f a, rc_vec2f b)
{
    return a.x == b.x && a.y == b.y;
}

/* ---- container types ---- */

#define ARRAY_T    rc_vec2f
#define ARRAY_NAME rc_array_vec2f
#define ARRAY_VIEW rc_view_vec2f
#define ARRAY_SPAN rc_span_vec2f
#include "richc/template/array.h"

#endif /* RC_MATH_VEC2F_H_ */
