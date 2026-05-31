/*
 * math/mat34f.h - 3D affine transform (rc_mat34f).
 *
 * Represents the 3x4 augmented matrix:
 *
 *   | rot.cx.x  rot.cy.x  rot.cz.x  trans.x |
 *   | rot.cx.y  rot.cy.y  rot.cz.y  trans.y |
 *   | rot.cx.z  rot.cy.z  rot.cz.z  trans.z |
 *
 * Applied to a column vector v as:  rot * v + trans
 * This encodes rotation, scale, and translation in 3D.
 *
 * Type
 * ----
 *   rc_mat34f  { rc_mat33f rot; rc_vec3f trans; }
 *
 * Construction
 * ------------
 *   rc_mat34f_make(rot, trans)         - from linear part and translation
 *   rc_mat34f_make_identity()          - identity transform
 *   rc_mat34f_make_translation(v)      - pure translation
 *   rc_mat34f_make_lookat(eye,focus,up)- view matrix (eye at origin, -z forward)
 *   rc_mat34f_from_mat33f(m)           - embed linear map (zero translation)
 *   rc_mat34f_from_floats(p)           - from float[12], column-major
 *
 * Conversion
 * ----------
 *   rc_mat34f_as_floats(m)             - pointer to first float (float[12])
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

typedef struct rc_mat34f {
    rc_mat33f rot;
    rc_vec3f  trans;
} rc_mat34f;

/* ---- construction ---- */

static inline rc_mat34f rc_mat34f_make(rc_mat33f rot, rc_vec3f trans)
{
    return (rc_mat34f) {rot, trans};
}

static inline rc_mat34f rc_mat34f_make_identity(void)
{
    return (rc_mat34f) {rc_mat33f_make_identity(), rc_vec3f_make_zero()};
}

/* Pure translation: identity linear part, translation trans. */
static inline rc_mat34f rc_mat34f_make_translation(rc_vec3f trans)
{
    return (rc_mat34f) {rc_mat33f_make_identity(), trans};
}

/*
 * Rigid-body view matrix (look-at), right-handed.
 *
 * Produces a transform that maps world coordinates into camera space where:
 *   - the origin is at `eye`
 *   - -Z points toward `focus`
 *   - Y is approximately aligned with `up`
 *
 * `forward` and `side` are computed from the inputs; `up` need only be
 * approximately correct (it is orthogonalised automatically).
 */
static inline rc_mat34f rc_mat34f_make_lookat(rc_vec3f eye, rc_vec3f focus, rc_vec3f up)
{
    rc_vec3f forward = rc_vec3f_normalize(rc_vec3f_sub(focus, eye));
    rc_vec3f side    = rc_vec3f_normalize(rc_vec3f_cross(forward, up));
    // reconstruct the true up from the orthogonal side and forward
    rc_mat33f rot = rc_mat33f_make_transpose(
        side,
        rc_vec3f_cross(side, forward),
        rc_vec3f_negate(forward)
    );
    return (rc_mat34f) {
        rot,
        rc_mat33f_vec3f_mul(rot, rc_vec3f_negate(eye))
    };
}

/* ---- construction from other types ---- */

/* Embed a linear map into a 3D affine transform (zero translation). */
static inline rc_mat34f rc_mat34f_from_mat33f(rc_mat33f rot)
{
    return (rc_mat34f) {rot, rc_vec3f_make_zero()};
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
    return &m->rot.cx.x;
}

/* ---- operations ---- */

/* Affine transform of a point: rot * v + trans. */
static inline rc_vec3f rc_mat34f_vec3f_mul(rc_mat34f m, rc_vec3f v)
{
    return rc_vec3f_add(rc_mat33f_vec3f_mul(m.rot, v), m.trans);
}

/* Composition of two affine transforms: a * b. */
static inline rc_mat34f rc_mat34f_mul(rc_mat34f a, rc_mat34f b)
{
    return (rc_mat34f) {
        rc_mat33f_mul(a.rot, b.rot),
        rc_mat34f_vec3f_mul(a, b.trans)
    };
}

/* Left-multiply an affine transform by a linear map. */
static inline rc_mat34f rc_mat33f_mat34f_mul(rc_mat33f a, rc_mat34f b)
{
    return (rc_mat34f) {
        rc_mat33f_mul(a, b.rot),
        rc_mat33f_vec3f_mul(a, b.trans)
    };
}

/* Right-multiply an affine transform by a linear map (translation unchanged). */
static inline rc_mat34f rc_mat34f_mat33f_mul(rc_mat34f a, rc_mat33f b)
{
    return (rc_mat34f) {
        rc_mat33f_mul(a.rot, b),
        a.trans
    };
}

/* Inverse affine transform.  Delegates to rc_mat33f_inverse (asserts det != 0). */
static inline rc_mat34f rc_mat34f_inverse(rc_mat34f m)
{
    rc_mat33f inv = rc_mat33f_inverse(m.rot);
    return (rc_mat34f) {
        inv,
        rc_mat33f_vec3f_mul(inv, rc_vec3f_negate(m.trans))
    };
}

#endif /* RC_MATH_MAT34F_H_ */
