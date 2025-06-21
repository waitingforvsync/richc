#ifndef RICHC_MATH_MAT22F_H_
#define RICHC_MATH_MAT22F_H_

#include "richc/math/vec2f.h"


typedef struct mat22f_t {
    vec2f_t cx, cy;
} mat22f_t;


// Make mat22f

static inline mat22f_t mat22f_make(vec2f_t cx, vec2f_t cy) {
	return (mat22f_t) {cx, cy};
}

static inline mat22f_t mat22f_make_zero(void) {
	return (mat22f_t) {
		vec2f_make_zero(),
		vec2f_make_zero()
	};
}

static inline mat22f_t mat22f_make_identity(void) {
	return (mat22f_t) {
		vec2f_make_unitx(),
		vec2f_make_unity()
	};
}

static inline mat22f_t mat22f_make_rotation(float angle) {
	float s = sinf(angle);
	float c = cosf(angle);
	return (mat22f_t) {
		{ c, s},
		{-s, c}
	};
}


// Make mat22f from other types

static inline mat22f_t mat22f_from_floats(const float *f) {
	return (mat22f_t){
		vec2f_from_floats(f),
		vec2f_from_floats(f+2)
	};
}


// Convert mat22f to other types

static inline const float *mat22f_as_floats(const mat22f_t *m) {
    return &m->cx.x;
}


// mat22f arithmetic ops

static inline mat22f_t mat22f_add(mat22f_t a, mat22f_t b) {
    return (mat22f_t) {
        vec2f_add(a.cx, b.cx),
        vec2f_add(a.cy, b.cy)
    };
}

static inline mat22f_t mat22f_sub(mat22f_t a, mat22f_t b) {
    return (mat22f_t) {
        vec2f_sub(a.cx, b.cx),
        vec2f_sub(a.cy, b.cy)
    };
}

static inline mat22f_t mat22f_scalar_mul(mat22f_t m, float s) {
	return (mat22f_t) {
		vec2f_scalar_mul(m.cx, s),
		vec2f_scalar_mul(m.cy, s)
	};
}

static inline vec2f_t mat22f_vec2f_mul(mat22f_t m, vec2f_t v) {
    return vec2f_add(
        vec2f_scalar_mul(m.cx, v.x),
        vec2f_scalar_mul(m.cy, v.y)
    );
}

static inline mat22f_t mat22f_mul(mat22f_t a, mat22f_t b) {
	return (mat22f_t) {
		mat22f_vec2f_mul(a, b.cx),
		mat22f_vec2f_mul(a, b.cy)
	};
}

static inline float mat22f_determinant(mat22f_t a) {
	return a.cx.x * a.cy.y - a.cx.y * a.cy.x;
}

static inline mat22f_t mat22f_transpose(mat22f_t a) {
	return (mat22f_t) {
		{a.cx.x, a.cy.x},
		{a.cx.y, a.cy.y}
	};
}

static inline mat22f_t mat22f_inverse(mat22f_t a) {
    float det = mat22f_determinant(a);
    check(det != 0.0f);
	float d = 1.0f / det;
	return (mat22f_t) {
		{ a.cy.y * d, -a.cx.y * d},
		{-a.cy.x * d,  a.cx.x * d}
	};
}


#endif // ifndef RICHC_MATH_MAT22F_H_
