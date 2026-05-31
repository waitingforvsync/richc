#include "richc/math/mat44f.h"
#include "richc/test.h"

#define RC_PI_OVER_2 1.5707963f

RC_TEST(mat44f, construct)
{
    rc_mat44f id = rc_mat44f_make_identity();
    RC_CHECK(id.cx, ==, rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f));
    RC_CHECK(id.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));

    rc_mat44f z = rc_mat44f_make_zero();
    RC_CHECK(z.cx, ==, rc_vec4f_make_zero());

    // make_transpose takes rows; stored columns are the transpose
    rc_mat44f t = rc_mat44f_make_transpose(rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f),
                                           rc_vec4f_make(5.0f, 6.0f, 7.0f, 8.0f),
                                           rc_vec4f_make(9.0f, 10.0f, 11.0f, 12.0f),
                                           rc_vec4f_make(13.0f, 14.0f, 15.0f, 16.0f));
    RC_CHECK(t.cx, ==, rc_vec4f_make(1.0f, 5.0f, 9.0f, 13.0f));
    RC_CHECK(t.cw, ==, rc_vec4f_make(4.0f, 8.0f, 12.0f, 16.0f));

    float raw[] = {1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
    rc_mat44f f = rc_mat44f_from_floats(raw);
    RC_CHECK(f.cx, ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 4.0f));
    RC_CHECK(f.cz, ==, rc_vec4f_make(9.0f, 10.0f, 11.0f, 12.0f));
    RC_CHECK_TRUE(rc_mat44f_as_floats(&id) == &id.cx.x);
}

RC_TEST(mat44f, embed)
{
    rc_mat22f m2 = rc_mat22f_make(rc_vec2f_make(2.0f, 0.0f), rc_vec2f_make(0.0f, 3.0f));
    rc_mat44f e2 = rc_mat44f_from_mat22f(m2);
    RC_CHECK(e2.cx, ==, rc_vec4f_make(2.0f, 0.0f, 0.0f, 0.0f));
    RC_CHECK(e2.cy, ==, rc_vec4f_make(0.0f, 3.0f, 0.0f, 0.0f));
    RC_CHECK(e2.cz, ==, rc_vec4f_make(0.0f, 0.0f, 1.0f, 0.0f));
    RC_CHECK(e2.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));

    rc_mat33f m3 = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f),
                                  rc_vec3f_make(0.0f, 3.0f, 0.0f),
                                  rc_vec3f_make(0.0f, 0.0f, 4.0f));
    rc_mat44f e3 = rc_mat44f_from_mat33f(m3);
    RC_CHECK(e3.cz, ==, rc_vec4f_make(0.0f, 0.0f, 4.0f, 0.0f));
    RC_CHECK(e3.cw, ==, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));

    rc_mat34f m34 = rc_mat34f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    rc_mat44f e34 = rc_mat44f_from_mat34f(m34);
    RC_CHECK(e34.cx, ==, rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f));
    RC_CHECK(e34.cw, ==, rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f));   // last row stays [0 0 0 1]
}

