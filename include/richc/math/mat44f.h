/*
 * math/mat44f.h - 4×4 single-precision float matrix (rc_mat44f).
 *
 * Column-major storage: cx, cy, cz, cw are the four columns.
 * Written explicitly:
 *
 *   | cx.x  cy.x  cz.x  cw.x |
 *   | cx.y  cy.y  cz.y  cw.y |
 *   | cx.z  cy.z  cz.z  cw.z |
 *   | cx.w  cy.w  cz.w  cw.w |
 *
 * A column vector v is transformed as:  m * v = cx*v.x + cy*v.y + cz*v.z + cw*v.w
 *
 * Type
 * ----
 *   rc_mat44f  { rc_vec4f cx, cy, cz, cw; }
 *
 * Construction
 * ------------
 *   rc_mat44f_make(cx,cy,cz,cw)          — from column vectors
 *   rc_mat44f_make_transpose(...)        — from row vectors
 *   rc_mat44f_make_zero()                — all zeros
 *   rc_mat44f_make_identity()            — identity matrix
 *   rc_mat44f_make_translation(v)        — 3D translation (w row preserved)
 *   rc_mat44f_make_ortho(l,r,t,b,n,f)   — OpenGL-style ortho projection (n,f = near,far)
 *   rc_mat44f_make_perspective(fov,a,n,f)— OpenGL-style perspective projection
 *   rc_mat44f_from_mat22f/33f/34f(m)    — embed smaller matrix
 *   rc_mat44f_from_floats(p)            — from float[16], column-major
 *
 * Conversion
 * ----------
 *   rc_mat44f_as_floats(m)               — pointer to first float (float[16])
 *
 * Operations
 * ----------
 *   add, sub, scalar_mul, vec4f_mul (M*v), mul (M*M), transpose
 *   determinant  — non-inline, implemented in src/math/mat44f.c
 *   inverse      — non-inline, implemented in src/math/mat44f.c (asserts det != 0)
 */

#ifndef RC_MATH_MAT44F_H_
#define RC_MATH_MAT44F_H_

#include "richc/math/vec4f.h"
#include "richc/math/mat34f.h"
#include "richc/math/mat23f.h"

/* ---- type ---- */

typedef struct {
    rc_vec4f cx, cy, cz, cw;
} rc_mat44f;

/* ---- construction ---- */

static inline rc_mat44f rc_mat44f_make(rc_vec4f cx, rc_vec4f cy, rc_vec4f cz, rc_vec4f cw)
{
    return (rc_mat44f) {cx, cy, cz, cw};
}

/*
 * Construct from four row vectors.  Transposes on store so that the result
 * is column-major with the given rows.
 */
static inline rc_mat44f rc_mat44f_make_transpose(rc_vec4f rx, rc_vec4f ry, rc_vec4f rz, rc_vec4f rw)
{
    return (rc_mat44f) {
        {rx.x, ry.x, rz.x, rw.x},
        {rx.y, ry.y, rz.y, rw.y},
        {rx.z, ry.z, rz.z, rw.z},
        {rx.w, ry.w, rz.w, rw.w}
    };
}

static inline rc_mat44f rc_mat44f_make_zero(void)
{
    return (rc_mat44f) {
        rc_vec4f_make_zero(),
        rc_vec4f_make_zero(),
        rc_vec4f_make_zero(),
        rc_vec4f_make_zero()
    };
}

static inline rc_mat44f rc_mat44f_make_identity(void)
{
    return (rc_mat44f) {
        rc_vec4f_make_unitx(),
        rc_vec4f_make_unity(),
        rc_vec4f_make_unitz(),
        rc_vec4f_make_unitw()
    };
}

/* 3D translation (upper-left 3×3 is identity; last column is (v, 1)). */
static inline rc_mat44f rc_mat44f_make_translation(rc_vec3f v)
{
    return (rc_mat44f) {
        rc_vec4f_make_unitx(),
        rc_vec4f_make_unity(),
        rc_vec4f_make_unitz(),
        rc_vec4f_from_vec3f(v, 1.0f)
    };
}

/*
 * OpenGL-style orthographic projection.
 *
 * Maps the view volume [left,right] × [bottom,top] × [-n,-f] to the
 * NDC cube [-1,1]^3.
 */
static inline rc_mat44f rc_mat44f_make_ortho(
    float left, float right, float top, float bottom, float n, float f)
{
    float rl = right - left;
    float tb = top   - bottom;
    float fn = f - n;
    return (rc_mat44f) {
        rc_vec4f_scalar_mul(rc_vec4f_make_unitx(),  2.0f / rl),
        rc_vec4f_scalar_mul(rc_vec4f_make_unity(),  2.0f / tb),
        rc_vec4f_scalar_mul(rc_vec4f_make_unitz(), -2.0f / fn),
        {-(right + left) / rl, -(top + bottom) / tb, -(f + n) / fn, 1.0f}
    };
}

/*
 * OpenGL-style symmetric perspective projection.
 *
 *   y_fov  — vertical field of view in radians
 *   aspect — viewport width / height
 *   n, f   — near and far plane distances (positive values)
 *
 * Produces a clip matrix with -Z forward (right-handed, depth in [-1, 1]).
 */
