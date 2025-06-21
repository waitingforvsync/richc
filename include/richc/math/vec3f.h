#ifndef RICHC_MATH_VEC3F_H_
#define RICHC_MATH_VEC3F_H_

#include "richc/math/vec2f.h"


typedef struct vec3f_t {
    float x, y, z;
} vec3f_t;


// Make vec3f

static inline vec3f_t vec3f_make(float x, float y, float z) {
    return (vec3f_t) {x, y, z};
}

static inline vec3f_t vec3f_make_zero(void) {
    return (vec3f_t) {0.0f, 0.0f, 0.0f};
}

static inline vec3f_t vec3f_make_unitx(void) {
    return (vec3f_t) {1.0f, 0.0f, 0.0f};
}

static inline vec3f_t vec3f_make_unity(void) {
    return (vec3f_t) {0.0f, 1.0f, 0.0f};
}

static inline vec3f_t vec3f_make_unitz(void) {
    return (vec3f_t) {0.0f, 0.0f, 1.0f};
}


// Make vec3f from other types

static inline vec3f_t vec3f_from_floats(const float *f) {
	return (vec3f_t) {f[0], f[1], f[2]};
}

static inline vec3f_t vec3f_from_vec2f(vec2f_t v, float z) {
    return (vec3f_t) {v.x, v.y, z};
}


// Convert vec3f to other types

static inline const float *vec3f_as_floats(const vec3f_t *a) {
    return &a->x;
}


// Arithmetic ops

static inline vec3f_t vec3f_add(vec3f_t a, vec3f_t b) {
    return (vec3f_t) {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

static inline vec3f_t vec3f_add3(vec3f_t a, vec3f_t b, vec3f_t c) {
	return (vec3f_t) {
		a.x + b.x + c.x,
		a.y + b.y + c.y,
        a.z + b.z + c.z
	};
}

static inline vec3f_t vec3f_add4(vec3f_t a, vec3f_t b, vec3f_t c, vec3f_t d) {
	return (vec3f_t) {
		a.x + b.x + c.x + d.x,
		a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z
	};
}

static inline vec3f_t vec3f_sub(vec3f_t a, vec3f_t b) {
    return (vec3f_t) {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

static inline vec3f_t vec3f_scalar_mul(vec3f_t a, float b) {
    return (vec3f_t) {
        a.x * b,
        a.y * b,
        a.z * b
    };
}

static inline vec3f_t vec3f_component_mul(vec3f_t a, vec3f_t b) {
	return (vec3f_t) {
		a.x * b.x,
		a.y * b.y,
        a.z * b.z
	};
}

static inline vec3f_t vec3f_component_min(vec3f_t a, vec3f_t b) {
	return (vec3f_t) {
		fminf(a.x, b.x),
		fminf(a.y, b.y),
        fminf(a.z, b.z)
	};
}

static inline vec3f_t vec3f_component_max(vec3f_t a, vec3f_t b) {
	return (vec3f_t) {
		fmaxf(a.x, b.x),
		fmaxf(a.y, b.y),
        fmaxf(a.z, b.z)
	};
}

static inline vec3f_t vec3f_component_floor(vec3f_t a) {
    return (vec3f_t) {
        floorf(a.x),
        floorf(a.y),
        floorf(a.z)
    };
}

static inline vec3f_t vec3f_component_ceil(vec3f_t a) {
    return (vec3f_t) {
        ceilf(a.x),
        ceilf(a.y),
        ceilf(a.z)
    };
}

static inline vec3f_t vec3f_component_abs(vec3f_t a) {
    return (vec3f_t) {
        fabsf(a.x),
        fabsf(a.y),
        fabsf(a.z)
    };
}

static inline vec3f_t vec3f_lerp(vec3f_t a, vec3f_t b, float t) {
	return (vec3f_t) {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
	};
}

static inline float vec3f_dot(vec3f_t a, vec3f_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3f_t vec3f_cross(vec3f_t a, vec3f_t b) {
    return (vec3f_t) {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float vec3f_lengthsqr(vec3f_t a) {
    return vec3f_dot(a, a);
}

static inline float vec3f_length(vec3f_t a) {
    return sqrtf(vec3f_lengthsqr(a));
}

static inline vec3f_t vec3f_normalize(vec3f_t a) {
    float len = vec3f_length(a);
    check(len != 0.0f);
    return vec3f_scalar_mul(a, 1.0f / len);
}

static inline vec3f_t vec3f_normalize_safe(vec3f_t a, float tolerance) {
	float len = vec3f_length(a);
	return (len >= tolerance) ? vec3f_scalar_mul(a, 1.0f / len) : vec3f_make_zero();
}

static inline vec3f_t vec3f_negate(vec3f_t a) {
	return (vec3f_t) {
        -a.x,
        -a.y,
        -a.z
    };
}

static inline bool vec3f_is_nearly_equal(vec3f_t a, vec3f_t b, float tolerance) {
	return vec3f_lengthsqr(vec3f_sub(a, b)) < tolerance * tolerance;
}

static inline bool vec3f_is_equal(vec3f_t a, vec3f_t b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}


// Define vec3f_t array type
#define TEMPLATE_ARRAY_NAME vec3f_array
#define TEMPLATE_ARRAY_TYPE vec3f_t
#include "richc/templates/array.h.template"


#endif // ifndef VEC3F_H_