RC_TEST(mat44f, vec_mul_and_mul)
{
    rc_mat44f s = rc_mat44f_make(rc_vec4f_make(2.0f, 0.0f, 0.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 3.0f, 0.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 0.0f, 4.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 0.0f, 0.0f, 5.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(s, rc_vec4f_make(1.0f, 1.0f, 1.0f, 1.0f)), ==, rc_vec4f_make(2.0f, 3.0f, 4.0f, 5.0f));

    // identity is the multiplicative unit
    rc_mat44f p = rc_mat44f_mul(s, rc_mat44f_make_identity());
    RC_CHECK(p.cx, ==, s.cx);
    RC_CHECK(p.cw, ==, s.cw);
}

RC_TEST(mat44f, translation)
{
    rc_mat44f t = rc_mat44f_make_translation(rc_vec3f_make(5.0f, 6.0f, 7.0f));
    // a homogeneous point (w = 1) is translated
    RC_CHECK(rc_mat44f_vec4f_mul(t, rc_vec4f_make(1.0f, 2.0f, 3.0f, 1.0f)), ==, rc_vec4f_make(6.0f, 8.0f, 10.0f, 1.0f));
    // a direction (w = 0) is unaffected
    RC_CHECK(rc_mat44f_vec4f_mul(t, rc_vec4f_make(1.0f, 2.0f, 3.0f, 0.0f)), ==, rc_vec4f_make(1.0f, 2.0f, 3.0f, 0.0f));
}

RC_TEST(mat44f, transpose)
{
    rc_mat44f a = rc_mat44f_from_floats((float[]){1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16});
    rc_mat44f t = rc_mat44f_transpose(a);
    RC_CHECK(t.cx, ==, rc_vec4f_make(1.0f, 5.0f, 9.0f, 13.0f));
    RC_CHECK(t.cw, ==, rc_vec4f_make(4.0f, 8.0f, 12.0f, 16.0f));
}

RC_TEST(mat44f, ortho)
{
    // left, right, top, bottom, near, far
    rc_mat44f o = rc_mat44f_make_ortho(0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 5.0f);
    // (left, bottom, -near) maps to the NDC corner (-1, -1, -1)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f)), ~=, rc_vec4f_make(-1.0f, -1.0f, -1.0f, 1.0f));
    // (right, top, -far) maps to the opposite corner (1, 1, 1)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(4.0f, 2.0f, -5.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, 1.0f, 1.0f, 1.0f));
    // the centre of the volume maps to the origin
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(2.0f, 1.0f, -3.0f, 1.0f)), ~=, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f));
}

RC_TEST(mat44f, perspective)
{
    // 90 degree vertical fov, square aspect, near 1, far 5
    rc_mat44f p = rc_mat44f_make_perspective(RC_PI_OVER_2, 1.0f, 1.0f, 5.0f);
    // near-plane centre: clip (0,0,-1,1) -> z/w = -1
    RC_CHECK(rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f)), ~=, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f));
    // far-plane centre: clip (0,0,5,5) -> z/w = 1
    RC_CHECK(rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.0f, 0.0f, -5.0f, 1.0f)), ~=, rc_vec4f_make(0.0f, 0.0f, 5.0f, 5.0f));
    // a point at the near-plane top edge: x/w stays 0, clip x scales by a/aspect = 1
    RC_CHECK(rc_mat44f_vec4f_mul(p, rc_vec4f_make(1.0f, 0.0f, -1.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, 0.0f, -1.0f, 1.0f));
}

RC_TEST(mat44f, determinant)
{
    RC_CHECK(rc_mat44f_determinant(rc_mat44f_make_identity()), ~=, 1.0f);
    RC_CHECK(rc_mat44f_determinant(rc_mat44f_make_translation(rc_vec3f_make(3.0f, 4.0f, 5.0f))), ~=, 1.0f);

    // affine scale(1,2,3) plus translation: det is the product of the diagonal = 6
    rc_mat44f a = rc_mat44f_make(rc_vec4f_make(1.0f, 0.0f, 0.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 2.0f, 0.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 0.0f, 3.0f, 0.0f),
                                 rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f));
    RC_CHECK(rc_mat44f_determinant(a), ~=, 6.0f);

    // a general matrix (value confirmed against numpy)
    rc_mat44f g = rc_mat44f_make(rc_vec4f_make(2.0f, 1.0f, 0.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 3.0f, 1.0f, 0.0f),
                                 rc_vec4f_make(1.0f, 0.0f, 4.0f, 0.0f),
                                 rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f));
    RC_CHECK(rc_mat44f_determinant(g), ~=, 25.0f);
}

RC_TEST(mat44f, inverse)
{
    rc_mat44f g = rc_mat44f_make(rc_vec4f_make(2.0f, 1.0f, 0.0f, 0.0f),
                                 rc_vec4f_make(0.0f, 3.0f, 1.0f, 0.0f),
                                 rc_vec4f_make(1.0f, 0.0f, 4.0f, 0.0f),
                                 rc_vec4f_make(5.0f, 6.0f, 7.0f, 1.0f));
    rc_mat44f p = rc_mat44f_mul(g, rc_mat44f_inverse(g));
    RC_CHECK(p.cx, ~=, rc_vec4f_make_unitx());
    RC_CHECK(p.cy, ~=, rc_vec4f_make_unity());
    RC_CHECK(p.cz, ~=, rc_vec4f_make_unitz());
    RC_CHECK(p.cw, ~=, rc_vec4f_make_unitw());
}
