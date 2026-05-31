/*
 * math/quatf.h - quaternion (rc_quatf).
 *
 * Represents a rotation as a quaternion q = (xyz, w) where xyz is the vector
 * part and w is the scalar part.  The rotation-building constructors produce
 * unit quaternions; the component arithmetic (add, scalar_mul, lerp, ...) does
 * not preserve unit length, so normalise when a unit result is required.
 *
 * Convention
 * ----------
 *   q = w + x*i + y*j + z*k   (Hamilton convention)
 *
 * Identity: (xyz = 0, w = 1).
 *
 * Type
 * ----
 *   rc_quatf  { rc_vec3f xyz; float w; }
 *
 * Construction
 * ------------
 *   rc_quatf_make(x,y,z,w)           - from components
 *   rc_quatf_make_identity()         - (0,0,0,1)
 *   rc_quatf_make_angle_axis(a,axis) - rotation by angle a about axis (normalised)
 *   rc_quatf_from_floats(p)          - from float[4]: (x,y,z,w)
 *   rc_quatf_from_vec3f(xyz, w)      - from vector and scalar parts
 *   rc_quatf_from_mat33f(m)          - extract rotation from an orthonormal mat33f
 *
 * Conversion
 * ----------
 *   rc_quatf_as_floats(q)            - pointer to first float (float[4]: x,y,z,w)
 *   rc_mat33f_from_quatf(q)          - rotation matrix from a unit quaternion
 *
 * Operations
 * ----------
 *   add, sub, scalar_mul, negate, dot, lengthsqr, length, normalize,
 *   conjugate (= inverse for unit q), inverse, mul (compose rotations),
 *   vec3f_transform (rotate a vector), angle, axis,
 *   lerp, nlerp, slerp, exp, log, pow, is_equal, is_nearly_equal
 */

#ifndef RC_MATH_QUATF_H_
#define RC_MATH_QUATF_H_

#include "richc/math/mat33f.h"

/* ---- type ---- */

typedef struct rc_quatf {
    rc_vec3f xyz;
    float    w;
} rc_quatf;

/* ---- construction ---- */

static inline rc_quatf rc_quatf_make(float x, float y, float z, float w)
{
    return (rc_quatf) {rc_vec3f_make(x, y, z), w};
}

/* Identity rotation. */
static inline rc_quatf rc_quatf_make_identity(void)
{
    return (rc_quatf) {rc_vec3f_make_zero(), 1.0f};
}

/*
 * Rotation of `angle` radians about `axis` (axis is normalised internally).
 * q = (sin(angle/2) * axis, cos(angle/2)).  Implemented in src/math/quatf.c.
 */
rc_quatf rc_quatf_make_angle_axis(float angle, rc_vec3f axis);

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
 * Extract the unit quaternion from an orthonormal 3x3 rotation matrix.
 * Non-inline (a small branch table); implemented in src/math/quatf.c.
 */
rc_quatf rc_quatf_from_mat33f(rc_mat33f m);

/* ---- conversion ---- */

/* Pointer to the first float element (x, y, z, w). */
static inline const float *rc_quatf_as_floats(const rc_quatf *q)
{
    return &q->xyz.x;
}

/*
 * 3x3 rotation matrix from a unit quaternion.
 *
 * The standard expansion of q * [v,0] * q^-1, where [v,0] is v extended as a
 * pure quaternion.  Equivalent to transforming each basis vector by q.
 * Implemented in src/math/quatf.c.
 */
rc_mat33f rc_mat33f_from_quatf(rc_quatf a);

/* ---- component arithmetic ---- */

/* Treat the quaternion as a 4-vector; these do not preserve unit length. */

static inline rc_quatf rc_quatf_add(rc_quatf a, rc_quatf b)
{
    return (rc_quatf) {rc_vec3f_add(a.xyz, b.xyz), a.w + b.w};
}

static inline rc_quatf rc_quatf_sub(rc_quatf a, rc_quatf b)
{
    return (rc_quatf) {rc_vec3f_sub(a.xyz, b.xyz), a.w - b.w};
}

static inline rc_quatf rc_quatf_scalar_mul(rc_quatf a, float s)
{
    return (rc_quatf) {rc_vec3f_scalar_mul(a.xyz, s), a.w * s};
}

static inline rc_quatf rc_quatf_negate(rc_quatf a)
{
    return (rc_quatf) {rc_vec3f_negate(a.xyz), -a.w};
}

