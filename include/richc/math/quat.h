#ifndef RICHC_MATH_QUAT_H_
#define RICHC_MATH_QUAT_H_

#include "richc/math/mat33f.h"


typedef struct quatf_t {
    vec3f_t xyz;
    float w;
} quatf_t;


static inline quatf_t quatf_make(float x, float y, float z, float w) {
    return (quatf_t) {vec3f_make(x, y, z), w};
}

static inline quatf_t quatf_make_identity(void) {
	return (quatf_t) {vec3f_make_zero(), 1.0f};
}

static inline quatf_t quatf_make_angle_axis(float angle, vec3f_t axis) {
	return (quatf_t) {
		vec3f_scalar_mul(vec3f_normalize(axis), sinf(angle / 2.0f)),
		cosf(angle / 2.0f)
	};
}


static inline quatf_t quatf_from_floats(const float *f) {
	return (quatf_t) {vec3f_from_floats(f), f[3]};
}

static inline quatf_t quatf_from_vec3f(vec3f_t xyz, float w) {
    return (quatf_t) {xyz, w};
}

static inline quatf_t quatf_from_mat33f(mat33f_t m) {
	return (quatf_t) {
		{copysignf(sqrtf(fmaxf(0.0f, 1.0f + m.cx.x - m.cy.y - m.cz.z)) / 2.0f, m.cy.z - m.cz.y),
		 copysignf(sqrtf(fmaxf(0.0f, 1.0f - m.cx.x + m.cy.y - m.cz.z)) / 2.0f, m.cz.x - m.cx.z),
		 copysignf(sqrtf(fmaxf(0.0f, 1.0f - m.cx.x - m.cy.y + m.cz.z)) / 2.0f, m.cx.y - m.cy.x)},
		sqrtf(fmaxf(0.0f, 1.0f + m.cx.x + m.cy.y + m.cz.z)) / 2.0f
	};
}

static inline mat33f_t mat33f_from_quatf(quatf_t a) {
    float xx = a.xyz.x * a.xyz.x;
    float xy = a.xyz.x * a.xyz.y;
    float xz = a.xyz.x * a.xyz.z;
    float yy = a.xyz.y * a.xyz.y;
    float yz = a.xyz.y * a.xyz.z;
    float zz = a.xyz.z * a.xyz.z;
    float xw = a.xyz.x * a.w;
    float yw = a.xyz.y * a.w;
    float zw = a.xyz.z * a.w;
    float ww = a.w * a.w;
    return (mat33f_t) {
        {ww + xx - yy - zz,  2.0f * (xy + zw),   2.0f * (xz - yw)},
        {2.0f * (xy - zw),   ww - xx + yy - zz,  2.0f * (yz + xw)},
        {2.0f * (xz + yw),   2.0f * (yz - xw),   ww - xx - yy + zz}
    };
}


static inline quatf_t quatf_conjugate(quatf_t a) {
	return (quatf_t) {vec3f_negate(a.xyz), a.w};
}

static inline quatf_t quatf_mul(quatf_t a, quatf_t b) {
	return (quatf_t) {
		vec3f_add3(
            vec3f_scalar_mul(b.xyz, a.w),
            vec3f_scalar_mul(a.xyz, b.w),
            vec3f_cross(a.xyz, b.xyz)
        ),
		a.w * b.w - vec3f_dot(a.xyz, b.xyz)
	};
}

static inline vec3f_t quatf_vec3f_transform(quatf_t q, vec3f_t v) {
    vec3f_t t = vec3f_scalar_mul(vec3f_cross(q.xyz, v), 2.0f);
    return vec3f_add3(
        v,
        vec3f_scalar_mul(t, q.w),
        vec3f_cross(q.xyz, t)
    );
}



#endif // ifndef RICHC_MATH_QUAT_H_
