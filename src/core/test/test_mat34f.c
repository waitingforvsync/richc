#include "richc/math/mat34f.h"
#include "richc/test.h"

#define RC_PI_OVER_2 1.5707963f

RC_TEST(mat34f, construct)
{
    rc_mat33f rot = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                   rc_vec3f_make(4.0f, 5.0f, 6.0f),
                                   rc_vec3f_make(7.0f, 8.0f, 9.0f));
    rc_mat34f m = rc_mat34f_make(rot, rc_vec3f_make(10.0f, 11.0f, 12.0f));
    RC_CHECK(m.rot.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(m.trans, ==, rc_vec3f_make(10.0f, 11.0f, 12.0f));

    rc_mat34f id = rc_mat34f_make_identity();
    RC_CHECK(id.rot.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(id.rot.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(id.trans, ==, rc_vec3f_make_zero());

    rc_mat34f tr = rc_mat34f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(tr.rot.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(tr.trans, ==, rc_vec3f_make(5.0f, 6.0f, 7.0f));

    rc_mat34f em = rc_mat34f_from_mat33f(rot);
    RC_CHECK(em.rot.cz, ==, rot.cz);
    RC_CHECK(em.trans, ==, rc_vec3f_make_zero());

    // column-major: rot.cx, rot.cy, rot.cz, trans
    float raw[] = {1,2,3, 4,5,6, 7,8,9, 10,11,12};
    rc_mat34f f = rc_mat34f_from_floats(raw);
    RC_CHECK(f.rot.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(f.trans, ==, rc_vec3f_make(10.0f, 11.0f, 12.0f));
    RC_CHECK_TRUE(rc_mat34f_as_floats(&m) == &m.rot.cx.x);
}

RC_TEST(mat34f, vec_mul)
{
    rc_mat34f t = rc_mat34f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(t, rc_vec3f_make(1.0f, 2.0f, 3.0f)), ==, rc_vec3f_make(6.0f, 8.0f, 10.0f));

    // rotate about Z by a quarter turn, then translate: (1,0,0) -> (0,1,0) -> (0,1,5)
    rc_mat34f m = rc_mat34f_make(rc_mat33f_make_rotation_z(RC_PI_OVER_2), rc_vec3f_make(0.0f, 0.0f, 5.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(m, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ~=, rc_vec3f_make(0.0f, 1.0f, 5.0f));
}

RC_TEST(mat34f, compose)
{
    rc_mat34f a = rc_mat34f_make_translation(rc_vec3f_make(10.0f, 0.0f, 0.0f));
    rc_mat34f b = rc_mat34f_make(rc_mat33f_make_rotation_z(RC_PI_OVER_2), rc_vec3f_make_zero());
    rc_mat34f ab = rc_mat34f_mul(a, b);

    rc_vec3f v = rc_vec3f_make(1.0f, 0.0f, 0.0f);
    rc_vec3f viaboth = rc_mat34f_vec3f_mul(a, rc_mat34f_vec3f_mul(b, v));
    RC_CHECK(rc_mat34f_vec3f_mul(ab, v), ~=, viaboth);
    RC_CHECK(rc_mat34f_vec3f_mul(ab, v), ~=, rc_vec3f_make(10.0f, 1.0f, 0.0f));
}

RC_TEST(mat34f, mixed_mul)
{
    // left-multiply by a linear map: result(v) = L * (b(v))
    rc_mat33f L = rc_mat33f_make_rotation_z(RC_PI_OVER_2);
    rc_mat34f b = rc_mat34f_make_translation(rc_vec3f_make(3.0f, 4.0f, 5.0f));
    rc_mat34f lb = rc_mat33f_mat34f_mul(L, b);
    RC_CHECK(rc_mat34f_vec3f_mul(lb, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ~=, rc_vec3f_make(-4.0f, 4.0f, 5.0f));

    // right-multiply by a linear map: translation unchanged
    rc_mat34f a = rc_mat34f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    rc_mat33f R = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 3.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 0.0f, 4.0f));
    rc_mat34f ar = rc_mat34f_mat33f_mul(a, R);
    RC_CHECK(ar.trans, ==, rc_vec3f_make(5.0f, 6.0f, 7.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(ar, rc_vec3f_make(1.0f, 1.0f, 1.0f)), ~=, rc_vec3f_make(7.0f, 9.0f, 11.0f));
}

RC_TEST(mat34f, lookat)
{
    // camera at (0,0,5) looking at the origin, +y up: the linear part is the
    // identity and the translation moves the eye to the origin
    rc_mat34f a = rc_mat34f_make_lookat(rc_vec3f_make(0.0f, 0.0f, 5.0f),
                                        rc_vec3f_make_zero(),
                                        rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(a.rot.cx, ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(a.rot.cy, ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(a.rot.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(a.trans, ~=, rc_vec3f_make(0.0f, 0.0f, -5.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(a, rc_vec3f_make(0.0f, 0.0f, 5.0f)), ~=, rc_vec3f_make_zero());        // eye -> origin
    RC_CHECK(rc_mat34f_vec3f_mul(a, rc_vec3f_make_zero()), ~=, rc_vec3f_make(0.0f, 0.0f, -5.0f));       // focus -> -z

    // camera at the origin looking down +x: the focus maps onto the -z axis
    rc_mat34f b = rc_mat34f_make_lookat(rc_vec3f_make_zero(),
                                        rc_vec3f_make(1.0f, 0.0f, 0.0f),
                                        rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(b, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ~=, rc_vec3f_make(0.0f, 0.0f, -1.0f));
    RC_CHECK(rc_mat34f_vec3f_mul(b, rc_vec3f_make(0.0f, 1.0f, 0.0f)), ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
}

RC_TEST(mat34f, inverse)
{
    rc_mat34f m = rc_mat34f_make(rc_mat33f_make_rotation_z(RC_PI_OVER_2), rc_vec3f_make(2.0f, 3.0f, 4.0f));
    rc_mat34f p = rc_mat34f_mul(m, rc_mat34f_inverse(m));
    RC_CHECK(p.rot.cx, ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(p.rot.cy, ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(p.rot.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
    RC_CHECK(p.trans, ~=, rc_vec3f_make_zero());

    rc_vec3f v = rc_vec3f_make(4.0f, -1.0f, 2.0f);
    RC_CHECK(rc_mat34f_vec3f_mul(rc_mat34f_inverse(m), rc_mat34f_vec3f_mul(m, v)), ~=, v);
}
