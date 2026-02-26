/*
 * math/mat23f.h - 2D affine transform (rc_mat23f).
 *
 * Represents the 2×3 augmented matrix:
 *
 *   | m.cx.x  m.cy.x  t.x |
 *   | m.cx.y  m.cy.y  t.y |
 *
 * Applied to a column vector v as:  m.m * v + m.t
 * This encodes rotation, scale, shear, and translation in 2D.
 *
 * Type
 * ----
 *   rc_mat23f  { rc_mat22f m; rc_vec2f t; }
 *
 * Construction
 * ------------
 *   rc_mat23f_make(m, t)           — from linear part and translation
 *   rc_mat23f_make_identity()      — identity transform
 *   rc_mat23f_make_translation(v)  — pure translation
 *   rc_mat23f_from_mat22f(m)       — embed linear map (zero translation)
 *   rc_mat23f_from_floats(p)       — from float[6], column-major
 *
 * Conversion
 * ----------
 *   rc_mat23f_as_floats(m)         — pointer to first float (float[6])
 *
 * Operations
 * ----------
 *   vec2f_mul (M*v with translation),
 *   mul (M*M), mat22f_mat23f_mul, mat23f_mat22f_mul,
 *   inverse (asserts linear part is invertible)
 */

#ifndef RC_MATH_MAT23F_H_
#define RC_MATH_MAT23F_H_

#include "richc/math/mat22f.h"

/* ---- type ---- */

typedef struct {
    rc_mat22f m;
    rc_vec2f  t;
} rc_mat23f;

/* ---- construction ---- */

static inline rc_mat23f rc_mat23f_make(rc_mat22f m, rc_vec2f t)
{
    return (rc_mat23f) {m, t};
}

static inline rc_mat23f rc_mat23f_make_identity(void)
{
    return (rc_mat23f) {rc_mat22f_make_identity(), rc_vec2f_make_zero()};
}

/* Pure translation: identity linear part, translation t. */
static inline rc_mat23f rc_mat23f_make_translation(rc_vec2f t)
{
    return (rc_mat23f) {rc_mat22f_make_identity(), t};
}

/* ---- construction from other types ---- */

/* Embed a linear map into a 2D affine transform (zero translation). */
static inline rc_mat23f rc_mat23f_from_mat22f(rc_mat22f m)
{
    return (rc_mat23f) {m, rc_vec2f_make_zero()};
}

/* Construct from a pointer to six consecutive floats (columns: m.cx, m.cy, t). */
static inline rc_mat23f rc_mat23f_from_floats(const float *f)
{
    return (rc_mat23f) {
        rc_mat22f_from_floats(f),
        rc_vec2f_from_floats(f + 4)
    };
}

/* ---- conversion ---- */

/* Pointer to the first float element (float[6], column-major). */
static inline const float *rc_mat23f_as_floats(const rc_mat23f *m)
{
    return &m->m.cx.x;
}

/* ---- operations ---- */

/* Affine transform of a point: m.m * v + m.t. */
static inline rc_vec2f rc_mat23f_vec2f_mul(rc_mat23f m, rc_vec2f v)
{
    return rc_vec2f_add(rc_mat22f_vec2f_mul(m.m, v), m.t);
}

/* Composition of two affine transforms: a * b. */
static inline rc_mat23f rc_mat23f_mul(rc_mat23f a, rc_mat23f b)
{
    return (rc_mat23f) {
        rc_mat22f_mul(a.m, b.m),
        rc_mat23f_vec2f_mul(a, b.t)
    };
}

/* Left-multiply an affine transform by a linear map. */
static inline rc_mat23f rc_mat22f_mat23f_mul(rc_mat22f a, rc_mat23f b)
{
    return (rc_mat23f) {
        rc_mat22f_mul(a, b.m),
        rc_mat22f_vec2f_mul(a, b.t)
    };
}

/* Right-multiply an affine transform by a linear map (translation unchanged). */
static inline rc_mat23f rc_mat23f_mat22f_mul(rc_mat23f a, rc_mat22f b)
{
    return (rc_mat23f) {
        rc_mat22f_mul(a.m, b),
        a.t
    };
}

/* Inverse affine transform.  Delegates to rc_mat22f_inverse (asserts det != 0). */
static inline rc_mat23f rc_mat23f_inverse(rc_mat23f m)
{
    rc_mat22f mi = rc_mat22f_inverse(m.m);
    return (rc_mat23f) {
        mi,
        rc_mat22f_vec2f_mul(mi, rc_vec2f_negate(m.t))
    };
}

#endif /* RC_MATH_MAT23F_H_ */
