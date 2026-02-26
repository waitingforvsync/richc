/*
 * math/mat33f.h - 3×3 single-precision float matrix (rc_mat33f).
 *
 * Column-major storage: cx is the first column, cy the second, cz the third.
 * Written explicitly:
 *
 *   | cx.x  cy.x  cz.x |
 *   | cx.y  cy.y  cz.y |
 *   | cx.z  cy.z  cz.z |
 *
 * A column vector v is transformed as:  m * v = cx*v.x + cy*v.y + cz*v.z
 *
 * Type
 * ----
 *   rc_mat33f  { rc_vec3f cx, cy, cz; }
 *
 * Construction
 * ------------
 *   rc_mat33f_make(cx, cy, cz)          — from column vectors
 *   rc_mat33f_make_transpose(rx,ry,rz)  — from row vectors (transposes on store)
 *   rc_mat33f_make_zero()               — all zeros
 *   rc_mat33f_make_identity()           — identity matrix
 *   rc_mat33f_make_rotation_x/y/z(a)   — rotation about each axis (radians)
 *   rc_mat33f_from_floats(p)            — from float[9], column-major
 *
 * Conversion
 * ----------
 *   rc_mat33f_as_floats(m)              — pointer to first float (float[9])
 *
 * Operations
 * ----------
 *   add, sub, scalar_mul, vec3f_mul (M*v), mul (M*M),
 *   determinant, transpose, inverse (asserts det != 0)
 */

#ifndef RC_MATH_MAT33F_H_
#define RC_MATH_MAT33F_H_

#include "richc/math/vec3f.h"

/* ---- type ---- */

typedef struct {
    rc_vec3f cx, cy, cz;
} rc_mat33f;

/* ---- construction ---- */

static inline rc_mat33f rc_mat33f_make(rc_vec3f cx, rc_vec3f cy, rc_vec3f cz)
{
    return (rc_mat33f) {cx, cy, cz};
}

/*
 * Construct from three row vectors.  Transposes on store so that the result
 * is column-major with the given rows.  Useful when specifying a matrix
 * row-by-row (e.g. a rotation matrix from known row bases).
 */
static inline rc_mat33f rc_mat33f_make_transpose(rc_vec3f rx, rc_vec3f ry, rc_vec3f rz)
{
    return (rc_mat33f) {
        {rx.x, ry.x, rz.x},
        {rx.y, ry.y, rz.y},
        {rx.z, ry.z, rz.z}
    };
}

static inline rc_mat33f rc_mat33f_make_zero(void)
{
    return (rc_mat33f) {
        rc_vec3f_make_zero(),
        rc_vec3f_make_zero(),
        rc_vec3f_make_zero()
    };
}

static inline rc_mat33f rc_mat33f_make_identity(void)
{
    return (rc_mat33f) {
        rc_vec3f_make_unitx(),
        rc_vec3f_make_unity(),
        rc_vec3f_make_unitz()
    };
}

/* Rotation about the X axis by a radians. */
static inline rc_mat33f rc_mat33f_make_rotation_x(float a)
{
    float s = sinf(a), c = cosf(a);
    return (rc_mat33f) {
        {1.0f, 0.0f, 0.0f},
        {0.0f,    c,    s},
        {0.0f,   -s,    c}
    };
}

/* Rotation about the Y axis by a radians. */
static inline rc_mat33f rc_mat33f_make_rotation_y(float a)
{
    float s = sinf(a), c = cosf(a);
    return (rc_mat33f) {
        {   c, 0.0f,   -s},
        {0.0f, 1.0f, 0.0f},
        {   s, 0.0f,    c}
    };
}

