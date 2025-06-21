#ifndef RICHC_MATH_VEC4F_H_
#define RICHC_MATH_VEC4F_H_

#include "richc/math/vec3f.h"


typedef struct vec4f_t {
    float x, y, z, w;
} vec4f_t;


// Make vec4f

static inline vec4f_t vec4f_make(float x, float y, float z, float w) {
    return (vec4f_t) {x, y, z, w};
}

static inline vec4f_t vec4f_make_zero(void) {
    return (vec4f_t) {0.0f, 0.0f, 0.0f, 0.0f};
}

static inline vec4f_t vec4f_make_unitx(void) {
    return (vec4f_t) {1.0f, 0.0f, 0.0f, 0.0f};
}

static inline vec4f_t vec4f_make_unity(void) {
    return (vec4f_t) {0.0f, 1.0f, 0.0f, 0.0f};
}

static inline vec4f_t vec4f_make_unitz(void) {
    return (vec4f_t) {0.0f, 0.0f, 1.0f, 0.0f};
}

static inline vec4f_t vec4f_make_unitw(void) {
    return (vec4f_t) {0.0f, 0.0f, 0.0f, 1.0f};
}


// Make vec4f from other types

static inline vec4f_t vec4f_from_floats(const float *f) {
	return (vec4f_t) {f[0], f[1], f[2], f[3]};
}

static inline vec4f_t vec4f_from_vec2f(vec2f_t v, float z, float w) {
    return (vec4f_t) {v.x, v.y, z, w};
}

static inline vec4f_t vec4f_from_vec3f(vec3f_t v, float w) {
    return (vec4f_t) {v.x, v.y, v.z, w};
}


// Convert vec4f to other types

static inline const float *vec4f_as_floats(const vec4f_t *a) {
    return &a->x;
}


// Arithmetic ops

static inline vec4f_t vec4f_add(vec4f_t a, vec4f_t b) {
    return (vec4f_t) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z,
        a.w + b.w
    };
}

static inline vec4f_t vec4f_add3(vec4f_t a, vec4f_t b, vec4f_t c) {
	return (vec4f_t) {
		a.x + b.x + c.x,
		a.y + b.y + c.y,
        a.z + b.z + c.z,
        a.w + b.w + c.w
	};
}

static inline vec4f_t vec4f_add4(vec4f_t a, vec4f_t b, vec4f_t c, vec4f_t d) {
	return (vec4f_t) {
		a.x + b.x + c.x + d.x,
		a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z,
        a.w + b.w + c.w + d.w
	};
}

static inline vec4f_t vec4f_sub(vec4f_t a, vec4f_t b) {
    return (vec4f_t) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z,
        a.w - b.w
    };
}

static inline vec4f_t vec4f_scalar_mul(vec4f_t a, float b) {
    return (vec4f_t) {
        a.x * b,
        a.y * b,
        a.z * b,
        a.w * b
    };
}

static inline vec4f_t vec4f_component_mul(vec4f_t a, vec4f_t b) {
	return (vec4f_t) {
		a.x * b.x,
		a.y * b.y,
        a.z * b.z,
        a.w * b.w
	};
}

static inline vec4f_t vec4f_component_min(vec4f_t a, vec4f_t b) {
	return (vec4f_t) {
		fminf(a.x, b.x),
		fminf(a.y, b.y),
        fminf(a.z, b.z),
        fminf(a.w, b.w)
	};
}

static inline vec4f_t vec4f_component_max(vec4f_t a, vec4f_t b) {
	return (vec4f_t) {
		fmaxf(a.x, b.x),
		fmaxf(a.y, b.y),
        fmaxf(a.z, b.z),
        fmaxf(a.w, b.w)
	};
}

static inline vec4f_t vec4f_component_floor(vec4f_t a) {
    return (vec4f_t) {
        floorf(a.x),
        floorf(a.y),
        floorf(a.z),
        floorf(a.w)
    };
}

static inline vec4f_t vec4f_component_ceil(vec4f_t a) {
    return (vec4f_t) {
        ceilf(a.x),
        ceilf(a.y),
        ceilf(a.z),
        ceilf(a.w)
    };
}

static inline vec4f_t vec4f_component_abs(vec4f_t a) {
    return (vec4f_t) {
        fabsf(a.x),
        fabsf(a.y),
        fabsf(a.z),
        fabsf(a.w)
    };
}

static inline vec4f_t vec4f_lerp(vec4f_t a, vec4f_t b, float t) {
	return (vec4f_t) {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
	};
}

static inline float vec4f_dot(vec4f_t a, vec4f_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static inline float vec4f_lengthsqr(vec4f_t a) {
    return vec4f_dot(a, a);
}

static inline float vec4f_length(vec4f_t a) {
    return sqrtf(vec4f_lengthsqr(a));
}

static inline vec4f_t vec4f_normalize(vec4f_t a) {
    float len = vec4f_length(a);
    check(len != 0.0f);
    return vec4f_scalar_mul(a, 1.0f / len);
}

static inline vec4f_t vec4f_normalize_safe(vec4f_t a, float tolerance) {
	float len = vec4f_length(a);
	return (len >= tolerance) ? vec4f_scalar_mul(a, 1.0f / len) : vec4f_make_zero();
}

static inline vec4f_t vec4f_negate(vec4f_t a) {
	return (vec4f_t) {
        -a.x,
        -a.y,
        -a.z,
        -a.w
    };
}

static inline bool vec4f_is_nearly_equal(vec4f_t a, vec4f_t b, float tolerance) {
	return vec4f_lengthsqr(vec4f_sub(a, b)) < tolerance * tolerance;
}

static inline bool vec4f_is_equal(vec4f_t a, vec4f_t b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}


// Define vec4f_t array type
#define TEMPLATE_ARRAY_NAME vec4f_array
#define TEMPLATE_ARRAY_TYPE vec4f_t
#include "richc/templates/array.h.template"


#endif // ifndef VEC4F_H_
