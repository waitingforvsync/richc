/*
 * math/mat22f.h - 2×2 single-precision float matrix (rc_mat22f).
 *
 * Column-major storage: cx is the first column, cy the second.
 * Written explicitly:
 *
 *   | cx.x  cy.x |
 *   | cx.y  cy.y |
 *
 * A column vector v is transformed as:  m * v = cx*v.x + cy*v.y
 *
 * Type
 * ----
 *   rc_mat22f  { rc_vec2f cx, cy; }
 *
 * Construction
 * ------------
 *   rc_mat22f_make(cx, cy)      — from column vectors
 *   rc_mat22f_make_zero()       — all zeros
 *   rc_mat22f_make_identity()   — identity matrix
 *   rc_mat22f_make_rotation(a)  — counter-clockwise rotation by angle a (radians)
 *   rc_mat22f_from_floats(p)    — from float[4], column-major
 *
 * Conversion
 * ----------
 *   rc_mat22f_as_floats(m)      — pointer to first float (float[4], column-major)
 *
 * Operations
 * ----------
 *   add, sub, scalar_mul, vec2f_mul (M*v), mul (M*M),
 *   determinant, transpose, inverse (asserts det != 0)
 */

#ifndef RC_MATH_MAT22F_H_
#define RC_MATH_MAT22F_H_

#include "richc/math/vec2f.h"

/* ---- type ---- */

typedef struct {
    rc_vec2f cx, cy;
} rc_mat22f;

/* ---- construction ---- */

static inline rc_mat22f rc_mat22f_make(rc_vec2f cx, rc_vec2f cy)
{
    return (rc_mat22f) {cx, cy};
}

static inline rc_mat22f rc_mat22f_make_zero(void)
{
    return (rc_mat22f) {rc_vec2f_make_zero(), rc_vec2f_make_zero()};
}

static inline rc_mat22f rc_mat22f_make_identity(void)
{
    return (rc_mat22f) {rc_vec2f_make_unitx(), rc_vec2f_make_unity()};
}

/* Counter-clockwise rotation matrix for angle a (radians). */
static inline rc_mat22f rc_mat22f_make_rotation(float a)
{
    float s = sinf(a);
    float c = cosf(a);
    return (rc_mat22f) {
        { c, s},
        {-s, c}
    };
}

/* ---- construction from other types ---- */

/* Construct from a pointer to four consecutive floats (column-major). */
static inline rc_mat22f rc_mat22f_from_floats(const float *f)
{
    return (rc_mat22f) {
        rc_vec2f_from_floats(f),
        rc_vec2f_from_floats(f + 2)
    };
}

/* ---- conversion ---- */

/* Pointer to the first float element (column-major, float[4]). */
static inline const float *rc_mat22f_as_floats(const rc_mat22f *m)
{
    return &m->cx.x;
}

/* ---- operations ---- */

static inline rc_mat22f rc_mat22f_add(rc_mat22f a, rc_mat22f b)
{
    return (rc_mat22f) {
        rc_vec2f_add(a.cx, b.cx),
        rc_vec2f_add(a.cy, b.cy)
    };
}

static inline rc_mat22f rc_mat22f_sub(rc_mat22f a, rc_mat22f b)
{
    return (rc_mat22f) {
        rc_vec2f_sub(a.cx, b.cx),
        rc_vec2f_sub(a.cy, b.cy)
    };
}

static inline rc_mat22f rc_mat22f_scalar_mul(rc_mat22f m, float s)
{
    return (rc_mat22f) {
        rc_vec2f_scalar_mul(m.cx, s),
        rc_vec2f_scalar_mul(m.cy, s)
    };
}

/* Matrix-vector product: m * v. */
static inline rc_vec2f rc_mat22f_vec2f_mul(rc_mat22f m, rc_vec2f v)
{
    return rc_vec2f_add(
        rc_vec2f_scalar_mul(m.cx, v.x),
        rc_vec2f_scalar_mul(m.cy, v.y)
    );
}

/* Matrix-matrix product: a * b. */
static inline rc_mat22f rc_mat22f_mul(rc_mat22f a, rc_mat22f b)
{
    return (rc_mat22f) {
        rc_mat22f_vec2f_mul(a, b.cx),
        rc_mat22f_vec2f_mul(a, b.cy)
    };
}

/* Determinant: cx.x * cy.y - cx.y * cy.x. */
static inline float rc_mat22f_determinant(rc_mat22f a)
{
    return a.cx.x * a.cy.y - a.cx.y * a.cy.x;
}

/* Transpose. */
static inline rc_mat22f rc_mat22f_transpose(rc_mat22f a)
{
    return (rc_mat22f) {
        {a.cx.x, a.cy.x},
        {a.cx.y, a.cy.y}
    };
}

/* Inverse.  Asserts that the determinant is non-zero. */
static inline rc_mat22f rc_mat22f_inverse(rc_mat22f a)
{
    float det = rc_mat22f_determinant(a);
    RC_ASSERT(det != 0.0f);
    float d = 1.0f / det;
    return (rc_mat22f) {
        { a.cy.y * d, -a.cx.y * d},
        {-a.cy.x * d,  a.cx.x * d}
    };
}

#endif /* RC_MATH_MAT22F_H_ */
