#ifndef RICHC_MATH_MAT44F_H_
#define RICHC_MATH_MAT44F_H_

#include "richc/math/vec4f.h"
#include "richc/math/mat34f.h"
#include "richc/math/mat23f.h"


typedef struct mat44f_t {
    vec4f_t cx, cy, cz, cw;
} mat44f_t;


// Make mat44f

static inline mat44f_t mat44f_make(vec4f_t cx, vec4f_t cy, vec4f_t cz, vec4f_t cw) {
	return (mat44f_t) {cx, cy, cz, cw};
}

static inline mat44f_t mat44f_make_transpose(vec4f_t cx, vec4f_t cy, vec4f_t cz, vec4f_t cw) {
	return (mat44f_t) {
		{cx.x, cy.x, cz.x, cw.x},
		{cx.y, cy.y, cz.y, cw.y},
		{cx.z, cy.z, cz.z, cw.z},
		{cx.w, cy.w, cz.w, cw.w}
	};
}

static inline mat44f_t mat44f_make_zero(void) {
	return (mat44f_t) {
		vec4f_make_zero(),
		vec4f_make_zero(),
		vec4f_make_zero(),
		vec4f_make_zero()
	};
}

static inline mat44f_t mat44f_make_identity(void) {
	return (mat44f_t) {
		vec4f_make_unitx(),
		vec4f_make_unity(),
		vec4f_make_unitz(),
		vec4f_make_unitw()
	};
}

static inline mat44f_t mat44f_make_translation(vec3f_t v) {
    return (mat44f_t) {
        vec4f_make_unitx(),
        vec4f_make_unity(),
        vec4f_make_unitz(),
        vec4f_from_vec3f(v, 1)
    };
}

static inline mat44f_t mat44f_make_ortho(float left, float right, float top, float bottom, float near, float far) {
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;
    return (mat44f_t) {
        vec4f_scalar_mul(vec4f_make_unitx(), 2.0f / rl),
        vec4f_scalar_mul(vec4f_make_unity(), 2.0f / tb),
        vec4f_scalar_mul(vec4f_make_unitz(), -2.0f / fn),
        {-(right + left) / rl, -(top + bottom) / tb, -(far + near) / fn, 1.0f}
    };
}

static inline mat44f_t mat44f_make_perspective(float y_fov, float aspect, float n, float f) {
	float a = 1.0f / tanf(y_fov / 2.0f);
	return (mat44f_t) {
		{a / aspect, 0.0f, 0.0f, 0.0f},
		{0.0f, a, 0.0f, 0.0f},
		{0.0f, 0.0f, -(f+n) / (f-n), -1.0f},
		{0.0f, 0.0f, -2.0f * f * n / (f-n), 0.0f}
	};
}


// Make mat44f from other types

static inline mat44f_t mat44f_from_mat22f(mat22f_t m) {
    return (mat44f_t) {
        vec4f_from_vec2f(m.cx, 0, 0),
        vec4f_from_vec2f(m.cy, 0, 0),
        vec4f_make_unitz(),
        vec4f_make_unitw()
    };
}

static inline mat44f_t mat44f_from_mat33f(mat33f_t m) {
	return (mat44f_t) {
		vec4f_from_vec3f(m.cx, 0.0f),
		vec4f_from_vec3f(m.cy, 0.0f),
		vec4f_from_vec3f(m.cz, 0.0f),
		vec4f_make_unitw()
	};
}

static inline mat44f_t mat44f_from_mat34f(mat34f_t m) {
	return (mat44f_t) {
		vec4f_from_vec3f(m.m.cx, 0.0f),
		vec4f_from_vec3f(m.m.cy, 0.0f),
		vec4f_from_vec3f(m.m.cz, 0.0f),
		vec4f_from_vec3f(m.t, 1.0f)
	};
}

static inline mat44f_t mat44f_from_floats(const float *f) {
	return (mat44f_t){
		vec4f_from_floats(f),
		vec4f_from_floats(f+4),
		vec4f_from_floats(f+8),
		vec4f_from_floats(f+12)
	};
}


// Convert mat44f to other types

static inline const float *mat44f_as_floats(const mat44f_t *m) {
    return &m->cx.x;
}


// mat44f arithmetic ops

static inline mat44f_t mat44f_add(mat44f_t a, mat44f_t b) {
    return (mat44f_t) {
        vec4f_add(a.cx, b.cx),
        vec4f_add(a.cy, b.cy),
		vec4f_add(a.cz, b.cz),
		vec4f_add(a.cw, b.cw)
    };
}

static inline mat44f_t mat44f_sub(mat44f_t a, mat44f_t b) {
    return (mat44f_t) {
        vec4f_sub(a.cx, b.cx),
        vec4f_sub(a.cy, b.cy),
		vec4f_sub(a.cz, b.cz),
		vec4f_sub(a.cw, b.cw)
    };
}

static inline mat44f_t mat44f_scalar_mul(mat44f_t m, float s) {
	return (mat44f_t) {
		vec4f_scalar_mul(m.cx, s),
		vec4f_scalar_mul(m.cy, s),
		vec4f_scalar_mul(m.cz, s),
		vec4f_scalar_mul(m.cw, s)
	};
}

static inline vec4f_t mat44f_vec4f_mul(mat44f_t m, vec4f_t v) {
    return vec4f_add4(
        vec4f_scalar_mul(m.cx, v.x),
        vec4f_scalar_mul(m.cy, v.y),
		vec4f_scalar_mul(m.cz, v.z),
		vec4f_scalar_mul(m.cw, v.w)
    );
}

static inline mat44f_t mat44f_mul(mat44f_t a, mat44f_t b) {
	return (mat44f_t) {
		mat44f_vec4f_mul(a, b.cx),
		mat44f_vec4f_mul(a, b.cy),
		mat44f_vec4f_mul(a, b.cz),
		mat44f_vec4f_mul(a, b.cw)
	};
}

float mat44f_determinant(mat44f_t m);

static inline mat44f_t mat44f_transpose(mat44f_t a) {
	return (mat44f_t) {
		{a.cx.x, a.cy.x, a.cz.x, a.cw.x},
		{a.cx.y, a.cy.y, a.cz.y, a.cw.y},
		{a.cx.z, a.cy.z, a.cz.z, a.cw.z},
		{a.cx.w, a.cy.w, a.cz.w, a.cw.w}
	};
}

mat44f_t mat44f_inverse(mat44f_t m);


#endif // ifndef RICHC_MATH_MAT44F_H_
