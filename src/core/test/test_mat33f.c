#include "richc/math/mat33f.h"
#include "richc/test.h"

#define RC_PI_OVER_2 1.5707963f
#define RC_PI_OVER_6 0.52359878f   // cos = 0.8660254, sin = 0.5

RC_TEST(mat33f, construct)
{
    rc_mat33f m = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                 rc_vec3f_make(4.0f, 5.0f, 6.0f),
                                 rc_vec3f_make(7.0f, 8.0f, 9.0f));
    RC_CHECK(m.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(m.cy, ==, rc_vec3f_make(4.0f, 5.0f, 6.0f));
    RC_CHECK(m.cz, ==, rc_vec3f_make(7.0f, 8.0f, 9.0f));

    // make_transpose takes rows; the stored columns are the transpose
    rc_mat33f t = rc_mat33f_make_transpose(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                           rc_vec3f_make(4.0f, 5.0f, 6.0f),
                                           rc_vec3f_make(7.0f, 8.0f, 9.0f));
    RC_CHECK(t.cx, ==, rc_vec3f_make(1.0f, 4.0f, 7.0f));
    RC_CHECK(t.cy, ==, rc_vec3f_make(2.0f, 5.0f, 8.0f));
    RC_CHECK(t.cz, ==, rc_vec3f_make(3.0f, 6.0f, 9.0f));

    rc_mat33f id = rc_mat33f_make_identity();
    RC_CHECK(id.cx, ==, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(id.cy, ==, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(id.cz, ==, rc_vec3f_make(0.0f, 0.0f, 1.0f));

    rc_mat33f z = rc_mat33f_make_zero();
    RC_CHECK(z.cx, ==, rc_vec3f_make_zero());

    float raw[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    rc_mat33f f = rc_mat33f_from_floats(raw);
    RC_CHECK(f.cx, ==, rc_vec3f_make(1.0f, 2.0f, 3.0f));
    RC_CHECK(f.cz, ==, rc_vec3f_make(7.0f, 8.0f, 9.0f));
    RC_CHECK_TRUE(rc_mat33f_as_floats(&m) == &m.cx.x);
}

RC_TEST(mat33f, rotations)
{
    // right-handed quarter turns about each axis
    rc_mat33f rx = rc_mat33f_make_rotation_x(RC_PI_OVER_2);
    RC_CHECK(rc_mat33f_vec3f_mul(rx, rc_vec3f_make(0.0f, 1.0f, 0.0f)), ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));

    rc_mat33f ry = rc_mat33f_make_rotation_y(RC_PI_OVER_2);
    RC_CHECK(rc_mat33f_vec3f_mul(ry, rc_vec3f_make(0.0f, 0.0f, 1.0f)), ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));

    rc_mat33f rz = rc_mat33f_make_rotation_z(RC_PI_OVER_2);
    RC_CHECK(rc_mat33f_vec3f_mul(rz, rc_vec3f_make(1.0f, 0.0f, 0.0f)), ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));

    // entries of a 30 degree rotation about Z
    rc_mat33f rz30 = rc_mat33f_make_rotation_z(RC_PI_OVER_6);
    RC_CHECK(rz30.cx, ~=, rc_vec3f_make(0.8660254f, 0.5f, 0.0f));
    RC_CHECK(rz30.cy, ~=, rc_vec3f_make(-0.5f, 0.8660254f, 0.0f));
    RC_CHECK(rz30.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
}

RC_TEST(mat33f, arithmetic)
{
    rc_mat33f a = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                 rc_vec3f_make(4.0f, 5.0f, 6.0f),
                                 rc_vec3f_make(7.0f, 8.0f, 9.0f));
    rc_mat33f b = rc_mat33f_scalar_mul(a, 10.0f);
    RC_CHECK(b.cx, ==, rc_vec3f_make(10.0f, 20.0f, 30.0f));

    rc_mat33f s = rc_mat33f_add(a, b);
    RC_CHECK(s.cx, ==, rc_vec3f_make(11.0f, 22.0f, 33.0f));
    rc_mat33f d = rc_mat33f_sub(b, a);
    RC_CHECK(d.cz, ==, rc_vec3f_make(63.0f, 72.0f, 81.0f));
}

