#include "richc/math/quatf.h"
#include "richc/test.h"

#define RC_PI        3.1415927f
#define RC_PI_OVER_2 1.5707963f
#define RC_PI_OVER_4 0.7853982f
#define RC_TWO_PI_3  2.0943951f    // 120 degrees
#define RC_SQRT_HALF 0.70710678f   // sin(45) = cos(45)
#define RC_SIN_22_5  0.38268343f
#define RC_COS_22_5  0.92387953f

RC_TEST(quatf, construct)
{
    rc_quatf q = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    RC_CHECK(q.xyz, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(q.w, ==, 4.0f);

    rc_quatf id = rc_quatf_make_identity();
    RC_CHECK(id.xyz, ==, rc_vec3f_make_zero());
    RC_CHECK(id.w, ==, 1.0f);

    float raw[] = {1.0f, 2.0f, 3.0f, 4.0f};
    rc_quatf f = rc_quatf_from_floats(raw);
    RC_CHECK(f.xyz, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(f.w, ==, 4.0f);

    rc_quatf v = rc_quatf_from_vec3f(rc_vec3f_make(5.0f, 6.0f, 7.0f), 8.0f);
    RC_CHECK(v.xyz, ==, rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(v.w, ==, 8.0f);
    RC_CHECK_TRUE(rc_quatf_as_floats(&q) == &q.xyz.x);
}

RC_TEST(quatf, angle_axis)
{
    // 90 degrees about +z -> (0, 0, sin45, cos45)
    rc_quatf q = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());
    RC_CHECK(q.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_SQRT_HALF));
    RC_CHECK(q.w, ~=, RC_SQRT_HALF);
    RC_CHECK(rc_quatf_length(q), ~=, 1.0f);

    // 120 degrees about (1,1,1) -> all components 0.5
    rc_quatf q2 = rc_quatf_make_angle_axis(RC_TWO_PI_3, rc_vec3f_make(1.0f, 1.0f, 1.0f));
    RC_CHECK(q2.xyz, ~=, rc_vec3f_make(0.5f, 0.5f, 0.5f));
    RC_CHECK(q2.w, ~=, 0.5f);

    // the axis is normalised internally, so its magnitude does not matter
    rc_quatf q3 = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make(0.0f, 0.0f, 5.0f));
    RC_CHECK(q3.xyz, ~=, q.xyz);
    RC_CHECK(q3.w, ~=, q.w);
}

RC_TEST(quatf, angle_and_axis)
{
    rc_quatf q = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());
    RC_CHECK(rc_quatf_angle(q), ~=, RC_PI_OVER_2);
    RC_CHECK(rc_quatf_axis(q), ~=, rc_vec3f_make_unitz());

    // the identity has zero angle and a defined (fallback) axis
    RC_CHECK(rc_quatf_angle(rc_quatf_make_identity()), ~=, 0.0f);
    RC_CHECK(rc_quatf_axis(rc_quatf_make_identity()), ~=, rc_vec3f_make_unitx());
}

RC_TEST(quatf, arithmetic)
{
    rc_quatf a = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    rc_quatf b = rc_quatf_make(10.0f, 20.0f, 30.0f, 40.0f);

    rc_quatf s = rc_quatf_add(a, b);
    RC_CHECK(s.xyz, ==, rc_vec3f_make(11.0f, 22.0f, 33.0f));
    RC_CHECK(s.w, ==, 44.0f);

    rc_quatf d = rc_quatf_sub(b, a);
    RC_CHECK(d.xyz, ==, rc_vec3f_make(9.0f, 18.0f, 27.0f));
    RC_CHECK(d.w, ==, 36.0f);

    rc_quatf m = rc_quatf_scalar_mul(a, 2.0f);
    RC_CHECK(m.xyz, ==, rc_vec3f_make(2.0f, 4.0f, 6.0f));
    RC_CHECK(m.w, ==, 8.0f);

    rc_quatf n = rc_quatf_negate(a);
    RC_CHECK(n.xyz, ==, rc_vec3f_make(-1.0f, -2.0f, -3.0f));
    RC_CHECK(n.w, ==, -4.0f);

    RC_CHECK(rc_quatf_dot(a, b), ~=, 300.0f);          // 10+40+90+160
    RC_CHECK(rc_quatf_lengthsqr(a), ~=, 30.0f);        // 1+4+9+16
    RC_CHECK(rc_quatf_length(a), ~=, 5.4772256f);
    RC_CHECK(rc_quatf_length(rc_quatf_normalize(a)), ~=, 1.0f);
}

