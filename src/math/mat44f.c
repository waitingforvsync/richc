#include "richc/math/mat44f.h"

/*
 * Determinant of a 4×4 matrix using the full Leibniz expansion (24 terms).
 *
 * The formula is obtained by cofactor expansion along the first column.
 * Each of the 24 signed products corresponds to one of the 4! = 24 permutations
 * of the column indices.
 */
float rc_mat44f_determinant(rc_mat44f m)
{
    return
        m.cx.w * m.cy.z * m.cz.y * m.cw.x - m.cx.z * m.cy.w * m.cz.y * m.cw.x -
        m.cx.w * m.cy.y * m.cz.z * m.cw.x + m.cx.y * m.cy.w * m.cz.z * m.cw.x +
        m.cx.z * m.cy.y * m.cz.w * m.cw.x - m.cx.y * m.cy.z * m.cz.w * m.cw.x -
        m.cx.w * m.cy.z * m.cz.x * m.cw.y + m.cx.z * m.cy.w * m.cz.x * m.cw.y +
        m.cx.w * m.cy.x * m.cz.z * m.cw.y - m.cx.x * m.cy.w * m.cz.z * m.cw.y -
        m.cx.z * m.cy.x * m.cz.w * m.cw.y + m.cx.x * m.cy.z * m.cz.w * m.cw.y +
        m.cx.w * m.cy.y * m.cz.x * m.cw.z - m.cx.y * m.cy.w * m.cz.x * m.cw.z -
        m.cx.w * m.cy.x * m.cz.y * m.cw.z + m.cx.x * m.cy.w * m.cz.y * m.cw.z +
        m.cx.y * m.cy.x * m.cz.w * m.cw.z - m.cx.x * m.cy.y * m.cz.w * m.cw.z -
        m.cx.z * m.cy.y * m.cz.x * m.cw.w + m.cx.y * m.cy.z * m.cz.x * m.cw.w +
        m.cx.z * m.cy.x * m.cz.y * m.cw.w - m.cx.x * m.cy.z * m.cz.y * m.cw.w -
        m.cx.y * m.cy.x * m.cz.z * m.cw.w + m.cx.x * m.cy.y * m.cz.z * m.cw.w;
}

/*
 * Inverse of a 4×4 matrix using the 2×2 sub-determinant method.
 *
 * s0..s5 are the six 2×2 sub-determinants from the upper-left 2×4 sub-matrix
 * (columns cx, cy crossed with rows x/y and y/z, etc.).
 * c0..c5 are the corresponding sub-determinants from the lower-right 2×4 block
 * (columns cz, cw crossed with rows z/w).
 *
 * The overall determinant is: det = s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0.
 * Asserts that det != 0.
 */
rc_mat44f rc_mat44f_inverse(rc_mat44f m)
{
    float s0 = m.cx.x * m.cy.y - m.cy.x * m.cx.y;
    float s1 = m.cx.x * m.cy.z - m.cy.x * m.cx.z;
    float s2 = m.cx.x * m.cy.w - m.cy.x * m.cx.w;
    float s3 = m.cx.y * m.cy.z - m.cy.y * m.cx.z;
    float s4 = m.cx.y * m.cy.w - m.cy.y * m.cx.w;
    float s5 = m.cx.z * m.cy.w - m.cy.z * m.cx.w;

    float c0 = m.cz.x * m.cw.y - m.cw.x * m.cz.y;
    float c1 = m.cz.x * m.cw.z - m.cw.x * m.cz.z;
    float c2 = m.cz.x * m.cw.w - m.cw.x * m.cz.w;
    float c3 = m.cz.y * m.cw.z - m.cw.y * m.cz.z;
    float c4 = m.cz.y * m.cw.w - m.cw.y * m.cz.w;
    float c5 = m.cz.z * m.cw.w - m.cw.z * m.cz.w;

    float det = s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0;
    RC_ASSERT(det != 0.0f);
    float d = 1.0f / det;

    return (rc_mat44f) {
        {( m.cy.y*c5 - m.cy.z*c4 + m.cy.w*c3) * d,
         (-m.cx.y*c5 + m.cx.z*c4 - m.cx.w*c3) * d,
         ( m.cw.y*s5 - m.cw.z*s4 + m.cw.w*s3) * d,
         (-m.cz.y*s5 + m.cz.z*s4 - m.cz.w*s3) * d},
        {(-m.cy.x*c5 + m.cy.z*c2 - m.cy.w*c1) * d,
         ( m.cx.x*c5 - m.cx.z*c2 + m.cx.w*c1) * d,
         (-m.cw.x*s5 + m.cw.z*s2 - m.cw.w*s1) * d,
         ( m.cz.x*s5 - m.cz.z*s2 + m.cz.w*s1) * d},
        {( m.cy.x*c4 - m.cy.y*c2 + m.cy.w*c0) * d,
         (-m.cx.x*c4 + m.cx.y*c2 - m.cx.w*c0) * d,
         ( m.cw.x*s4 - m.cw.y*s2 + m.cw.w*s0) * d,
         (-m.cz.x*s4 + m.cz.y*s2 - m.cz.w*s0) * d},
        {(-m.cy.x*c3 + m.cy.y*c1 - m.cy.z*c0) * d,
         ( m.cx.x*c3 - m.cx.y*c1 + m.cx.z*c0) * d,
         (-m.cw.x*s3 + m.cw.y*s1 - m.cw.z*s0) * d,
         ( m.cz.x*s3 - m.cz.y*s1 + m.cz.z*s0) * d}
    };
}
