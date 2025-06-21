#ifndef VEC2I_H_
#define VEC2I_H_

#include <stdbool.h>
#include <stdint.h>


typedef struct vec2i_t {
    int32_t x, y;
} vec2i_t;


static inline vec2i_t vec2i_make(int32_t x, int32_t y) {
    return (vec2i_t) {x, y};
}

static inline vec2i_t vec2i_make_zero(void) {
    return (vec2i_t) {0, 0};
}

static inline vec2i_t vec2i_make_unitx(void) {
    return (vec2i_t) {1, 0};
}

static inline vec2i_t vec2i_make_unity(void) {
    return (vec2i_t) {0, 1};
}

static inline vec2i_t vec2i_from_int32s(const int32_t *i) {
	return (vec2i_t) {i[0], i[1]};
}

static inline const int32_t *vec2i_as_int32s(const vec2i_t *a) {
    return &a->x;
}


static inline vec2i_t vec2i_add(vec2i_t a, vec2i_t b) {
    return (vec2i_t) {
        a.x + b.x,
        a.y + b.y
    };
}

static inline vec2i_t vec2i_add3(vec2i_t a, vec2i_t b, vec2i_t c) {
	return (vec2i_t) {
		a.x + b.x + c.x,
		a.y + b.y + c.y
	};
}

static inline vec2i_t vec2i_add4(vec2i_t a, vec2i_t b, vec2i_t c, vec2i_t d) {
	return (vec2i_t) {
		a.x + b.x + c.x + d.x,
		a.y + b.y + c.y + d.y
	};
}

static inline vec2i_t vec2i_sub(vec2i_t a, vec2i_t b) {
    return (vec2i_t) {
        a.x - b.x,
        a.y - b.y
    };
}

static inline vec2i_t vec2i_scalar_mul(vec2i_t a, int32_t b) {
    return (vec2i_t) {
        a.x * b,
        a.y * b
    };
}

static inline vec2i_t vec2i_component_mul(vec2i_t a, vec2i_t b) {
	return (vec2i_t) {
		a.x * b.x,
		a.y * b.y
	};
}

static inline vec2i_t vec2i_component_min(vec2i_t a, vec2i_t b) {
	return (vec2i_t) {
		(a.x < b.x) ? a.x : b.x,
		(a.y < b.y) ? a.y : b.y
	};
}

static inline vec2i_t vec2i_component_max(vec2i_t a, vec2i_t b) {
	return (vec2i_t) {
		(a.x > b.x) ? a.x : b.x,
		(a.y > b.y) ? a.y : b.y
	};
}

static inline int32_t vec2i_dot(vec2i_t a, vec2i_t b) {
    return a.x * b.x + a.y * b.y;
}

static inline int32_t vec2i_wedge(vec2i_t a, vec2i_t b) {
    return a.x * b.y - b.x * a.y;
}

static inline vec2i_t vec2i_perp(vec2i_t a) {
    return (vec2i_t) {
        -a.y,
        a.x
    };
}

static inline int32_t vec2i_lengthsqr(vec2i_t a) {
    return vec2i_dot(a, a);
}

static inline vec2i_t vec2i_negate(vec2i_t a) {
	return (vec2i_t) {
        -a.x,
        -a.y
    };
}

static inline bool vec2i_is_equal(vec2i_t a, vec2i_t b) {
	return a.x == b.x && a.y == b.y;
}



#endif // ifndef RICHC_MATH_VEC2I_H_