RC_TEST(quatf, conjugate_inverse)
{
    rc_quatf a = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    RC_CHECK(rc_quatf_conjugate(a).xyz, ==, rc_vec3f_make(-1.0f, -2.0f, -3.0f));
    RC_CHECK(rc_quatf_conjugate(a).w, ==, 4.0f);

    // q * inverse(q) == identity, even for a non-unit quaternion
    rc_quatf p = rc_quatf_mul(a, rc_quatf_inverse(a));
    RC_CHECK(p.xyz, ~=, rc_vec3f_make_zero());
    RC_CHECK(p.w, ~=, 1.0f);

    // for a unit quaternion the inverse is the conjugate
    rc_quatf u = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());
    RC_CHECK(rc_quatf_inverse(u).xyz, ~=, rc_quatf_conjugate(u).xyz);
    RC_CHECK(rc_quatf_inverse(u).w, ~=, rc_quatf_conjugate(u).w);
}

RC_TEST(quatf, mul)
{
    rc_quatf q = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());

    // identity is the multiplicative unit
    rc_quatf iq = rc_quatf_mul(rc_quatf_make_identity(), q);
    RC_CHECK(iq.xyz, ~=, q.xyz);
    RC_CHECK(iq.w, ~=, q.w);

    // q * conjugate(q) == identity for a unit quaternion
    rc_quatf p = rc_quatf_mul(q, rc_quatf_conjugate(q));
    RC_CHECK(p.xyz, ~=, rc_vec3f_make_zero());
    RC_CHECK(p.w, ~=, 1.0f);

    // compose: rotate 90 about x first, then 90 about y -> (0.5, 0.5, -0.5, 0.5)
    rc_quatf qx = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitx());
    rc_quatf qy = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unity());
    rc_quatf comp = rc_quatf_mul(qy, qx);
    RC_CHECK(comp.xyz, ~=, rc_vec3f_make(0.5f, 0.5f, -0.5f));
    RC_CHECK(comp.w, ~=, 0.5f);

    // the composed rotation equals applying qx then qy to a vector
    rc_vec3f v = rc_vec3f_make(0.0f, 1.0f, 0.0f);
    rc_vec3f stepwise = rc_quatf_vec3f_transform(qy, rc_quatf_vec3f_transform(qx, v));
    RC_CHECK(rc_quatf_vec3f_transform(comp, v), ~=, stepwise);
}

