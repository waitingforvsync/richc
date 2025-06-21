#ifndef RICHC_MATH_MAT33F_H_
#define RICHC_MATH_MAT33F_H_

#include "richc/math/vec3f.h"


typedef struct mat33f_t {
    vec3f_t cx, cy, cz;
} mat33f_t;


// Make mat33f

static inline mat33f_t mat33f_make(vec3f_t cx, vec3f_t cy, vec3f_t cz) {
	return (mat33f_t) {cx, cy, cz};
}

static inline mat33f_t mat33f_make_transpose(vec3f_t cx, vec3f_t cy, vec3f_t cz) {
	return (mat33f_t) {
		{cx.x, cy.x, cz.x},
		{cx.y, cy.y, cz.y},
		{cx.z, cy.z, cz.z}
	};
}

static inline mat33f_t mat33f_make_zero(void) {
	return (mat33f_t) {
		vec3f_make_zero(),
		vec3f_make_zero(),
		vec3f_make_zero()
	};
}

static inline mat33f_t mat33f_make_identity(void) {
	return (mat33f_t) {
		vec3f_make_unitx(),
		vec3f_make_unity(),
		vec3f_make_unitz()
	};
}

static inline mat33f_t mat33f_make_rotation_x(float a) {
    float s = sinf(a);
    float c = cosf(a);
    return (mat33f_t) {
        {1.0f, 0.0f, 0.0f},
        {0.0f,    c,    s},
        {0.0f,   -s,    c}
    };
}

static inline mat33f_t mat33f_make_rotation_y(float a) {
    float s = sinf(a);
    float c = cosf(a);
    return (mat33f_t) {
        {   c, 0.0f,   -s},
        {0.0f, 1.0f, 0.0f},
        {   s, 0.0f,    c}
    };
}

static inline mat33f_t mat33f_make_rotation_z(float a) {
    float s = sinf(a);
    float c = cosf(a);
    return (mat33f_t) {
        {   c,    s, 0.0f},
        {  -s,    c, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };
}


// Make mat33f from other types

static inline mat33f_t mat33f_from_floats(const float *f) {
	return (mat33f_t){
		vec3f_from_floats(f),
		vec3f_from_floats(f+3),
		vec3f_from_floats(f+6)
	};
}


// Convert mat33f to other types

static inline const float *mat33f_as_floats(const mat33f_t *m) {
    return &m->cx.x;
}


// mat33f arithmetic ops

static inline mat33f_t mat33f_add(mat33f_t a, mat33f_t b) {
    return (mat33f_t) {
        vec3f_add(a.cx, b.cx),
        vec3f_add(a.cy, b.cy),
		vec3f_add(a.cz, b.cz)
    };
}

static inline mat33f_t mat33f_sub(mat33f_t a, mat33f_t b) {
    return (mat33f_t) {
        vec3f_sub(a.cx, b.cx),
        vec3f_sub(a.cy, b.cy),
		vec3f_sub(a.cz, b.cz)
    };
}

static inline mat33f_t mat33f_scalar_mul(mat33f_t m, float s) {
	return (mat33f_t) {
		vec3f_scalar_mul(m.cx, s),
		vec3f_scalar_mul(m.cy, s),
		vec3f_scalar_mul(m.cz, s)
	};
}

static inline vec3f_t mat33f_vec3f_mul(mat33f_t m, vec3f_t v) {
    return vec3f_add3(
        vec3f_scalar_mul(m.cx, v.x),
        vec3f_scalar_mul(m.cy, v.y),
		vec3f_scalar_mul(m.cz, v.z)
    );
}

static inline mat33f_t mat33f_mul(mat33f_t a, mat33f_t b) {
	return (mat33f_t) {
		mat33f_vec3f_mul(a, b.cx),
		mat33f_vec3f_mul(a, b.cy),
		mat33f_vec3f_mul(a, b.cz)
	};
}

static inline float mat33f_determinant(mat33f_t a) {
	return vec3f_dot(a.cx, vec3f_cross(a.cy, a.cz));
}

static inline mat33f_t mat33f_transpose(mat33f_t a) {
	return (mat33f_t) {
		{a.cx.x, a.cy.x, a.cz.x},
		{a.cx.y, a.cy.y, a.cz.y},
		{a.cx.z, a.cy.z, a.cz.z}
	};
}

static inline mat33f_t mat33f_inverse(mat33f_t a) {
	vec3f_t x = vec3f_cross(a.cy, a.cz);
	vec3f_t y = vec3f_cross(a.cz, a.cx);
	vec3f_t z = vec3f_cross(a.cx, a.cy);
	float det = vec3f_dot(a.cx, x);
	check(det != 0.0f);
	return mat33f_scalar_mul(
		mat33f_make_transpose(x, y, z),
		1.0f / det
	);
}


#endif // ifndef RICHC_MATH_MAT33F_H_