static inline float rc_quatf_dot(rc_quatf a, rc_quatf b)
{
    return rc_vec3f_dot(a.xyz, b.xyz) + a.w * b.w;
}

static inline float rc_quatf_lengthsqr(rc_quatf a)
{
    return rc_quatf_dot(a, a);
}

static inline float rc_quatf_length(rc_quatf a)
{
    return sqrtf(rc_quatf_lengthsqr(a));
}

/* Normalise to unit length.  Asserts that the quaternion is non-zero. */
static inline rc_quatf rc_quatf_normalize(rc_quatf a)
{
    float len = rc_quatf_length(a);
    RC_ASSERT(len != 0.0f);
    return rc_quatf_scalar_mul(a, 1.0f / len);
}

/* ---- rotation operations ---- */

/* Conjugate: negate the vector part.  For a unit quaternion this is the inverse. */
static inline rc_quatf rc_quatf_conjugate(rc_quatf a)
{
    return (rc_quatf) {rc_vec3f_negate(a.xyz), a.w};
}

/* General inverse: conjugate / |q|^2.  Asserts that the quaternion is non-zero. */
static inline rc_quatf rc_quatf_inverse(rc_quatf a)
{
    float ls = rc_quatf_lengthsqr(a);
    RC_ASSERT(ls != 0.0f);
    return rc_quatf_scalar_mul(rc_quatf_conjugate(a), 1.0f / ls);
}

/*
 * Quaternion product: a * b  (compose rotations: b applied first, then a).
 *
 * a * b = (a.w*b.xyz + b.w*a.xyz + a.xyz X b.xyz, a.w*b.w - a.xyz . b.xyz)
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
 *   t = 2 * (q.xyz X v)
 *   result = v + q.w * t + q.xyz X t
 *
 * Avoids building a rotation matrix; costs two cross products and a few adds.
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

/* Rotation angle in radians, in [0, 2*pi).  Robust via atan2 over the full range. */
static inline float rc_quatf_angle(rc_quatf q)
{
    return 2.0f * atan2f(rc_vec3f_length(q.xyz), q.w);
}

/* Normalised rotation axis.  Returns unit X when there is no rotation. */
static inline rc_vec3f rc_quatf_axis(rc_quatf q)
{
    float len = rc_vec3f_length(q.xyz);
    if (len < 1e-6f) {
        return rc_vec3f_make_unitx();
    }
    return rc_vec3f_scalar_mul(q.xyz, 1.0f / len);
}

/* ---- interpolation ---- */

/* Component-wise linear interpolation; the result is generally not unit. */
static inline rc_quatf rc_quatf_lerp(rc_quatf a, rc_quatf b, float t)
{
    return rc_quatf_add(a, rc_quatf_scalar_mul(rc_quatf_sub(b, a), t));
}

/* Normalised linear interpolation: cheap, but non-constant angular velocity. */
static inline rc_quatf rc_quatf_nlerp(rc_quatf a, rc_quatf b, float t)
{
    return rc_quatf_normalize(rc_quatf_lerp(a, b, t));
}

/*
 * Spherical linear interpolation along the shorter arc, constant angular
 * velocity.  Non-inline (acos/sin plus a fallback branch); in src/math/quatf.c.
 */
rc_quatf rc_quatf_slerp(rc_quatf a, rc_quatf b, float t);

/* ---- exponential maps ---- */

/*
 * Quaternion exponential, logarithm, and real power.  exp maps a pure-vector
 * quaternion to a unit rotation quaternion; log is its inverse; pow(q, t) raises
 * a unit quaternion to a real power (= exp(t * log(q))), the basis of slerp.
 * Non-inline (transcendental); implemented in src/math/quatf.c.
 */
rc_quatf rc_quatf_exp(rc_quatf q);
rc_quatf rc_quatf_log(rc_quatf q);
rc_quatf rc_quatf_pow(rc_quatf q, float t);

/* ---- comparison ---- */

static inline bool rc_quatf_is_equal(rc_quatf a, rc_quatf b)
{
    return rc_vec3f_is_equal(a.xyz, b.xyz) && a.w == b.w;
}

/* True if the squared 4-vector distance between a and b is less than tolerance^2. */
static inline bool rc_quatf_is_nearly_equal(rc_quatf a, rc_quatf b, float tolerance)
{
    return rc_quatf_lengthsqr(rc_quatf_sub(a, b)) < tolerance * tolerance;
}

#endif /* RC_MATH_QUATF_H_ */
