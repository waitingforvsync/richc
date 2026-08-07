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
    // left, right, top, bottom, near, far; reverse-Z: near face -> depth 1
    rc_mat44f o = rc_mat44f_make_ortho(0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 5.0f);
    // (left, bottom, -near) maps to (-1, -1) with depth 1 (near, reverse-Z); w untouched
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f)), ~=, rc_vec4f_make(-1.0f, -1.0f, 1.0f, 1.0f));
    // (right, top, -far) maps to (1, 1) with depth 0 (far)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(4.0f, 2.0f, -5.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, 1.0f, 0.0f, 1.0f));
    // the centre of the volume maps to (0, 0) at mid depth
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(2.0f, 1.0f, -3.0f, 1.0f)), ~=, rc_vec4f_make(0.0f, 0.0f, 0.5f, 1.0f));
    // remaining NDC-cube corners, mixing the extents (z sense reversed vs x/y)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(0.0f, 2.0f, -5.0f, 1.0f)), ~=, rc_vec4f_make(-1.0f, 1.0f, 0.0f, 1.0f));
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(4.0f, 0.0f, -1.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, -1.0f, 1.0f, 1.0f));

    // round-trip a point through the matrix and its inverse
    rc_vec4f pt = rc_vec4f_make(1.5f, 0.5f, -2.5f, 1.0f);
    rc_vec4f back = rc_mat44f_vec4f_mul(rc_mat44f_inverse(o), rc_mat44f_vec4f_mul(o, pt));
    RC_CHECK(back, ~=, pt);
}

RC_TEST(mat44f, ortho_2d)
{
    // 640x480 pixel rect, origin top-left, y down, to canonical NDC (y up)
    rc_mat44f o = rc_mat44f_make_ortho_2d(640.0f, 480.0f);
    // (0, 0) (top-left pixel corner) -> NDC (-1, +1); z = 0 -> depth 1 (near plane)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(0.0f, 0.0f, 0.0f, 1.0f)), ~=, rc_vec4f_make(-1.0f, 1.0f, 1.0f, 1.0f));
    // (w, h) (bottom-right) -> NDC (+1, -1)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(640.0f, 480.0f, 0.0f, 1.0f)), ~=, rc_vec4f_make(1.0f, -1.0f, 1.0f, 1.0f));
    // (w/2, h/2) (centre) -> NDC (0, 0)
    RC_CHECK(rc_mat44f_vec4f_mul(o, rc_vec4f_make(320.0f, 240.0f, 0.0f, 1.0f)), ~=, rc_vec4f_make(0.0f, 0.0f, 1.0f, 1.0f));
}

RC_TEST(mat44f, perspective)
{
    // 90 degree vertical fov, 2:1 aspect, near 1, far 5; reverse-Z depth [0,1]
    rc_mat44f p = rc_mat44f_make_perspective(RC_PI_OVER_2, 2.0f, 1.0f, 5.0f);
    // near-plane centre: clip.z == n and clip.w == n, so ndc.z == 1 exactly
    rc_vec4f near_c = rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.0f, 0.0f, -1.0f, 1.0f));
    RC_CHECK(near_c, ~=, rc_vec4f_make(0.0f, 0.0f, 1.0f, 1.0f));
    RC_CHECK(near_c.z / near_c.w, ~=, 1.0f);
    // far-plane centre: clip.z == 0 exactly, so ndc.z == 0
    rc_vec4f far_c = rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.0f, 0.0f, -5.0f, 1.0f));
    RC_CHECK(far_c, ~=, rc_vec4f_make(0.0f, 0.0f, 0.0f, 5.0f));
    // x/y scales are unchanged by the depth convention: a/aspect and a (a = 1 here)
    rc_vec4f edge = rc_mat44f_vec4f_mul(p, rc_vec4f_make(2.0f, 1.0f, -1.0f, 1.0f));
    RC_CHECK(edge.x, ~=, 2.0f * 0.5f);
    RC_CHECK(edge.y, ~=, 1.0f);
    // clip.w == -z_v for any point
    rc_vec4f mid = rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.3f, 0.7f, -3.0f, 1.0f));
    RC_CHECK(mid.w, ~=, 3.0f);
}

RC_TEST(mat44f, perspective_inf)
{
    rc_mat44f p = rc_mat44f_make_perspective_inf(RC_PI_OVER_2, 1.0f, 2.0f);
    // near plane -> depth 1
    rc_vec4f near_c = rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.0f, 0.0f, -2.0f, 1.0f));
    RC_CHECK(near_c.z / near_c.w, ~=, 1.0f);
    // a distant point -> ndc.z == n / distance
    rc_vec4f far_pt = rc_mat44f_vec4f_mul(p, rc_vec4f_make(0.0f, 0.0f, -1000.0f, 1.0f));
    RC_CHECK(far_pt.z / far_pt.w, ~=, 2.0f / 1000.0f);
    // agreement with the finite-far matrix as f grows large
    rc_mat44f pf = rc_mat44f_make_perspective(RC_PI_OVER_2, 1.0f, 2.0f, 100000.0f);
    rc_vec4f a = rc_mat44f_vec4f_mul(p,  rc_vec4f_make(1.0f, -1.0f, -50.0f, 1.0f));
    rc_vec4f b = rc_mat44f_vec4f_mul(pf, rc_vec4f_make(1.0f, -1.0f, -50.0f, 1.0f));
    RC_CHECK(a.x, ~=, b.x);
    RC_CHECK(a.y, ~=, b.y);
    RC_CHECK(a.z / a.w, ~=, b.z / b.w);
    RC_CHECK(a.w, ~=, b.w);
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