RC_TEST(mat33f, vec_mul_and_mul)
{
    // diagonal scale
    rc_mat33f s = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 3.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 0.0f, 4.0f));
    RC_CHECK(rc_mat33f_vec3f_mul(s, rc_vec3f_make(1.0f, 1.0f, 1.0f)), ==, rc_vec3f_make(2.0f, 3.0f, 4.0f));

    // matrix product: s applied to each column of b
    rc_mat33f b = rc_mat33f_make(rc_vec3f_make(1.0f, 0.0f, 0.0f),
                                 rc_vec3f_make(1.0f, 1.0f, 0.0f),
                                 rc_vec3f_make(1.0f, 1.0f, 1.0f));
    rc_mat33f p = rc_mat33f_mul(s, b);
    RC_CHECK(p.cx, ==, rc_vec3f_make(2.0f, 0.0f, 0.0f));
    RC_CHECK(p.cy, ==, rc_vec3f_make(2.0f, 3.0f, 0.0f));
    RC_CHECK(p.cz, ==, rc_vec3f_make(2.0f, 3.0f, 4.0f));
}

RC_TEST(mat33f, determinant)
{
    RC_CHECK(rc_mat33f_determinant(rc_mat33f_make_identity()), ~=, 1.0f);
    // diagonal scale: product of the diagonal
    rc_mat33f s = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 3.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 0.0f, 4.0f));
    RC_CHECK(rc_mat33f_determinant(s), ~=, 24.0f);
    // a general matrix (value confirmed against numpy)
    rc_mat33f g = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                 rc_vec3f_make(0.0f, 1.0f, 4.0f),
                                 rc_vec3f_make(5.0f, 6.0f, 0.0f));
    RC_CHECK(rc_mat33f_determinant(g), ~=, 1.0f);
    // a rotation is orthogonal: determinant 1
    RC_CHECK(rc_mat33f_determinant(rc_mat33f_make_rotation_y(RC_PI_OVER_6)), ~=, 1.0f);
}

RC_TEST(mat33f, transpose)
{
    rc_mat33f a = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                 rc_vec3f_make(4.0f, 5.0f, 6.0f),
                                 rc_vec3f_make(7.0f, 8.0f, 9.0f));
    rc_mat33f t = rc_mat33f_transpose(a);
    RC_CHECK(t.cx, ==, rc_vec3f_make(1.0f, 4.0f, 7.0f));
    RC_CHECK(t.cy, ==, rc_vec3f_make(2.0f, 5.0f, 8.0f));
    RC_CHECK(t.cz, ==, rc_vec3f_make(3.0f, 6.0f, 9.0f));
}

RC_TEST(mat33f, inverse)
{
    // inverse of a diagonal scale is the reciprocal scale
    rc_mat33f s = rc_mat33f_make(rc_vec3f_make(2.0f, 0.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 4.0f, 0.0f),
                                 rc_vec3f_make(0.0f, 0.0f, 5.0f));
    rc_mat33f si = rc_mat33f_inverse(s);
    RC_CHECK(si.cx, ~=, rc_vec3f_make(0.5f, 0.0f, 0.0f));
    RC_CHECK(si.cy, ~=, rc_vec3f_make(0.0f, 0.25f, 0.0f));
    RC_CHECK(si.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 0.2f));

    // m * inverse(m) == identity for a general invertible matrix
    rc_mat33f g = rc_mat33f_make(rc_vec3f_make(1.0f, 2.0f, 3.0f),
                                 rc_vec3f_make(0.0f, 1.0f, 4.0f),
                                 rc_vec3f_make(5.0f, 6.0f, 0.0f));
    rc_mat33f p = rc_mat33f_mul(g, rc_mat33f_inverse(g));
    RC_CHECK(p.cx, ~=, rc_vec3f_make(1.0f, 0.0f, 0.0f));
    RC_CHECK(p.cy, ~=, rc_vec3f_make(0.0f, 1.0f, 0.0f));
    RC_CHECK(p.cz, ~=, rc_vec3f_make(0.0f, 0.0f, 1.0f));
}
