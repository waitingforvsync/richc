/*
 * math/mat34f.h - 3D affine transform (rc_mat34f).
 *
 * Represents the 3×4 augmented matrix:
 *
 *   | m.cx.x  m.cy.x  m.cz.x  t.x |
 *   | m.cx.y  m.cy.y  m.cz.y  t.y |
 *   | m.cx.z  m.cy.z  m.cz.z  t.z |
 *
 * Applied to a column vector v as:  m.m * v + m.t
 * This encodes rotation, scale, shear, and translation in 3D.
 *
 * Type
 * ----
 *   rc_mat34f  { rc_mat33f m; rc_vec3f t; }
 *
 * Construction
 * ------------
 *   rc_mat34f_make(m, t)               — from linear part and translation
 *   rc_mat34f_make_identity()          — identity transform
 *   rc_mat34f_make_translation(v)      — pure translation
 *   rc_mat34f_make_lookat(eye,focus,up)— view matrix (eye at origin, -z forward)
 *   rc_mat34f_from_mat33f(m)           — embed linear map (zero translation)
 *   rc_mat34f_from_floats(p)           — from float[12], column-major
 *
 * Conversion
 * ----------
 *   rc_mat34f_as_floats(m)             — pointer to first float (float[12])
 *
 * Operations
 * ----------
 *   vec3f_mul (M*v with translation), mul (M*M),
 *   mat33f_mat34f_mul, mat34f_mat33f_mul,
 *   inverse (asserts linear part is invertible)
 */

#ifndef RC_MATH_MAT34F_H_
#define RC_MATH_MAT34F_H_

#include "richc/math/mat33f.h"

/* ---- type ---- */

typedef struct {
    rc_mat33f m;
    rc_vec3f  t;
} rc_mat34f;

/* ---- construction ---- */

static inline rc_mat34f rc_mat34f_make(rc_mat33f m, rc_vec3f t)
{
    return (rc_mat34f) {m, t};
}

static inline rc_mat34f rc_mat34f_make_identity(void)
{
    return (rc_mat34f) {rc_mat33f_make_identity(), rc_vec3f_make_zero()};
}

/* Pure translation: identity linear part, translation t. */
static inline rc_mat34f rc_mat34f_make_translation(rc_vec3f t)
{
    return (rc_mat34f) {rc_mat33f_make_identity(), t};
}

/*
 * Rigid-body view matrix (look-at).
 *
 * Produces a transform that maps world coordinates into camera space where:
 *   - origin is at `eye`
 *   - -Z points toward `focus`
 *   - Y is approximately aligned with `up`
 *
 * The resulting matrix transforms world-space points into camera space.
 * `forward` and `side` are computed from the input; `up` need only be
 * approximately correct (it is orthogonalised automatically).
 */
static inline rc_mat34f rc_mat34f_make_lookat(rc_vec3f eye, rc_vec3f focus, rc_vec3f up)
{
    rc_vec3f forward = rc_vec3f_normalize(rc_vec3f_sub(focus, eye));
    rc_vec3f side    = rc_vec3f_normalize(rc_vec3f_cross(forward, up));
    /* Reconstruct the true up from the orthogonal side and forward. */
    rc_mat33f mi = rc_mat33f_make_transpose(
        side,
        rc_vec3f_cross(side, forward),
        rc_vec3f_negate(forward)
    );
    return (rc_mat34f) {
        mi,
        rc_mat33f_vec3f_mul(mi, rc_vec3f_negate(eye))
    };
}

/* ---- construction from other types ---- */

/* Embed a linear map into a 3D affine transform (zero translation). */
static inline rc_mat34f rc_mat34f_from_mat33f(rc_mat33f m)
{
    return (rc_mat34f) {m, rc_vec3f_make_zero()};
}

/* Construct from a pointer to twelve consecutive floats (column-major). */
static inline rc_mat34f rc_mat34f_from_floats(const float *f)
{
    return (rc_mat34f) {
        rc_mat33f_from_floats(f),
        rc_vec3f_from_floats(f + 9)
    };
}

/* ---- conversion ---- */

/* Pointer to the first float element (float[12], column-major). */
static inline const float *rc_mat34f_as_floats(const rc_mat34f *m)
{
    return &m->m.cx.x;
}

/* ---- operations ---- */

/* Affine transform of a point: m.m * v + m.t. */
static inline rc_vec3f rc_mat34f_vec3f_mul(rc_mat34f m, rc_vec3f v)
{
    return rc_vec3f_add(rc_mat33f_vec3f_mul(m.m, v), m.t);
}

/* Composition of two affine transforms: a * b. */
static inline rc_mat34f rc_mat34f_mul(rc_mat34f a, rc_mat34f b)
{
    return (rc_mat34f) {
        rc_mat33f_mul(a.m, b.m),
        rc_mat34f_vec3f_mul(a, b.t)
    };
}

/* Left-multiply an affine transform by a linear map. */
static inline rc_mat34f rc_mat33f_mat34f_mul(rc_mat33f a, rc_mat34f b)
{
    return (rc_mat34f) {
        rc_mat33f_mul(a, b.m),
        rc_mat33f_vec3f_mul(a, b.t)
    };
}

/* Right-multiply an affine transform by a linear map (translation unchanged). */
static inline rc_mat34f rc_mat34f_mat33f_mul(rc_mat34f a, rc_mat33f b)
{
    return (rc_mat34f) {
        rc_mat33f_mul(a.m, b),
        a.t
    };
}

/* Inverse affine transform.  Delegates to rc_mat33f_inverse (asserts det != 0). */
static inline rc_mat34f rc_mat34f_inverse(rc_mat34f m)
{
    rc_mat33f mi = rc_mat33f_inverse(m.m);
    return (rc_mat34f) {
        mi,
        rc_mat33f_vec3f_mul(mi, rc_vec3f_negate(m.t))
    };
}

#endif /* RC_MATH_MAT34F_H_ */