RC_TEST(quatf, transform)
{
    rc_quatf q = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());
    // a quarter turn about z sends +x to +y
    RC_CHECK(rc_quatf_vec3f_transform(q, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    // the axis itself is fixed
    RC_CHECK(rc_quatf_vec3f_transform(q, rc_vec3f_make(0.0f, 0.0f, 1.0f)), ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    // identity leaves a vector unchanged
    RC_CHECK(rc_quatf_vec3f_transform(rc_quatf_make_identity(), rc_vec3f_make(3.0f, 4.0f, 5.0f)), ~=, rc_vec3f_make(3.0f, 4.0f, 5.0f));

    // rotation preserves length
    rc_quatf r = rc_quatf_make_angle_axis(RC_TWO_PI_3, rc_vec3f_make(1.0f, 1.0f, 1.0f));
    RC_CHECK(rc_vec3f_length(rc_quatf_vec3f_transform(r, rc_vec3f_make(3.0f, 4.0f, 0.0f))), ~=, 5.0f);
    // Rodrigues transform agrees with the rotation matrix
    rc_mat33f m = rc_mat33f_from_quatf(r);
    rc_vec3f v = rc_vec3f_make(2.0f, -1.0f, 3.0f);
    RC_CHECK(rc_quatf_vec3f_transform(r, v), ~=, rc_mat33f_vec3f_mul(m, v));
}

RC_TEST(quatf, to_matrix)
{
    // matrix of a 90 degree turn about z
    rc_quatf q = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());
    rc_mat33f m = rc_mat33f_from_quatf(q);
    RC_CHECK(m.cx, ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(m.cy, ~=, rc_vec3f_make(-1.0f, 0.0f, 0.0f));
    RC_CHECK(m.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));

    // identity quaternion -> identity matrix
    rc_mat33f mi = rc_mat33f_from_quatf(rc_quatf_make_identity());
    RC_CHECK(mi.cx, ~=, rc_vec3f_make_unitx());
    RC_CHECK(mi.cy, ~=, rc_vec3f_make_unity());
    RC_CHECK(mi.cz, ~=, rc_vec3f_make_unitz());

    // a general rotation (xyz euler 30,40,50; columns and quat from scipy)
    rc_quatf g = rc_quatf_make(0.0808047f, 0.4021985f, 0.3033718f, 0.8600422f);
    rc_mat33f gm = rc_mat33f_from_quatf(g);
    RC_CHECK(gm.cx, ~=, rc_vec3f_make(0.492404f, 0.586824f, -0.642788f));
    RC_CHECK(gm.cy, ~=, rc_vec3f_make(-0.456826f, 0.802872f, 0.383022f));
    RC_CHECK(gm.cz, ~=, rc_vec3f_make(0.740843f, 0.10504f, 0.663414f));
}

RC_TEST(quatf, from_matrix)
{
    // recover the quaternion of a 90 degree turn about z
    rc_mat33f m = rc_mat33f_make(rc_vec3f_make(0.0f, 1.0f, 0.0f),
                                 rc_vec3f_make(-1.0f, 0.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 0.0f, 1.0f));
    rc_quatf q = rc_quatf_from_mat33f(m);
    RC_CHECK(q.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_SQRT_HALF));
    RC_CHECK(q.w, ~=, RC_SQRT_HALF);

    // the hard case: a 180 degree turn about x, where w = 0 and the naive
    // copysign method loses the component signs
    rc_mat33f m180 = rc_mat33f_make(rc_vec3f_make(1.0f, 0.0f, 0.0f),
                                    rc_vec3f_make(0.0f, -1.0f, 0.0f),
                                    rc_vec3f_make(0.0f, 0.0f, -1.0f));
    rc_quatf q180 = rc_quatf_from_mat33f(m180);
    RC_CHECK(q180.xyz, ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(q180.w, ~=, 0.0f);

    // a general rotation matrix (columns from scipy) recovers its quaternion
    rc_mat33f gm = rc_mat33f_make(rc_vec3f_make(0.492404f, 0.586824f, -0.642788f),
                                  rc_vec3f_make(-0.456826f, 0.802872f, 0.383022f),
                                  rc_vec3f_make(0.740843f, 0.10504f, 0.663414f));
    rc_quatf g = rc_quatf_from_mat33f(gm);
    RC_CHECK(g.xyz, ~=, rc_vec3f_make(0.0808047f, 0.4021985f, 0.3033718f));
    RC_CHECK(g.w, ~=, 0.8600422f);

    // round trip matrix -> quaternion -> matrix
    rc_mat33f rt = rc_mat33f_from_quatf(rc_quatf_from_mat33f(gm));
    RC_CHECK(rt.cx, ~=, gm.cx);
    RC_CHECK(rt.cy, ~=, gm.cy);
    RC_CHECK(rt.cz, ~=, gm.cz);
}

RC_TEST(quatf, slerp)
{
    rc_quatf a = rc_quatf_make_identity();
    rc_quatf b = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());

    // endpoints
    RC_CHECK(rc_quatf_slerp(a, b, 0.0f).w, ~=, 1.0f);
    rc_quatf at1 = rc_quatf_slerp(a, b, 1.0f);
    RC_CHECK(at1.xyz, ~=, b.xyz);
    RC_CHECK(at1.w, ~=, b.w);

    // halfway between identity and a 90 degree turn is a 45 degree turn
    rc_quatf mid = rc_quatf_slerp(a, b, 0.5f);
    RC_CHECK(mid.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_SIN_22_5));
    RC_CHECK(mid.w, ~=, RC_COS_22_5);

    // slerp takes the shorter arc: negating an endpoint gives the same path
    rc_quatf midn = rc_quatf_slerp(a, rc_quatf_negate(b), 0.5f);
    RC_CHECK(midn.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_SIN_22_5));
    RC_CHECK(midn.w, ~=, RC_COS_22_5);

    // nlerp matches slerp at the symmetric midpoint
    rc_quatf nmid = rc_quatf_nlerp(a, b, 0.5f);
    RC_CHECK(nmid.xyz, ~=, mid.xyz);
    RC_CHECK(nmid.w, ~=, mid.w);
}

