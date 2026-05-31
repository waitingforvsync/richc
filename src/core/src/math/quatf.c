#include "richc/math/quatf.h"

/*
 * Rotation of `angle` radians about `axis` (axis is normalised internally).
 * q = (sin(angle/2) * axis, cos(angle/2)).
 */
rc_quatf rc_quatf_make_angle_axis(float angle, rc_vec3f axis)
{
    float h = angle * 0.5f;
    return (rc_quatf) {
        rc_vec3f_scalar_mul(rc_vec3f_normalize(axis), sinf(h)),
        cosf(h)
    };
}

/*
 * 3x3 rotation matrix from a unit quaternion: the standard expansion of
 * q * [v,0] * q^-1, equivalent to transforming each basis vector by q.
 */
rc_mat33f rc_mat33f_from_quatf(rc_quatf a)
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

/*
 * Matrix to quaternion via Mike Day's branch method ("Converting a Rotation
 * Matrix to a Quaternion", Insomniac Games).
 *
 * The naive approach - recover each component as sqrt((1 +/- diagonal terms)/4)
 * and fix the signs with copysign from the off-diagonal differences - loses the
 * relative signs near a 180-degree rotation, where w (and hence the off-diagonal
 * differences that carry the signs) collapse to zero.
 *
 * Instead, pick the largest of the four components first: each of 4x*x, 4y*y,
 * 4z*z, 4w*w equals one of (1 +- m00 +- m11 +- m22), and the branch selects the
 * largest, which is always >= 1 so its sqrt is well-conditioned.  The remaining
 * three components come from off-diagonal sums/differences divided by 4 times
 * the chosen one.  One sqrt, one divide, stable across the whole range.
 *
 * Storage is column-major, so m.cx.x = R00, m.cx.y = R10, m.cy.x = R01, etc.
 */
rc_quatf rc_quatf_from_mat33f(rc_mat33f m)
{
    float x, y, z, w, t;
    if (m.cz.z < 0.0f) {
        if (m.cx.x > m.cy.y) {
            // |x| is largest
            t = 1.0f + m.cx.x - m.cy.y - m.cz.z;
            x = t;
            y = m.cx.y + m.cy.x;
            z = m.cx.z + m.cz.x;
            w = m.cy.z - m.cz.y;
        } else {
            // |y| is largest
            t = 1.0f - m.cx.x + m.cy.y - m.cz.z;
            x = m.cx.y + m.cy.x;
            y = t;
            z = m.cy.z + m.cz.y;
            w = m.cz.x - m.cx.z;
        }
    } else {
        if (m.cx.x < -m.cy.y) {
            // |z| is largest
            t = 1.0f - m.cx.x - m.cy.y + m.cz.z;
            x = m.cx.z + m.cz.x;
            y = m.cy.z + m.cz.y;
            z = t;
            w = m.cx.y - m.cy.x;
        } else {
            // |w| is largest
            t = 1.0f + m.cx.x + m.cy.y + m.cz.z;
            x = m.cy.z - m.cz.y;
            y = m.cz.x - m.cx.z;
            z = m.cx.y - m.cy.x;
            w = t;
        }
    }
    float s = 0.5f / sqrtf(t);
    return (rc_quatf) {rc_vec3f_make(x * s, y * s, z * s), w * s};
}

/*
 * Spherical linear interpolation along the shorter arc.
 *
 * Negate b when the quaternions point into opposite hemispheres so the
 * interpolation takes the short way round.  When the endpoints are nearly
 * parallel the sin(theta) denominator vanishes, so fall back to normalised
 * linear interpolation, which agrees with slerp to first order there.
 */
rc_quatf rc_quatf_slerp(rc_quatf a, rc_quatf b, float t)
{
    float d = rc_quatf_dot(a, b);
    if (d < 0.0f) {
        b = rc_quatf_negate(b);
        d = -d;
    }
    if (d > 0.9995f) {
        return rc_quatf_nlerp(a, b, t);
    }
    float theta = acosf(d);
    float inv_sin = 1.0f / sinf(theta);
    float wa = sinf((1.0f - t) * theta) * inv_sin;
    float wb = sinf(t * theta) * inv_sin;
    return rc_quatf_add(rc_quatf_scalar_mul(a, wa), rc_quatf_scalar_mul(b, wb));
}

/*
 * Quaternion exponential.  For q = (v, w):
 *   exp(q) = e^w * (sin|v| * v/|v|, cos|v|)
 * As |v| -> 0 the vector scale sin|v|/|v| -> 1, handled directly to avoid 0/0.
 */
rc_quatf rc_quatf_exp(rc_quatf q)
{
    float vlen = rc_vec3f_length(q.xyz);
    float ew = expf(q.w);
    float scale = (vlen < 1e-6f) ? ew : ew * sinf(vlen) / vlen;
    return (rc_quatf) {rc_vec3f_scalar_mul(q.xyz, scale), ew * cosf(vlen)};
}

/*
 * Quaternion logarithm.  For q = (v, w) with norm |q|:
 *   log(q) = (acos(w/|q|) * v/|v|, ln|q|)
 * A pure-real quaternion (|v| -> 0) has a zero vector part.  Asserts |q| != 0.
 */
rc_quatf rc_quatf_log(rc_quatf q)
{
    float qlen = rc_quatf_length(q);
    RC_ASSERT(qlen != 0.0f);
    float vlen = rc_vec3f_length(q.xyz);
    if (vlen < 1e-6f) {
        return (rc_quatf) {rc_vec3f_make_zero(), logf(qlen)};
    }
    float c = q.w / qlen;
    c = fmaxf(-1.0f, fminf(1.0f, c));   // guard acos domain against rounding
    float scale = acosf(c) / vlen;
    return (rc_quatf) {rc_vec3f_scalar_mul(q.xyz, scale), logf(qlen)};
}

/* Real power of a quaternion: q^t = exp(t * log(q)). */
rc_quatf rc_quatf_pow(rc_quatf q, float t)
{
    return rc_quatf_exp(rc_quatf_scalar_mul(rc_quatf_log(q), t));
}
