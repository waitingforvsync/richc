/*
 * math/solve.c - analytic polynomial root solvers.
 */

#include "richc/math/solve.h"
#include <math.h>

#define EPS_ 1e-6f

/* ---- quadratic ---- */

/*
 * Solve a*t² + b*t + c = 0.
 *
 * Numerically stable: pick the larger-magnitude root first (sign-selection to
 * avoid catastrophic cancellation), then derive the second via Vieta's theorem.
 */
rc_quadratic_roots rc_solve_quadratic(float a, float b, float c)
{
    if (fabsf(a) < EPS_) {
        /* Linear: b*t + c = 0 */
        if (fabsf(b) < EPS_) return (rc_quadratic_roots) {0};
        return (rc_quadratic_roots) { .num_roots = 1, .root = {-c / b} };
    }

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return (rc_quadratic_roots) {0};
    if (disc == 0.0f)
        return (rc_quadratic_roots) { .num_roots = 1, .root = {-b / (2.0f * a)} };

    float sq = sqrtf(disc);
    float q  = ((b >= 0.0f) ? (-b - sq) : (-b + sq)) * 0.5f;
    float t0 = q / a;
    float t1 = c / q;
    return (rc_quadratic_roots) { .num_roots = 2, .root = {t0, t1} };
}

/* ---- cubic ---- */

/*
 * Solve a*t³ + b*t² + c*t + d = 0.
 *
 * Reduces to depressed form u³ + pu + q = 0 via t = u - A/3.
 * discriminant = q²/4 + p³/27:
 *   > 0 → one real root (Cardano).
 *   ≤ 0 → three real roots (trigonometric method).
 *
 * cbrtf handles negative arguments directly per the C standard.
 * The sqrt inside the Cardano branch is only called when disc > 0.
 * The sqrt inside the trig branch is only called when p < 0, which is
 * guaranteed by the discriminant condition.
 */
rc_cubic_roots rc_solve_cubic(float a, float b, float c, float d)
{
    if (fabsf(a) < EPS_) {
        rc_quadratic_roots qr = rc_solve_quadratic(b, c, d);
        return (rc_cubic_roots) {
            .num_roots = qr.num_roots,
            .root      = {qr.root[0], qr.root[1]},
        };
    }

    float A  = b / a,  B = c / a,  C = d / a;
    float A3 = A / 3.0f;
    float p  = B - A * A3;                         /* = B - A²/3        */
    float q  = C + A3 * (2.0f * A3 * A3 - B);     /* = 2A³/27 - AB/3 + C */

    float discr = q * q * 0.25f + p * p * p / 27.0f;

    if (discr > 0.0f) {
        /* One real root — Cardano's formula. */
        float sqrtD = sqrtf(discr);
        return (rc_cubic_roots) {
            .num_roots = 1,
            .root      = {cbrtf(-q * 0.5f + sqrtD) + cbrtf(-q * 0.5f - sqrtD) - A3},
        };
    }

    if (p > -EPS_) {
        /* p ≈ 0: nearly-triple root at cbrt(-q). */
        return (rc_cubic_roots) { .num_roots = 1, .root = {cbrtf(-q) - A3} };
    }

    /* Three real roots — trigonometric method.
     * discr ≤ 0 implies p < 0, so -p/3 > 0 (sqrtf is safe). */
    static const float k_pi23 = 2.09439510239319549f; /* 2π/3 */
    float r_trig  = sqrtf(-p / 3.0f);
    float cos3phi = -q / (2.0f * r_trig * r_trig * r_trig);
    if (cos3phi < -1.0f) cos3phi = -1.0f;
    if (cos3phi >  1.0f) cos3phi =  1.0f;
    float phi = acosf(cos3phi) / 3.0f;
    float t2r = 2.0f * r_trig;
    return (rc_cubic_roots) {
        .num_roots = 3,
        .root      = {
            t2r * cosf(phi)                  - A3,
            t2r * cosf(phi + k_pi23)         - A3,
            t2r * cosf(phi + 2.0f * k_pi23)  - A3,
        },
    };
}

#undef EPS_
