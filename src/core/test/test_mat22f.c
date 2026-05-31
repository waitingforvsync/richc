#include "richc/math/mat22f.h"
#include "richc/test.h"

#define RC_PI_OVER_2 1.5707963f
#define RC_PI_OVER_6 0.52359878f   // cos = sqrt(3)/2 = 0.8660254, sin = 0.5

RC_TEST(mat22f, construct)
{
    rc_mat22f m = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(m.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(m.cy, ==, rc_vec2f_make(3.0f, 4.0f));

    rc_mat22f z = rc_mat22f_make_zero();
    RC_CHECK(z.cx, ==, rc_vec2f_make_zero());
    RC_CHECK(z.cy, ==, rc_vec2f_make_zero());

    rc_mat22f id = rc_mat22f_make_identity();
    RC_CHECK(id.cx, ==, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(id.cy, ==, rc_vec2f_make(0.0f, 1.0f));

    // column-major: {cx.x, cx.y, cy.x, cy.y}
    float raw[] = {1.0f, 2.0f, 3.0f, 4.0f};
    rc_mat22f f = rc_mat22f_from_floats(raw);
    RC_CHECK(f.cx, ==, rc_vec2f_make(1.0f, 2.0f));
    RC_CHECK(f.cy, ==, rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK_TRUE(rc_mat22f_as_floats(&m) == &m.cx.x);
}

RC_TEST(mat22f, rotation)
{
    // a CCW quarter turn sends +x to +y and +y to -x
    rc_mat22f r = rc_mat22f_make_rotation(RC_PI_OVER_2);
    RC_CHECK(rc_mat22f_vec2f_mul(r, rc_vec2f_make(1.0f, 0.0f)), ~=, rc_vec2f_make(0.0f, 1.0f));
    RC_CHECK(rc_mat22f_vec2f_mul(r, rc_vec2f_make(0.0f, 1.0f)), ~=, rc_vec2f_make(-1.0f, 0.0f));

    // entries for a 30 degree rotation: cx = (cos, sin), cy = (-sin, cos)
    rc_mat22f r30 = rc_mat22f_make_rotation(RC_PI_OVER_6);
    RC_CHECK(r30.cx, ~=, rc_vec2f_make(0.8660254f, 0.5f));
    RC_CHECK(r30.cy, ~=, rc_vec2f_make(-0.5f, 0.8660254f));
    // a rotation preserves length
    RC_CHECK(rc_vec2f_length(rc_mat22f_vec2f_mul(r30, rc_vec2f_make(3.0f, 4.0f))), ~=, 5.0f);
}

RC_TEST(mat22f, arithmetic)
{
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f b = rc_mat22f_make(rc_vec2f_make(10.0f, 20.0f), rc_vec2f_make(30.0f, 40.0f));

    rc_mat22f s = rc_mat22f_add(a, b);
    RC_CHECK(s.cx, ==, rc_vec2f_make(11.0f, 22.0f));
    RC_CHECK(s.cy, ==, rc_vec2f_make(33.0f, 44.0f));

    rc_mat22f d = rc_mat22f_sub(b, a);
    RC_CHECK(d.cx, ==, rc_vec2f_make(9.0f, 18.0f));
    RC_CHECK(d.cy, ==, rc_vec2f_make(27.0f, 36.0f));

    rc_mat22f m = rc_mat22f_scalar_mul(a, 2.0f);
    RC_CHECK(m.cx, ==, rc_vec2f_make(2.0f, 4.0f));
    RC_CHECK(m.cy, ==, rc_vec2f_make(6.0f, 8.0f));
}

RC_TEST(mat22f, vec_mul)
{
    // diagonal scale matrix [[2,0],[0,3]] (columns (2,0) and (0,3))
    rc_mat22f s = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    RC_CHECK(rc_mat22f_vec2f_mul(s, rc_vec2f_make(1.0f, 1.0f)), ==, rc_vec2f_make(2.0f, 3.0f));
    RC_CHECK(rc_mat22f_vec2f_mul(s, rc_vec2f_make(5.0f, 7.0f)), ==, rc_vec2f_make(10.0f, 21.0f));
}

RC_TEST(mat22f, mul)
{
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    rc_mat22f b = rc_mat22f_make(rc_vec2f_make(1.0f, 1.0f), rc_vec2f_make(1.0f, -1.0f));
    // a*b columns are a applied to each column of b
    rc_mat22f p = rc_mat22f_mul(a, b);
    RC_CHECK(p.cx, ==, rc_vec2f_make(2.0f, 3.0f));
    RC_CHECK(p.cy, ==, rc_vec2f_make(2.0f, -3.0f));

    // identity is the multiplicative unit
    rc_mat22f id = rc_mat22f_make_identity();
    rc_mat22f ai = rc_mat22f_mul(a, id);
    RC_CHECK(ai.cx, ==, a.cx);
    RC_CHECK(ai.cy, ==, a.cy);
}

RC_TEST(mat22f, determinant)
{
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    RC_CHECK(rc_mat22f_determinant(a), ~=, -2.0f);                    // 1*4 - 2*3
    RC_CHECK(rc_mat22f_determinant(rc_mat22f_make_identity()), ~=, 1.0f);
    RC_CHECK(rc_mat22f_determinant(rc_mat22f_make_rotation(RC_PI_OVER_6)), ~=, 1.0f);
}

RC_TEST(mat22f, transpose)
{
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f t = rc_mat22f_transpose(a);
    RC_CHECK(t.cx, ==, rc_vec2f_make(1.0f, 3.0f));
    RC_CHECK(t.cy, ==, rc_vec2f_make(2.0f, 4.0f));
}

RC_TEST(mat22f, inverse)
{
    // inverse of a diagonal scale is the reciprocal scale
    rc_mat22f s = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 4.0f));
    rc_mat22f si = rc_mat22f_inverse(s);
    RC_CHECK(si.cx, ~=, rc_vec2f_make(0.5f, 0.0f));
    RC_CHECK(si.cy, ~=, rc_vec2f_make(0.0f, 0.25f));

    // m * inverse(m) == identity for a general invertible matrix
    rc_mat22f a = rc_mat22f_make(rc_vec2f_make(1.0f, 2.0f), rc_vec2f_make(3.0f, 4.0f));
    rc_mat22f p = rc_mat22f_mul(a, rc_mat22f_inverse(a));
    RC_CHECK(p.cx, ~=, rc_vec2f_make(1.0f, 0.0f));
    RC_CHECK(p.cy, ~=, rc_vec2f_make(0.0f, 1.0f));
}
