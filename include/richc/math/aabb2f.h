#ifndef RICHC_MATH_AABB2F_H_
#define RICHC_MATH_AABB2F_H_

#include "richc/math/vec2f.h"


typedef struct aabb2f_t {
    vec2f_t min;
    vec2f_t max;
} aabb2f_t;


static inline aabb2f_t aabb2f_make(vec2f_t a, vec2f_t b) {
    return (aabb2f_t) {
        vec2f_component_min(a, b),
        vec2f_component_max(a, b)
    };
}

static inline aabb2f_t aabb2f_make_with_margin(vec2f_t a, vec2f_t b, float margin) {
    return (aabb2f_t) {
        vec2f_sub(vec2f_component_min(a, b), (vec2f_t) {margin, margin}),
        vec2f_add(vec2f_component_max(a, b), (vec2f_t) {margin, margin})
    };
}

static inline bool aabb2f_contains(aabb2f_t a, aabb2f_t b) {
    return a.min.x <= b.min.x && a.min.y <= b.min.y && a.max.x >= b.max.x && a.max.y >= b.max.y;
}

static inline bool aabb2f_intersects(aabb2f_t a, aabb2f_t b) {
    return a.min.x <= b.max.x && a.min.y <= b.max.y && a.max.x >= b.min.x && a.max.y >= b.min.y;
}

static inline bool aabb2f_contains_point(aabb2f_t a, vec2f_t p) {
    return a.min.x <= p.x && a.min.y <= p.y && a.max.x >= p.x && a.max.y >= p.y;
}

static inline aabb2f_t aabb2f_union(aabb2f_t a, aabb2f_t b) {
    return (aabb2f_t) {
        vec2f_component_min(a.min, b.min),
        vec2f_component_max(a.max, b.max)
    };
}

static inline aabb2f_t aabb2f_vec2f_union(aabb2f_t a, vec2f_t b) {
    return (aabb2f_t) {
        vec2f_component_min(a.min, b),
        vec2f_component_max(a.max, b)
    };
}


#endif // RICHC_MATH_AABB2F_H_
