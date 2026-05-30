/*
 * math/mat23f.h - 2D affine transform (rc_mat23f).
 *
 * Represents the 2x3 augmented matrix:
 *
 *   | rot.cx.x  rot.cy.x  trans.x |
 *   | rot.cx.y  rot.cy.y  trans.y |
 *
 * Applied to a column vector v as:  rot * v + trans
 * This encodes rotation, scale, and translation in 2D.
 *
 * Type
 * ----
 *   rc_mat23f  { rc_mat22f rot; rc_vec2f trans; }
 *
 * Construction
 * ------------
 *   rc_mat23f_make(rot, trans)     - from linear part and translation
 *   rc_mat23f_make_identity()      - identity transform
 *   rc_mat23f_make_translation(v)  - pure translation
 *   rc_mat23f_from_mat22f(m)       - embed linear map (zero translation)
 *   rc_mat23f_from_floats(p)       - from float[6], column-major
 *
 * Conversion
 * ----------
 *   rc_mat23f_as_floats(m)         - pointer to first float (float[6])
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

typedef struct rc_mat23f {
    rc_mat22f rot;
    rc_vec2f  trans;
} rc_mat23f;

/* ---- construction ---- */

static inline rc_mat23f rc_mat23f_make(rc_mat22f rot, rc_vec2f trans)
{
    return (rc_mat23f) {rot, trans};
}

static inline rc_mat23f rc_mat23f_make_identity(void)
{
    return (rc_mat23f) {rc_mat22f_make_identity(), rc_vec2f_make_zero()};
}

/* Pure translation: identity linear part, translation trans. */
static inline rc_mat23f rc_mat23f_make_translation(rc_vec2f trans)
{
    return (rc_mat23f) {rc_mat22f_make_identity(), trans};
}

/* ---- construction from other types ---- */

/* Embed a linear map into a 2D affine transform (zero translation). */
static inline rc_mat23f rc_mat23f_from_mat22f(rc_mat22f rot)
{
    return (rc_mat23f) {rot, rc_vec2f_make_zero()};
}

/* Construct from a pointer to six consecutive floats (columns: rot.cx, rot.cy, trans). */
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
    return &m->rot.cx.x;
}

/* ---- operations ---- */

/* Affine transform of a point: rot * v + trans. */
static inline rc_vec2f rc_mat23f_vec2f_mul(rc_mat23f m, rc_vec2f v)
{
    return rc_vec2f_add(rc_mat22f_vec2f_mul(m.rot, v), m.trans);
}

/* Composition of two affine transforms: a * b. */
static inline rc_mat23f rc_mat23f_mul(rc_mat23f a, rc_mat23f b)
{
    return (rc_mat23f) {
        rc_mat22f_mul(a.rot, b.rot),
        rc_mat23f_vec2f_mul(a, b.trans)
    };
}

/* Left-multiply an affine transform by a linear map. */
static inline rc_mat23f rc_mat22f_mat23f_mul(rc_mat22f a, rc_mat23f b)
{
    return (rc_mat23f) {
        rc_mat22f_mul(a, b.rot),
        rc_mat22f_vec2f_mul(a, b.trans)
    };
}

/* Right-multiply an affine transform by a linear map (translation unchanged). */
static inline rc_mat23f rc_mat23f_mat22f_mul(rc_mat23f a, rc_mat22f b)
{
    return (rc_mat23f) {
        rc_mat22f_mul(a.rot, b),
        a.trans
    };
}

/* Inverse affine transform.  Delegates to rc_mat22f_inverse (asserts det != 0). */
static inline rc_mat23f rc_mat23f_inverse(rc_mat23f m)
{
    rc_mat22f inv = rc_mat22f_inverse(m.rot);
    return (rc_mat23f) {
        inv,
        rc_mat22f_vec2f_mul(inv, rc_vec2f_negate(m.trans))
    };
}

#endif /* RC_MATH_MAT23F_H_ */
