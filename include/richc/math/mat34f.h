#ifndef RICHC_MATH_MAT34F_H_
#define RICHC_MATH_MAT34F_H_

#include "richc/math/mat33f.h"


typedef struct mat34f_t {
    mat33f_t m;
    vec3f_t t;
} mat34f_t;


// Make mat34f

static inline mat34f_t mat34f_make(mat33f_t m, vec3f_t t) {
	return (mat34f_t) {m, t};
}

static inline mat34f_t mat34f_make_identity(void) {
	return (mat34f_t) {
		mat33f_make_identity(),
		vec3f_make_zero()
	};
}

static inline mat34f_t mat34f_make_translation(vec3f_t v) {
	return (mat34f_t) {
		mat33f_make_identity(),
		v
	};
}

static inline mat34f_t mat34f_make_lookat(vec3f_t eye, vec3f_t focus, vec3f_t up) {
	vec3f_t forward = vec3f_normalize(vec3f_sub(focus, eye));
	vec3f_t side = vec3f_normalize(vec3f_cross(forward, up));
    mat33f_t mi = mat33f_make_transpose(
		side,
		vec3f_cross(side, forward),
		vec3f_negate(forward)
	);
    return (mat34f_t) {
		mi,
		mat33f_vec3f_mul(mi, vec3f_negate(eye))
	};
}


// Make mat34f from other types

static inline mat34f_t mat34f_from_mat33f(mat33f_t m) {
	return (mat34f_t) {
		m,
		vec3f_make_zero()
	};
}

static inline mat34f_t mat34f_from_floats(const float *f) {
	return (mat34f_t) {
		mat33f_from_floats(f),
		vec3f_from_floats(f+9)
	};
}


// Convert mat34f to other types

static inline const float *mat34f_as_floats(const mat34f_t *m) {
    return &m->m.cx.x;
}


// mat34f arithmetic ops

static inline vec3f_t mat34f_vec3f_mul(mat34f_t m, vec3f_t v) {
	return vec3f_add(
		mat33f_vec3f_mul(m.m, v),
		m.t
	);
}

static inline mat34f_t mat34f_mul(mat34f_t a, mat34f_t b) {
	return (mat34f_t) {
		mat33f_mul(a.m, b.m),
		mat34f_vec3f_mul(a, b.t)
	};
}

static inline mat34f_t mat33f_mat34f_mul(mat33f_t a, mat34f_t b) {
	return (mat34f_t) {
		mat33f_mul(a, b.m),
		mat33f_vec3f_mul(a, b.t)
	};
}

static inline mat34f_t mat34f_mat33f_mul(mat34f_t a, mat33f_t b) {
	return (mat34f_t) {
		mat33f_mul(a.m, b),
		a.t
	};
}

static inline mat34f_t mat34f_inverse(mat34f_t m) {
	mat33f_t mi = mat33f_inverse(m.m);
	return (mat34f_t) {
		mi,
		mat33f_vec3f_mul(mi, vec3f_negate(m.t))
	};
}


#endif // ifndef RICHC_MATH_MAT34F_H_
