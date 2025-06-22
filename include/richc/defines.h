#ifndef RICHC_DEFINES_H_
#define RICHC_DEFINES_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef _MSC_VER
#define trap() __debugbreak()
#define unreachable() __assume(0)
#else
#define trap() __builtin_trap()
#define unreachable() __builtin_unreachable()
#endif

#define check(cond) ((void)((cond) || (trap(), 0)))
#define require(cond) ((void)((cond) || (abort(), 0)))
#define static_check(cond, msg) _Static_assert(cond, msg)

#define ssizeof(x) ((int32_t)sizeof(x))

#define CONCAT(a, b) CONCAT2_(a, b)
#define CONCAT2_(a, b) a##b

#define INDEX_NONE (-1)


#endif // ifndef RICHC_DEFINES_H_