/* Rotation about the Z axis by a radians. */
static inline rc_mat33f rc_mat33f_make_rotation_z(float a)
{
    float s = sinf(a), c = cosf(a);
    return (rc_mat33f) {
        {   c,    s, 0.0f},
        {  -s,    c, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
}

/* ---- construction from other types ---- */

/* Construct from a pointer to nine consecutive floats (column-major). */
static inline rc_mat33f rc_mat33f_from_floats(const float *f)
{
    return (rc_mat33f) {
        rc_vec3f_from_floats(f),
        rc_vec3f_from_floats(f + 3),
        rc_vec3f_from_floats(f + 6)
    };
}

/* ---- conversion ---- */

/* Pointer to the first float element (column-major, float[9]). */
static inline const float *rc_mat33f_as_floats(const rc_mat33f *m)
{
    return &m->cx.x;
}

/* ---- operations ---- */

static inline rc_mat33f rc_mat33f_add(rc_mat33f a, rc_mat33f b)
{
    return (rc_mat33f) {
        rc_vec3f_add(a.cx, b.cx),
        rc_vec3f_add(a.cy, b.cy),
        rc_vec3f_add(a.cz, b.cz)
    };
}

static inline rc_mat33f rc_mat33f_sub(rc_mat33f a, rc_mat33f b)
{
    return (rc_mat33f) {
        rc_vec3f_sub(a.cx, b.cx),
        rc_vec3f_sub(a.cy, b.cy),
        rc_vec3f_sub(a.cz, b.cz)
    };
}

static inline rc_mat33f rc_mat33f_scalar_mul(rc_mat33f m, float s)
{
    return (rc_mat33f) {
        rc_vec3f_scalar_mul(m.cx, s),
        rc_vec3f_scalar_mul(m.cy, s),
        rc_vec3f_scalar_mul(m.cz, s)
    };
}

/* Matrix-vector product: m * v. */
static inline rc_vec3f rc_mat33f_vec3f_mul(rc_mat33f m, rc_vec3f v)
{
    return rc_vec3f_add3(
        rc_vec3f_scalar_mul(m.cx, v.x),
        rc_vec3f_scalar_mul(m.cy, v.y),
        rc_vec3f_scalar_mul(m.cz, v.z)
    );
}

/* Matrix-matrix product: a * b. */
static inline rc_mat33f rc_mat33f_mul(rc_mat33f a, rc_mat33f b)
{
    return (rc_mat33f) {
        rc_mat33f_vec3f_mul(a, b.cx),
        rc_mat33f_vec3f_mul(a, b.cy),
        rc_mat33f_vec3f_mul(a, b.cz)
    };
}

/*
 * Determinant via the scalar triple product: det = cx . (cy × cz).
 * Equivalent to the full Leibniz expansion for a 3×3 column-major matrix.
 */
static inline float rc_mat33f_determinant(rc_mat33f a)
{
    return rc_vec3f_dot(a.cx, rc_vec3f_cross(a.cy, a.cz));
}

/* Transpose. */
static inline rc_mat33f rc_mat33f_transpose(rc_mat33f a)
{
    return (rc_mat33f) {
        {a.cx.x, a.cy.x, a.cz.x},
        {a.cx.y, a.cy.y, a.cz.y},
        {a.cx.z, a.cy.z, a.cz.z}
    };
}

/*
 * Inverse via the adjugate / cofactor method.
 *
 * The cofactor columns of the adjugate are the cross products of column pairs:
 *   x = cy × cz,  y = cz × cx,  z = cx × cy
 * The transpose of [x, y, z] is the classical adjugate.  Dividing by the
 * determinant (= cx . x) gives the inverse.  Asserts det != 0.
 */
static inline rc_mat33f rc_mat33f_inverse(rc_mat33f a)
{
    rc_vec3f x = rc_vec3f_cross(a.cy, a.cz);
    rc_vec3f y = rc_vec3f_cross(a.cz, a.cx);
    rc_vec3f z = rc_vec3f_cross(a.cx, a.cy);
    float det = rc_vec3f_dot(a.cx, x);
    RC_ASSERT(det != 0.0f);
    return rc_mat33f_scalar_mul(
        rc_mat33f_make_transpose(x, y, z),
        1.0f / det
    );
}

#endif /* RC_MATH_MAT33F_H_ */
