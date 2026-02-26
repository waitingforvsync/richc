/*
 * math/quatf.h - unit quaternion (rc_quatf).
 *
 * Represents a rotation as a unit quaternion q = (xyz, w) where xyz is the
 * vector part and w is the scalar part.  All construction functions produce
 * unit quaternions; arithmetic results are only unit if the inputs are.
 *
 * Convention
 * ----------
 *   q = w + xi + yj + zk   (Hamilton convention)
 *
 * Identity: (xyz = 0, w = 1).
 *
 * Type
 * ----
 *   rc_quatf  { rc_vec3f xyz; float w; }
 *
 * Construction
 * ------------
 *   rc_quatf_make(x,y,z,w)           — from components
 *   rc_quatf_make_identity()         — (0,0,0,1)
 *   rc_quatf_make_angle_axis(a,axis) — rotation by angle a about axis (normalised)
 *   rc_quatf_from_floats(p)          — from float[4]: (x,y,z,w)
 *   rc_quatf_from_vec3f(xyz, w)      — from vector and scalar parts
 *   rc_quatf_from_mat33f(m)          — extract rotation from orthonormal mat33f
 *
 * Conversion
 * ----------
 *   rc_mat33f_from_quatf(q)          — rotation matrix from unit quaternion
 *
 * Operations
 * ----------
 *   rc_quatf_conjugate(q)            — conjugate (= inverse for unit q)
 *   rc_quatf_mul(a, b)               — quaternion product (compose rotations)
 *   rc_quatf_vec3f_transform(q, v)   — rotate v by q (Rodrigues formula, no mat)
 */

#ifndef RC_MATH_QUATF_H_
#define RC_MATH_QUATF_H_

#include "richc/math/mat33f.h"

/* ---- type ---- */

typedef struct {
    rc_vec3f xyz;
    float    w;
} rc_quatf;

/* ---- construction ---- */

static inline rc_quatf rc_quatf_make(float x, float y, float z, float w)
{
    return (rc_quatf) {rc_vec3f_make(x, y, z), w};
}

/* Identity rotation: no rotation. */
static inline rc_quatf rc_quatf_make_identity(void)
{
    return (rc_quatf) {rc_vec3f_make_zero(), 1.0f};
}

/*
 * Rotation of `angle` radians about `axis` (axis is normalised internally).
 * q = (sin(a/2)*axis, cos(a/2))
 */
static inline rc_quatf rc_quatf_make_angle_axis(float angle, rc_vec3f axis)
{
    return (rc_quatf) {
        rc_vec3f_scalar_mul(rc_vec3f_normalize(axis), sinf(angle / 2.0f)),
        cosf(angle / 2.0f)
    };
}

/* ---- construction from other types ---- */

/* Construct from a pointer to four consecutive floats: x, y, z, w. */
static inline rc_quatf rc_quatf_from_floats(const float *f)
{
    return (rc_quatf) {rc_vec3f_from_floats(f), f[3]};
}

static inline rc_quatf rc_quatf_from_vec3f(rc_vec3f xyz, float w)
{
    return (rc_quatf) {xyz, w};
}

/*
 * Extract the unit quaternion from an orthonormal 3×3 rotation matrix.
 *
 * Uses the Shepperd method via copysign + sqrt on each component, selecting
 * the sign of the imaginary parts from the off-diagonal elements.  The result
 * is numerically stable for any valid rotation matrix.
 */
static inline rc_quatf rc_quatf_from_mat33f(rc_mat33f m)
{
    return (rc_quatf) {
        {
            copysignf(sqrtf(fmaxf(0.0f, 1.0f + m.cx.x - m.cy.y - m.cz.z)) / 2.0f,
                      m.cy.z - m.cz.y),
            copysignf(sqrtf(fmaxf(0.0f, 1.0f - m.cx.x + m.cy.y - m.cz.z)) / 2.0f,
                      m.cz.x - m.cx.z),
            copysignf(sqrtf(fmaxf(0.0f, 1.0f - m.cx.x - m.cy.y + m.cz.z)) / 2.0f,
                      m.cx.y - m.cy.x)
        },
        sqrtf(fmaxf(0.0f, 1.0f + m.cx.x + m.cy.y + m.cz.z)) / 2.0f
    };
}

/* ---- conversion ---- */

/*
 * 3×3 rotation matrix from a unit quaternion.
 *
 * Uses the standard formula derived from expanding q * [v,0] * q^-1,
 * where [v,0] is v extended as a pure quaternion.  The result is equivalent
 * to mat33f_from_quatf applied column by column.
 */
static inline rc_mat33f rc_mat33f_from_quatf(rc_quatf a)
{
    float xx = a.xyz.x * a.xyz.x;
    float xy = a.xyz.x * a.xyz.y;
    float xz = a.xyz.x * a.xyz.z;
    float yy = a.xyz.y * a.xyz.y;
    float yz = a.xyz.y * a.xyz.z;
    float zz = a.xyz.z * a.xyz.z;
    float xw = a.xyz.x * a.w;
    float yw = a.xyz.y * a.w;
    float zw = a.xyz.z * a.w;
    float ww = a.w * a.w;
    return (rc_mat33f) {
        {ww + xx - yy - zz,  2.0f * (xy + zw),   2.0f * (xz - yw)},
        {2.0f * (xy - zw),   ww - xx + yy - zz,  2.0f * (yz + xw)},
        {2.0f * (xz + yw),   2.0f * (yz - xw),   ww - xx - yy + zz}
    };
}

/* ---- operations ---- */

/* Conjugate: negates the vector part.  For a unit quaternion this is the inverse. */
static inline rc_quatf rc_quatf_conjugate(rc_quatf a)
{
    return (rc_quatf) {rc_vec3f_negate(a.xyz), a.w};
}

/*
 * Quaternion product: a * b  (compose rotations: b applied first, then a).
 *
 * q1 * q2 = (w1*xyz2 + w2*xyz1 + xyz1 × xyz2, w1*w2 - xyz1·xyz2)
 */
static inline rc_quatf rc_quatf_mul(rc_quatf a, rc_quatf b)
{
    return (rc_quatf) {
        rc_vec3f_add3(
            rc_vec3f_scalar_mul(b.xyz, a.w),
            rc_vec3f_scalar_mul(a.xyz, b.w),
            rc_vec3f_cross(a.xyz, b.xyz)
        ),
        a.w * b.w - rc_vec3f_dot(a.xyz, b.xyz)
    };
}

/*
 * Rotate vector v by unit quaternion q using the Rodrigues formula:
 *
 *   t = 2 * (q.xyz × v)
 *   result = v + q.w * t + q.xyz × t
 *
 * Avoids constructing a rotation matrix; costs 2 cross products and a few adds.
 */
static inline rc_vec3f rc_quatf_vec3f_transform(rc_quatf q, rc_vec3f v)
{
    rc_vec3f t = rc_vec3f_scalar_mul(rc_vec3f_cross(q.xyz, v), 2.0f);
    return rc_vec3f_add3(
        v,
        rc_vec3f_scalar_mul(t, q.w),
        rc_vec3f_cross(q.xyz, t)
    );
}

#endif /* RC_MATH_QUATF_H_ */
