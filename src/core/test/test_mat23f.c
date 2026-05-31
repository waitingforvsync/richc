#include "richc/math/mat23f.h"
#include "richc/test.h"

#define RC_PI_OVER_2 1.5707963f

RC_TEST(mat23f, construct)
{
    rc_mat22f rot = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat23f m = rc_mat23f_make(rot, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK(m.rot.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(m.rot.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(m.trans, ==, rc_vec2f_make(5.0f, 6.0f));

    rc_mat23f id = rc_mat23f_make_identity();
    RC_CHECK(id.rot.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(id.rot.cy, ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(id.trans, ==, rc_vec2f_make_zero());

    rc_mat23f tr = rc_mat23f_make_translation(rc_vec2f_make(7.0f, 8.0f));
    RC_CHECK(tr.rot.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(tr.rot.cy, ==, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(tr.trans, ==, rc_vec2f_make(7.0f, 8.0f));

    rc_mat23f em = rc_mat23f_from_mat22f(rot);
    RC_CHECK(em.rot.cx, ==, rot.cx);
    RC_CHECK(em.trans, ==, rc_vec2f_make_zero());

    // column-major: rot.cx, rot.cy, trans
    float raw[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    rc_mat23f f = rc_mat23f_from_floats(raw);
    RC_CHECK(f.rot.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(f.rot.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(f.trans, ==, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK_TRUE(rc_mat23f_as_floats(&m) == &m.rot.cx.x);
}

RC_TEST(mat23f, vec_mul)
{
    // pure translation moves a point
    rc_mat23f t = rc_mat23f_make_translation(rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(t, rc_vec2f_make(1.0f, 2.0f)), ==, rc_vec2f_make(6.0f, 8.0f));

    // rotate by a quarter turn, then translate: (1,0) -> (0,1) -> (1,1)
    rc_mat23f m = rc_mat23f_make(rc_mat22f_make_rotation(RC_PI_OVER_2), rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(m, rc_vec2f_make(1.0f, 0.0f)), ~=, rc_vec2f_make(1.0f, 1.0f));
}

RC_TEST(mat23f, compose)
{
    // (a*b) applied to v must equal a applied to (b applied to v)
    rc_mat23f a = rc_mat23f_make_translation(rc_vec2f_make(10.0f, 0.0f));
    rc_mat23f b = rc_mat23f_make(rc_mat22f_make_rotation(RC_PI_OVER_2), rc_vec2f_make_zero());
    rc_mat23f ab = rc_mat23f_mul(a, b);

    rc_vec2f v = rc_vec2f_make(1.0f, 0.0f);
    rc_vec2f viaboth = rc_mat23f_vec2f_mul(a, rc_mat23f_vec2f_mul(b, v));
    RC_CHECK(rc_mat23f_vec2f_mul(ab, v), ~=, viaboth);
    RC_CHECK(rc_mat23f_vec2f_mul(ab, v), ~=, rc_vec2f_make(10.0f, 1.0f));
}

RC_TEST(mat23f, mixed_mul)
{
    // left-multiply by a linear map: result(v) = L * (b(v))
    rc_mat22f L = rc_mat22f_make_rotation(RC_PI_OVER_2);
    rc_mat23f b = rc_mat23f_make_translation(rc_vec2f_make(3.0f, 4.0f));
    rc_mat23f lb = rc_mat22f_mat23f_mul(L, b);
    RC_CHECK(rc_mat23f_vec2f_mul(lb, rc_vec2f_make(1.0f, 0.0f)), ~=, rc_vec2f_make(-4.0f, 4.0f));

    // right-multiply by a linear map: translation is unchanged
    rc_mat23f a = rc_mat23f_make_translation(rc_vec2f_make(5.0f, 6.0f));
    rc_mat22f R = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    rc_mat23f ar = rc_mat23f_mat22f_mul(a, R);
    RC_CHECK(ar.trans, ==, rc_vec2f_make(5.0f, 6.0f));
    RC_CHECK(rc_mat23f_vec2f_mul(ar, rc_vec2f_make(1.0f, 1.0f)), ~=, rc_vec2f_make(7.0f, 9.0f));
}

RC_TEST(mat23f, inverse)
{
    // m composed with its inverse is the identity transform
    rc_mat23f m = rc_mat23f_make(rc_mat22f_make_rotation(RC_PI_OVER_2), rc_vec2f_make(2.0f, 3.0f));
    rc_mat23f p = rc_mat23f_mul(m, rc_mat23f_inverse(m));
    RC_CHECK(p.rot.cx, ~=, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(p.rot.cy, ~=, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(p.trans, ~=, rc_vec2f_make_zero());

    // and inverse undoes the transform of a point
    rc_vec2f v = rc_vec2f_make(4.0f, -1.0f);
    RC_CHECK(rc_mat23f_vec2f_mul(rc_mat23f_inverse(m), rc_mat23f_vec2f_mul(m, v)), ~=, v);
}
