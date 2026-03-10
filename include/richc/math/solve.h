/*
 * math/solve.h - analytic polynomial root solvers.
 *
 * rc_solve_quadratic  —  a*t² + b*t + c = 0
 * rc_solve_cubic      —  a*t³ + b*t² + c*t + d = 0
 *
 * Both functions return the number of real roots in num_roots and write them
 * to root[].  Degenerate leading coefficients are handled gracefully
 * (quadratic falls back to linear when |a| < eps; cubic delegates to
 * rc_solve_quadratic).
 *
 * Quadratic uses sign-selection to avoid catastrophic cancellation.
 * Cubic uses Cardano's formula for one real root (discriminant > 0) and the
 * trigonometric method for three real roots (discriminant ≤ 0).
 */

#ifndef RC_MATH_SOLVE_H_
#define RC_MATH_SOLVE_H_

/* Result types — returned by value. */

typedef struct {
    int   num_roots;
    float root[2];
} rc_quadratic_roots;

typedef struct {
    int   num_roots;
    float root[3];
} rc_cubic_roots;

/* Solve a*t² + b*t + c = 0.  Returns 0, 1, or 2 real roots. */
rc_quadratic_roots rc_solve_quadratic(float a, float b, float c);

/* Solve a*t³ + b*t² + c*t + d = 0.  Returns 1 or 3 real roots. */
rc_cubic_roots     rc_solve_cubic    (float a, float b, float c, float d);

#endif /* RC_MATH_SOLVE_H_ */
