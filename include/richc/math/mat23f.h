#ifndef RICHC_MATH_MAT23F_H_
#define RICHC_MATH_MAT23F_H_

#include "richc/math/mat22f.h"


typedef struct mat23f_t {
    mat22f_t m;
    vec2f_t t;
} mat23f_t;


// Make mat23f

static inline mat23f_t mat23f_make(mat22f_t m, vec2f_t t) {
	return (mat23f_t) {m, t};
}

static inline mat23f_t mat23f_make_identity(void) {
	return (mat23f_t) {
		mat22f_make_identity(),
		vec2f_make_zero()
	};
}

static inline mat23f_t mat23f_make_translation(vec2f_t v) {
	return (mat23f_t) {
		mat22f_make_identity(),
		v
	};
}


// Make mat23f from other types

static inline mat23f_t mat23f_from_mat22f(mat22f_t m) {
	return (mat23f_t) {
		m,
		vec2f_make_zero()
	};
}

static inline mat23f_t mat23f_from_floats(const float *f) {
	return (mat23f_t) {
		mat22f_from_floats(f),
		vec2f_from_floats(f+4)
	};
}


// Convert mat23f to other types

static inline const float *mat23f_as_floats(const mat23f_t *m) {
    return &m->m.cx.x;
}


// mat23f arithmetic ops

static inline vec2f_t mat23f_vec2f_mul(mat23f_t m, vec2f_t v) {
	return vec2f_add(
		mat22f_vec2f_mul(m.m, v),
		m.t
	);
}

static inline mat23f_t mat23f_mul(mat23f_t a, mat23f_t b) {
	return (mat23f_t) {
		mat22f_mul(a.m, b.m),
		mat23f_vec2f_mul(a, b.t)
	};
}

static inline mat23f_t mat22f_mat23f_mul(mat22f_t a, mat23f_t b) {
	return (mat23f_t) {
		mat22f_mul(a, b.m),
		mat22f_vec2f_mul(a, b.t)
	};
}

static inline mat23f_t mat23f_mat22f_mul(mat23f_t a, mat22f_t b) {
	return (mat23f_t) {
		mat22f_mul(a.m, b),
		a.t
	};
}

static inline mat23f_t mat23f_inverse(mat23f_t m) {
	mat22f_t mi = mat22f_inverse(m.m);
	return (mat23f_t) {
		mi,
		mat22f_vec2f_mul(mi, vec2f_negate(m.t))
	};
}


#endif // ifndef RICHC_MATH_MAT23F_H_
