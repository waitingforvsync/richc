#ifndef RICHC_MATH_MATH_H_
#define RICHC_MATH_MATH_H_

#include "richc/defines.h"


static inline float deg_to_rad(float degrees) {
    return degrees * 0.0174532925f;
}

#ifndef min
#undef min
#endif

static inline int32_t min(int32_t a, int32_t b) {
    return (a < b) ? a : b;
}

static inline int64_t llmin(int64_t a, int64_t b) {
    return (a < b) ? a : b;
}

#ifndef max
#undef max
#endif

static inline int32_t max(int32_t a, int32_t b) {
    return (a > b) ? a : b;
}

static inline int64_t llmax(int64_t a, int64_t b) {
    return (a > b) ? a : b;
}


static inline int32_t sgn(int32_t a) {
    return (a < 0) ? -1 : (a > 0) ? 1 : 0;
}

static inline int64_t llsgn(int64_t a) {
    return (a < 0) ? -1 : (a > 0) ? 1 : 0;
}


static inline int32_t gcd(int32_t a, int32_t b) {
    while (b != 0) {
        int32_t t = b;
        b = a % b;
        a = t;
    }
    return abs(a);
}

static inline int64_t llgcd(int64_t a, int64_t b) {
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return llabs(a);
}

static inline uint32_t clz(uint32_t a) {
    if (a == 0) {
        return 32;
    }
#ifdef _MSC_VER
    return __lzcnt(a);
#else
    return __builtin_clz(a);
#endif
}

static inline uint32_t llclz(uint64_t a) {
    if (a == 0) {
        return 64;
    }
#ifdef _MSC_VER
    return (uint32_t)__lzcnt64(a);
#else
    return (uint32_t)__builtin_clzll(a);
#endif
}

static inline bool does_mul_overflow_uint64(uint64_t a, uint64_t b) {
    return a != 0 && b > UINT64_MAX / a;
}

static inline bool does_add_overflow_uint64(uint64_t a, uint64_t b) {
    return b > UINT64_MAX - a;
}



#endif // ifndef RICHC_MATH_MATH_H_