static inline rc_mat44f rc_mat44f_make_perspective(float y_fov, float aspect, float n, float f)
{
    float a = 1.0f / tanf(y_fov / 2.0f);
    return (rc_mat44f) {
        {a / aspect, 0.0f,               0.0f,  0.0f},
        {0.0f,          a,               0.0f,  0.0f},
        {0.0f,       0.0f,  -(f + n) / (f - n), -1.0f},
        {0.0f,       0.0f, -2.0f * f * n / (f - n), 0.0f}
    };
}

/* ---- construction from other types ---- */

/* Embed a 2×2 matrix into the upper-left corner (identity elsewhere). */
static inline rc_mat44f rc_mat44f_from_mat22f(rc_mat22f m)
{
    return (rc_mat44f) {
        rc_vec4f_from_vec2f(m.cx, 0.0f, 0.0f),
        rc_vec4f_from_vec2f(m.cy, 0.0f, 0.0f),
        rc_vec4f_make_unitz(),
        rc_vec4f_make_unitw()
    };
}

/* Embed a 3×3 matrix into the upper-left corner (w column/row = unit). */
static inline rc_mat44f rc_mat44f_from_mat33f(rc_mat33f m)
{
    return (rc_mat44f) {
        rc_vec4f_from_vec3f(m.cx, 0.0f),
        rc_vec4f_from_vec3f(m.cy, 0.0f),
        rc_vec4f_from_vec3f(m.cz, 0.0f),
        rc_vec4f_make_unitw()
    };
}

/* Embed a 3D affine transform (last row is [0 0 0 1]). */
static inline rc_mat44f rc_mat44f_from_mat34f(rc_mat34f m)
{
    return (rc_mat44f) {
        rc_vec4f_from_vec3f(m.m.cx, 0.0f),
        rc_vec4f_from_vec3f(m.m.cy, 0.0f),
        rc_vec4f_from_vec3f(m.m.cz, 0.0f),
        rc_vec4f_from_vec3f(m.t,    1.0f)
    };
}

/* Construct from a pointer to sixteen consecutive floats (column-major). */
static inline rc_mat44f rc_mat44f_from_floats(const float *f)
{
    return (rc_mat44f) {
        rc_vec4f_from_floats(f),
        rc_vec4f_from_floats(f + 4),
        rc_vec4f_from_floats(f + 8),
        rc_vec4f_from_floats(f + 12)
    };
}

/* ---- conversion ---- */

/* Pointer to the first float element (column-major, float[16]). */
static inline const float *rc_mat44f_as_floats(const rc_mat44f *m)
{
    return &m->cx.x;
}

/* ---- operations ---- */

static inline rc_mat44f rc_mat44f_add(rc_mat44f a, rc_mat44f b)
{
    return (rc_mat44f) {
        rc_vec4f_add(a.cx, b.cx),
        rc_vec4f_add(a.cy, b.cy),
        rc_vec4f_add(a.cz, b.cz),
        rc_vec4f_add(a.cw, b.cw)
    };
}

static inline rc_mat44f rc_mat44f_sub(rc_mat44f a, rc_mat44f b)
{
    return (rc_mat44f) {
        rc_vec4f_sub(a.cx, b.cx),
        rc_vec4f_sub(a.cy, b.cy),
        rc_vec4f_sub(a.cz, b.cz),
        rc_vec4f_sub(a.cw, b.cw)
    };
}

static inline rc_mat44f rc_mat44f_scalar_mul(rc_mat44f m, float s)
{
    return (rc_mat44f) {
        rc_vec4f_scalar_mul(m.cx, s),
        rc_vec4f_scalar_mul(m.cy, s),
        rc_vec4f_scalar_mul(m.cz, s),
        rc_vec4f_scalar_mul(m.cw, s)
    };
}

/* Matrix-vector product: m * v. */
static inline rc_vec4f rc_mat44f_vec4f_mul(rc_mat44f m, rc_vec4f v)
{
    return rc_vec4f_add4(
        rc_vec4f_scalar_mul(m.cx, v.x),
        rc_vec4f_scalar_mul(m.cy, v.y),
        rc_vec4f_scalar_mul(m.cz, v.z),
        rc_vec4f_scalar_mul(m.cw, v.w)
    );
}

/* Matrix-matrix product: a * b. */
static inline rc_mat44f rc_mat44f_mul(rc_mat44f a, rc_mat44f b)
{
    return (rc_mat44f) {
        rc_mat44f_vec4f_mul(a, b.cx),
        rc_mat44f_vec4f_mul(a, b.cy),
        rc_mat44f_vec4f_mul(a, b.cz),
        rc_mat44f_vec4f_mul(a, b.cw)
    };
}

/* Transpose. */
static inline rc_mat44f rc_mat44f_transpose(rc_mat44f a)
{
    return (rc_mat44f) {
        {a.cx.x, a.cy.x, a.cz.x, a.cw.x},
        {a.cx.y, a.cy.y, a.cz.y, a.cw.y},
        {a.cx.z, a.cy.z, a.cz.z, a.cw.z},
        {a.cx.w, a.cy.w, a.cz.w, a.cw.w}
    };
}

/*
 * Determinant and inverse are non-trivial; implemented in src/math/mat44f.c.
 * rc_mat44f_inverse asserts that the determinant is non-zero.
 */
float      rc_mat44f_determinant(rc_mat44f m);
rc_mat44f  rc_mat44f_inverse(rc_mat44f m);

#endif /* RC_MATH_MAT44F_H_ */
