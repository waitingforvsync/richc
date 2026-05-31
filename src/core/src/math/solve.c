#include "richc/math/solve.h"

#include <math.h>

static const float eps = 1e-6f;

/* ---- quadratic ---- */

/*
 * Solve a*t^2 + b*t + c = 0.
 *
 * Numerically stable: compute the larger-magnitude root first (sign-selection to
 * avoid catastrophic cancellation), then derive the second via Vieta's theorem
 * (the product of the roots is c/a).
 */
rc_quadratic_roots rc_solve_quadratic(float a, float b, float c)
{
    if (fabsf(a) < eps) {
        // linear: b*t + c = 0
        if (fabsf(b) < eps) {
            return (rc_quadratic_roots) {0};
        }
        return (rc_quadratic_roots) {
            .num_roots = 1,
            .root = {-c / b}
        };
    }

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) {
        return (rc_quadratic_roots) {0};
    }
    if (disc == 0.0f) {
        return (rc_quadratic_roots) {
            .num_roots = 1,
            .root = {-b / (2.0f * a)}
        };
    }

    float sq = sqrtf(disc);
    // q = -(b + sign(b)*sqrt(disc)) / 2: the sign matches b so the terms add,
    // never a cancelling subtraction of two near-equal magnitudes.  q/a is then
    // the well-conditioned root, and the other comes from the product of the
    // roots c/a (Vieta), i.e. (c/a)/(q/a) = c/q - a plain divide, no subtraction.
    float q  = ((b >= 0.0f) ? (-b - sq) : (-b + sq)) * 0.5f;
    return (rc_quadratic_roots) {
        .num_roots = 2,
        .root = {q / a, c / q}
    };
}

/* ---- cubic ---- */

/*
 * Solve a*t^3 + b*t^2 + c*t + d = 0.
 *
 * Reduce to the depressed form u^3 + p*u + q = 0 via t = u - A/3, where the
 * discriminant is q^2/4 + p^3/27:
 *   > 0  -> one real root (Cardano).
 *   <= 0 -> three real roots (trigonometric method).
 *
 * cbrtf handles negative arguments directly.  The Cardano sqrt is only reached
 * when the discriminant is positive, and the trigonometric sqrt only when p < 0,
 * which the discriminant condition guarantees.
 */
rc_cubic_roots rc_solve_cubic(float a, float b, float c, float d)
{
    if (fabsf(a) < eps) {
        rc_quadratic_roots qr = rc_solve_quadratic(b, c, d);
        return (rc_cubic_roots) {
            .num_roots = qr.num_roots,
            .root = {
                qr.root[0],
                qr.root[1]
            }
        };
    }

    float A  = b / a, B = c / a, C = d / a;
    float A3 = A / 3.0f;
    float p  = B - A * A3;                       // B - A^2/3
    float q  = C + A3 * (2.0f * A3 * A3 - B);    // 2*A^3/27 - A*B/3 + C

    float discr = q * q * 0.25f + p * p * p / 27.0f;

    if (discr > 0.0f) {
        // one real root - Cardano's formula
        float sqrt_d = sqrtf(discr);
        return (rc_cubic_roots) {
            .num_roots = 1,
            .root = {cbrtf(-q * 0.5f + sqrt_d) + cbrtf(-q * 0.5f - sqrt_d) - A3}
        };
    }

    if (p > -eps) {
        // p ~ 0: nearly-triple root at cbrt(-q)
        return (rc_cubic_roots) {
            .num_roots = 1,
            .root = {cbrtf(-q) - A3}
        };
    }

    // three real roots - trigonometric method.
    // discr <= 0 implies p < 0, so -p/3 > 0 and sqrtf is safe.
    const float two_pi_over_3 = 2.0943951f;
    float r = sqrtf(-p / 3.0f);
    float cos3phi = -q / (2.0f * r * r * r);
    if (cos3phi < -1.0f) cos3phi = -1.0f;
    if (cos3phi >  1.0f) cos3phi =  1.0f;
    float phi = acosf(cos3phi) / 3.0f;
    float t2r = 2.0f * r;
    return (rc_cubic_roots) {
        .num_roots = 3,
        .root = {
            t2r * cosf(phi) - A3,
            t2r * cosf(phi + two_pi_over_3) - A3,
            t2r * cosf(phi + 2.0f * two_pi_over_3) - A3
        }
    };
}