RC_TEST(quatf, exp_log_pow)
{
    rc_quatf q = rc_quatf_make_angle_axis(RC_PI_OVER_2, rc_vec3f_make_unitz());

    // log of a unit rotation quaternion: (axis * angle/2, 0)
    rc_quatf l = rc_quatf_log(q);
    RC_CHECK(l.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_PI_OVER_4));
    RC_CHECK(l.w, ~=, 0.0f);

    // exp of a pure quaternion is the rotation it generates
    rc_quatf e = rc_quatf_exp(rc_quatf_make(0.0f, 0.0f, RC_PI_OVER_4, 0.0f));
    RC_CHECK(e.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_SQRT_HALF));
    RC_CHECK(e.w, ~=, RC_SQRT_HALF);

    // exp and log are inverse on unit quaternions
    rc_quatf rt = rc_quatf_exp(rc_quatf_log(q));
    RC_CHECK(rt.xyz, ~=, q.xyz);
    RC_CHECK(rt.w, ~=, q.w);

    // a half power is a half rotation: q^0.5 is a 45 degree turn
    rc_quatf half = rc_quatf_pow(q, 0.5f);
    RC_CHECK(half.xyz, ~=, rc_vec3f_make(0.0f, 0.0f, RC_SIN_22_5));
    RC_CHECK(half.w, ~=, RC_COS_22_5);

    // squaring a rotation doubles its angle: q^2 == q * q
    rc_quatf sq = rc_quatf_pow(q, 2.0f);
    rc_quatf qq = rc_quatf_mul(q, q);
    RC_CHECK(sq.xyz, ~=, qq.xyz);
    RC_CHECK(sq.w, ~=, qq.w);
}

RC_TEST(quatf, equality)
{
    rc_quatf a = rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f);
    RC_CHECK_TRUE(rc_quatf_is_equal(a, rc_quatf_make(1.0f, 2.0f, 3.0f, 4.0f)));
    RC_CHECK_FALSE(rc_quatf_is_equal(a, rc_quatf_make(1.0f, 2.0f, 3.0f, 5.0f)));
    RC_CHECK_TRUE(rc_quatf_is_nearly_equal(a, rc_quatf_make(1.00001f, 2.0f, 3.0f, 4.0f), 0.001f));
    RC_CHECK_FALSE(rc_quatf_is_nearly_equal(a, rc_quatf_make(1.0f, 2.0f, 3.0f, 5.0f), 0.001f));
}
