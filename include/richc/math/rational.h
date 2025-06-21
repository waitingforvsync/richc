#ifndef RICHC_MATH_RATIONAL_H_
#define RICHC_MATH_RATIONAL_H_

#include "richc/math/math.h"


typedef struct rational_t {
    int64_t num;
    int64_t denom;
} rational_t;



static inline rational_t rational_make(int64_t num, int64_t denom) {
    if (denom == 0) {
        return (rational_t) {0};
    }
    if (denom < 0) {
        num = -num;
        denom = -denom;
    }
    int64_t d = llgcd(num, denom);
    return (rational_t){
        num / d,
        denom / d
    };
}

static inline rational_t rational_from_int(int64_t num) {
    return (rational_t) {num, 1};
}

static inline bool rational_is_valid(rational_t a) {
    return a.denom > 0 && llgcd(a.num, a.denom) == 1;
}

static inline rational_t rational_int_mul(rational_t a, int64_t b) {
    check(a.denom > 0);
    return rational_make(a.num * b, a.denom);
}

static inline rational_t rational_mul(rational_t a, rational_t b) {
    check(a.denom > 0);
    check(b.denom > 0);
    return rational_make(a.num * b.num, a.denom * b.denom);
}

static inline rational_t rational_int_div(rational_t a, int64_t b) {
    check(a.denom > 0);
    check(b != 0);
    return rational_make(a.num * llsgn(b), a.denom * b);
}

static inline rational_t rational_div(rational_t a, rational_t b) {
    check(a.denom > 0);
    check(b.num != 0);
    check(b.denom > 0);
    return rational_make(a.num * b.denom * llsgn(b.num), a.denom * llabs(b.num));
}

static inline rational_t rational_int_add(rational_t a, int64_t b) {
    check(a.denom > 0);
    return (rational_t) {
        a.num + b * a.denom,
        a.denom
    };
}

static inline rational_t rational_add(rational_t a, rational_t b) {
    check(a.denom > 0);
    check(b.denom > 0);
    int64_t d = llgcd(a.denom, b.denom);
    int64_t da = a.denom / d;
    int64_t db = b.denom / d;
    return (rational_t) {
        a.num * db + b.num * da,
        da * b.denom
    };
}

static inline rational_t rational_int_sub(rational_t a, int64_t b) {
    check(a.denom > 0);
    return (rational_t) {
        a.num - b * a.denom,
        a.denom
    };
}

static inline rational_t rational_sub(rational_t a, rational_t b) {
    check(a.denom > 0);
    check(b.denom > 0);
    int64_t d = llgcd(a.denom, b.denom);
    int64_t da = a.denom / d;
    int64_t db = b.denom / d;
    return (rational_t) {
        a.num * db - b.num * da,
        da * b.denom
    };
}


#endif // ifndef RICHC_MATH_RATIONAL_H_
