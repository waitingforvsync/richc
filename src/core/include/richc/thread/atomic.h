/*
 * thread/atomic.h - lock-free atomic scalars with explicit memory ordering.
 *
 * A hand-rolled replacement for C11 <stdatomic.h> (which is not reliably
 * available, notably under MSVC). Each atomic type wraps a single value in a
 * struct with a private, naturally-aligned member; all access goes through the
 * typed operations below, every one of which takes an explicit rc_memory_order.
 *
 * Types
 * -----
 *   rc_atomic_{u8,i8,u16,i16,u32,i32,u64,i64}  full-width integer atomics
 *   rc_atomic_ptr                               atomic void *
 *   rc_atomic_bool                              atomic bool
 *   rc_atomic_flag                              test-and-set flag (a la C11)
 *
 * Integer operations (each takes an rc_memory_order)
 * --------------------------------------------------
 *   load / store / exchange
 *   compare_exchange_strong / compare_exchange_weak (expected in/out, -> bool)
 *   fetch_add / fetch_sub / fetch_and / fetch_or / fetch_xor  (-> previous value)
 * rc_atomic_ptr and rc_atomic_bool provide load/store/exchange/compare_exchange
 * only (no arithmetic). rc_atomic_flag provides test_and_set / clear.
 *
 * Fences
 * ------
 *   rc_atomic_thread_fence(order)   full inter-thread fence
 *   rc_atomic_signal_fence(order)   compiler-only fence (same thread / handler)
 *
 * Backends
 * --------
 *   clang / gcc (and clang-cl): GNU __atomic_* builtins, full order support.
 *   MSVC (cl.exe): <intrin.h> _Interlocked* intrinsics, one picked per width.
 *   Those intrinsics are full-barrier on x86/x64 and ARM64, so the MSVC path
 *   maps every requested order to sequential consistency: always correct, if
 *   stronger than necessary for relaxed/acquire/release.
 */

#ifndef RC_THREAD_ATOMIC_H_
#define RC_THREAD_ATOMIC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "richc/macros.h"

/* ---- memory ordering ---- */

/*
 * Maps 1:1 onto the C11 / GNU orders (consume is deliberately omitted - it is
 * universally strengthened to acquire in practice).
 */
typedef enum rc_memory_order {
    RC_MEMORY_ORDER_RELAXED,
    RC_MEMORY_ORDER_ACQUIRE,
    RC_MEMORY_ORDER_RELEASE,
    RC_MEMORY_ORDER_ACQ_REL,
    RC_MEMORY_ORDER_SEQ_CST,
} rc_memory_order;

/* ---- backend selection ---- */

#if defined(__clang__) || defined(__GNUC__)
#  define RC_ATOMIC_BUILTINS_ 1
#elif defined(_MSC_VER)
#  ifndef _WIN64
#    error "richc requires a 64-bit target on MSVC (32-bit Windows is not supported)"
#  endif
#  include <intrin.h>
#else
#  error "thread/atomic.h: unsupported compiler (expected clang, gcc, or MSVC)"
#endif

#if defined(RC_ATOMIC_BUILTINS_)

/* Translate an rc_memory_order to the matching GNU __ATOMIC_* constant. */
static inline int rc_atomic_order_(rc_memory_order order)
{
    switch (order) {
        case RC_MEMORY_ORDER_RELAXED: return __ATOMIC_RELAXED;
        case RC_MEMORY_ORDER_ACQUIRE: return __ATOMIC_ACQUIRE;
        case RC_MEMORY_ORDER_RELEASE: return __ATOMIC_RELEASE;
        case RC_MEMORY_ORDER_ACQ_REL: return __ATOMIC_ACQ_REL;
        case RC_MEMORY_ORDER_SEQ_CST: return __ATOMIC_SEQ_CST;
    }
    RC_UNREACHABLE();
}

#endif


/* ---- integer atomic types ---- */

/* ---- rc_atomic_u8 ---- */

typedef struct rc_atomic_u8 {
    _Alignas(sizeof(uint8_t)) uint8_t value_;
} rc_atomic_u8;

static inline uint8_t rc_atomic_u8_load(const rc_atomic_u8 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((uint8_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedOr8((char volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_u8_store(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange8((char volatile *)&a->value_, (char)v);
#endif
}

static inline uint8_t rc_atomic_u8_exchange(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedExchange8((char volatile *)&a->value_, (char)v);
#endif
}

static inline bool rc_atomic_u8_compare_exchange_strong(rc_atomic_u8 *a, uint8_t *expected, uint8_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    char want = (char)*expected;
    char prev = _InterlockedCompareExchange8((char volatile *)&a->value_, (char)desired, want);
    if (prev == want)
        return true;
    *expected = (uint8_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_u8_compare_exchange_weak(rc_atomic_u8 *a, uint8_t *expected, uint8_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_u8_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline uint8_t rc_atomic_u8_fetch_add(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedExchangeAdd8((char volatile *)&a->value_, (char)v);
#endif
}

static inline uint8_t rc_atomic_u8_fetch_sub(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedExchangeAdd8((char volatile *)&a->value_, (char)(0u - (uint8_t)v));
#endif
}

static inline uint8_t rc_atomic_u8_fetch_and(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedAnd8((char volatile *)&a->value_, (char)v);
#endif
}

static inline uint8_t rc_atomic_u8_fetch_or(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedOr8((char volatile *)&a->value_, (char)v);
#endif
}

static inline uint8_t rc_atomic_u8_fetch_xor(rc_atomic_u8 *a, uint8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint8_t)_InterlockedXor8((char volatile *)&a->value_, (char)v);
#endif
}

/* ---- rc_atomic_i8 ---- */

typedef struct rc_atomic_i8 {
    _Alignas(sizeof(int8_t)) int8_t value_;
} rc_atomic_i8;

static inline int8_t rc_atomic_i8_load(const rc_atomic_i8 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((int8_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedOr8((char volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_i8_store(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange8((char volatile *)&a->value_, (char)v);
#endif
}

static inline int8_t rc_atomic_i8_exchange(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedExchange8((char volatile *)&a->value_, (char)v);
#endif
}

static inline bool rc_atomic_i8_compare_exchange_strong(rc_atomic_i8 *a, int8_t *expected, int8_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    char want = (char)*expected;
    char prev = _InterlockedCompareExchange8((char volatile *)&a->value_, (char)desired, want);
    if (prev == want)
        return true;
    *expected = (int8_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_i8_compare_exchange_weak(rc_atomic_i8 *a, int8_t *expected, int8_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_i8_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline int8_t rc_atomic_i8_fetch_add(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedExchangeAdd8((char volatile *)&a->value_, (char)v);
#endif
}

static inline int8_t rc_atomic_i8_fetch_sub(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedExchangeAdd8((char volatile *)&a->value_, (char)(0u - (uint8_t)v));
#endif
}

static inline int8_t rc_atomic_i8_fetch_and(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedAnd8((char volatile *)&a->value_, (char)v);
#endif
}

static inline int8_t rc_atomic_i8_fetch_or(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedOr8((char volatile *)&a->value_, (char)v);
#endif
}

static inline int8_t rc_atomic_i8_fetch_xor(rc_atomic_i8 *a, int8_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int8_t)_InterlockedXor8((char volatile *)&a->value_, (char)v);
#endif
}

/* ---- rc_atomic_u16 ---- */

typedef struct rc_atomic_u16 {
    _Alignas(sizeof(uint16_t)) uint16_t value_;
} rc_atomic_u16;

static inline uint16_t rc_atomic_u16_load(const rc_atomic_u16 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((uint16_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedOr16((short volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_u16_store(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange16((short volatile *)&a->value_, (short)v);
#endif
}

static inline uint16_t rc_atomic_u16_exchange(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedExchange16((short volatile *)&a->value_, (short)v);
#endif
}

static inline bool rc_atomic_u16_compare_exchange_strong(rc_atomic_u16 *a, uint16_t *expected, uint16_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    short want = (short)*expected;
    short prev = _InterlockedCompareExchange16((short volatile *)&a->value_, (short)desired, want);
    if (prev == want)
        return true;
    *expected = (uint16_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_u16_compare_exchange_weak(rc_atomic_u16 *a, uint16_t *expected, uint16_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_u16_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline uint16_t rc_atomic_u16_fetch_add(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedExchangeAdd16((short volatile *)&a->value_, (short)v);
#endif
}

static inline uint16_t rc_atomic_u16_fetch_sub(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedExchangeAdd16((short volatile *)&a->value_, (short)(0u - (uint16_t)v));
#endif
}

static inline uint16_t rc_atomic_u16_fetch_and(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedAnd16((short volatile *)&a->value_, (short)v);
#endif
}

static inline uint16_t rc_atomic_u16_fetch_or(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedOr16((short volatile *)&a->value_, (short)v);
#endif
}

static inline uint16_t rc_atomic_u16_fetch_xor(rc_atomic_u16 *a, uint16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint16_t)_InterlockedXor16((short volatile *)&a->value_, (short)v);
#endif
}

/* ---- rc_atomic_i16 ---- */

typedef struct rc_atomic_i16 {
    _Alignas(sizeof(int16_t)) int16_t value_;
} rc_atomic_i16;

static inline int16_t rc_atomic_i16_load(const rc_atomic_i16 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((int16_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedOr16((short volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_i16_store(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange16((short volatile *)&a->value_, (short)v);
#endif
}

static inline int16_t rc_atomic_i16_exchange(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedExchange16((short volatile *)&a->value_, (short)v);
#endif
}

static inline bool rc_atomic_i16_compare_exchange_strong(rc_atomic_i16 *a, int16_t *expected, int16_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    short want = (short)*expected;
    short prev = _InterlockedCompareExchange16((short volatile *)&a->value_, (short)desired, want);
    if (prev == want)
        return true;
    *expected = (int16_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_i16_compare_exchange_weak(rc_atomic_i16 *a, int16_t *expected, int16_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_i16_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline int16_t rc_atomic_i16_fetch_add(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedExchangeAdd16((short volatile *)&a->value_, (short)v);
#endif
}

static inline int16_t rc_atomic_i16_fetch_sub(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedExchangeAdd16((short volatile *)&a->value_, (short)(0u - (uint16_t)v));
#endif
}

static inline int16_t rc_atomic_i16_fetch_and(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedAnd16((short volatile *)&a->value_, (short)v);
#endif
}

static inline int16_t rc_atomic_i16_fetch_or(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedOr16((short volatile *)&a->value_, (short)v);
#endif
}

static inline int16_t rc_atomic_i16_fetch_xor(rc_atomic_i16 *a, int16_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int16_t)_InterlockedXor16((short volatile *)&a->value_, (short)v);
#endif
}

/* ---- rc_atomic_u32 ---- */

typedef struct rc_atomic_u32 {
    _Alignas(sizeof(uint32_t)) uint32_t value_;
} rc_atomic_u32;

static inline uint32_t rc_atomic_u32_load(const rc_atomic_u32 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((uint32_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedOr((long volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_u32_store(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange((long volatile *)&a->value_, (long)v);
#endif
}

static inline uint32_t rc_atomic_u32_exchange(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedExchange((long volatile *)&a->value_, (long)v);
#endif
}

static inline bool rc_atomic_u32_compare_exchange_strong(rc_atomic_u32 *a, uint32_t *expected, uint32_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    long want = (long)*expected;
    long prev = _InterlockedCompareExchange((long volatile *)&a->value_, (long)desired, want);
    if (prev == want)
        return true;
    *expected = (uint32_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_u32_compare_exchange_weak(rc_atomic_u32 *a, uint32_t *expected, uint32_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_u32_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline uint32_t rc_atomic_u32_fetch_add(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedExchangeAdd((long volatile *)&a->value_, (long)v);
#endif
}

static inline uint32_t rc_atomic_u32_fetch_sub(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedExchangeAdd((long volatile *)&a->value_, (long)(0u - (uint32_t)v));
#endif
}

static inline uint32_t rc_atomic_u32_fetch_and(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedAnd((long volatile *)&a->value_, (long)v);
#endif
}

static inline uint32_t rc_atomic_u32_fetch_or(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedOr((long volatile *)&a->value_, (long)v);
#endif
}

static inline uint32_t rc_atomic_u32_fetch_xor(rc_atomic_u32 *a, uint32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint32_t)_InterlockedXor((long volatile *)&a->value_, (long)v);
#endif
}

/* ---- rc_atomic_i32 ---- */

typedef struct rc_atomic_i32 {
    _Alignas(sizeof(int32_t)) int32_t value_;
} rc_atomic_i32;

static inline int32_t rc_atomic_i32_load(const rc_atomic_i32 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((int32_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedOr((long volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_i32_store(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange((long volatile *)&a->value_, (long)v);
#endif
}

static inline int32_t rc_atomic_i32_exchange(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedExchange((long volatile *)&a->value_, (long)v);
#endif
}

static inline bool rc_atomic_i32_compare_exchange_strong(rc_atomic_i32 *a, int32_t *expected, int32_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    long want = (long)*expected;
    long prev = _InterlockedCompareExchange((long volatile *)&a->value_, (long)desired, want);
    if (prev == want)
        return true;
    *expected = (int32_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_i32_compare_exchange_weak(rc_atomic_i32 *a, int32_t *expected, int32_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_i32_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline int32_t rc_atomic_i32_fetch_add(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedExchangeAdd((long volatile *)&a->value_, (long)v);
#endif
}

static inline int32_t rc_atomic_i32_fetch_sub(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedExchangeAdd((long volatile *)&a->value_, (long)(0u - (uint32_t)v));
#endif
}

static inline int32_t rc_atomic_i32_fetch_and(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedAnd((long volatile *)&a->value_, (long)v);
#endif
}

static inline int32_t rc_atomic_i32_fetch_or(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedOr((long volatile *)&a->value_, (long)v);
#endif
}

static inline int32_t rc_atomic_i32_fetch_xor(rc_atomic_i32 *a, int32_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int32_t)_InterlockedXor((long volatile *)&a->value_, (long)v);
#endif
}

/* ---- rc_atomic_u64 ---- */

typedef struct rc_atomic_u64 {
    _Alignas(sizeof(uint64_t)) uint64_t value_;
} rc_atomic_u64;

static inline uint64_t rc_atomic_u64_load(const rc_atomic_u64 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((uint64_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedOr64((__int64 volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_u64_store(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline uint64_t rc_atomic_u64_exchange(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedExchange64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline bool rc_atomic_u64_compare_exchange_strong(rc_atomic_u64 *a, uint64_t *expected, uint64_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    __int64 want = (__int64)*expected;
    __int64 prev = _InterlockedCompareExchange64((__int64 volatile *)&a->value_, (__int64)desired, want);
    if (prev == want)
        return true;
    *expected = (uint64_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_u64_compare_exchange_weak(rc_atomic_u64 *a, uint64_t *expected, uint64_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_u64_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline uint64_t rc_atomic_u64_fetch_add(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedExchangeAdd64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline uint64_t rc_atomic_u64_fetch_sub(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedExchangeAdd64((__int64 volatile *)&a->value_, (__int64)(0u - (uint64_t)v));
#endif
}

static inline uint64_t rc_atomic_u64_fetch_and(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedAnd64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline uint64_t rc_atomic_u64_fetch_or(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedOr64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline uint64_t rc_atomic_u64_fetch_xor(rc_atomic_u64 *a, uint64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (uint64_t)_InterlockedXor64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

/* ---- rc_atomic_i64 ---- */

typedef struct rc_atomic_i64 {
    _Alignas(sizeof(int64_t)) int64_t value_;
} rc_atomic_i64;

static inline int64_t rc_atomic_i64_load(const rc_atomic_i64 *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((int64_t *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedOr64((__int64 volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_i64_store(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline int64_t rc_atomic_i64_exchange(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedExchange64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline bool rc_atomic_i64_compare_exchange_strong(rc_atomic_i64 *a, int64_t *expected, int64_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    __int64 want = (__int64)*expected;
    __int64 prev = _InterlockedCompareExchange64((__int64 volatile *)&a->value_, (__int64)desired, want);
    if (prev == want)
        return true;
    *expected = (int64_t)prev;
    return false;
#endif
}

static inline bool rc_atomic_i64_compare_exchange_weak(rc_atomic_i64 *a, int64_t *expected, int64_t desired,
    rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_i64_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

static inline int64_t rc_atomic_i64_fetch_add(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_add(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedExchangeAdd64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline int64_t rc_atomic_i64_fetch_sub(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_sub(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedExchangeAdd64((__int64 volatile *)&a->value_, (__int64)(0u - (uint64_t)v));
#endif
}

static inline int64_t rc_atomic_i64_fetch_and(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_and(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedAnd64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline int64_t rc_atomic_i64_fetch_or(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_or(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedOr64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

static inline int64_t rc_atomic_i64_fetch_xor(rc_atomic_i64 *a, int64_t v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_fetch_xor(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (int64_t)_InterlockedXor64((__int64 volatile *)&a->value_, (__int64)v);
#endif
}

/* ---- atomic pointer ---- */

typedef struct rc_atomic_ptr {
    _Alignas(sizeof(void *)) void *value_;
} rc_atomic_ptr;

static inline void *rc_atomic_ptr_load(const rc_atomic_ptr *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((void **)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return _InterlockedCompareExchangePointer((void *volatile *)&a->value_, NULL, NULL);
#endif
}

static inline void rc_atomic_ptr_store(rc_atomic_ptr *a, void *v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchangePointer((void *volatile *)&a->value_, v);
#endif
}

static inline void *rc_atomic_ptr_exchange(rc_atomic_ptr *a, void *v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return _InterlockedExchangePointer((void *volatile *)&a->value_, v);
#endif
}

static inline bool rc_atomic_ptr_compare_exchange_strong(rc_atomic_ptr *a, void **expected,
    void *desired, rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    void *want = *expected;
    void *prev = _InterlockedCompareExchangePointer((void *volatile *)&a->value_, desired, want);
    if (prev == want)
        return true;
    *expected = prev;
    return false;
#endif
}

static inline bool rc_atomic_ptr_compare_exchange_weak(rc_atomic_ptr *a, void **expected,
    void *desired, rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, true,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    return rc_atomic_ptr_compare_exchange_strong(a, expected, desired, success, failure);
#endif
}

/* ---- atomic bool ---- */

typedef struct rc_atomic_bool {
    bool value_;
} rc_atomic_bool;

static inline bool rc_atomic_bool_load(const rc_atomic_bool *a, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_load_n((bool *)&a->value_, rc_atomic_order_(order));
#else
    (void)order;
    return (bool)_InterlockedOr8((char volatile *)&a->value_, 0);
#endif
}

static inline void rc_atomic_bool_store(rc_atomic_bool *a, bool v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange8((char volatile *)&a->value_, (char)v);
#endif
}

static inline bool rc_atomic_bool_exchange(rc_atomic_bool *a, bool v, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&a->value_, v, rc_atomic_order_(order));
#else
    (void)order;
    return (bool)_InterlockedExchange8((char volatile *)&a->value_, (char)v);
#endif
}

static inline bool rc_atomic_bool_compare_exchange_strong(rc_atomic_bool *a, bool *expected,
    bool desired, rc_memory_order success, rc_memory_order failure)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_compare_exchange_n(&a->value_, expected, desired, false,
        rc_atomic_order_(success), rc_atomic_order_(failure));
#else
    (void)success;
    (void)failure;
    char want = (char)*expected;
    char prev = _InterlockedCompareExchange8((char volatile *)&a->value_, (char)desired, want);
    if (prev == want)
        return true;
    *expected = (bool)prev;
    return false;
#endif
}

/* ---- atomic flag (test-and-set) ---- */

typedef struct rc_atomic_flag {
    bool value_;
} rc_atomic_flag;

/* Set the flag and return its previous value. */
static inline bool rc_atomic_flag_test_and_set(rc_atomic_flag *f, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    return __atomic_exchange_n(&f->value_, true, rc_atomic_order_(order));
#else
    (void)order;
    return (bool)_InterlockedExchange8((char volatile *)&f->value_, (char)1);
#endif
}

/* Clear the flag. */
static inline void rc_atomic_flag_clear(rc_atomic_flag *f, rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_store_n(&f->value_, false, rc_atomic_order_(order));
#else
    (void)order;
    (void)_InterlockedExchange8((char volatile *)&f->value_, (char)0);
#endif
}

/* ---- fences ---- */

/* Full inter-thread memory fence. */
static inline void rc_atomic_thread_fence(rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_thread_fence(rc_atomic_order_(order));
#else
    // A locked interlocked op on a throwaway location is a full memory barrier
    // on every supported architecture (MemoryBarrier lives in <windows.h>,
    // which this header must not pull in).
    (void)order;
    long barrier = 0;
    (void)_InterlockedOr(&barrier, 0);
#endif
}

/* Compiler-only fence (orders against signal handlers / same-thread reordering). */
static inline void rc_atomic_signal_fence(rc_memory_order order)
{
#if defined(RC_ATOMIC_BUILTINS_)
    __atomic_signal_fence(rc_atomic_order_(order));
#else
    (void)order;
    _ReadWriteBarrier();
#endif
}

#endif /* RC_THREAD_ATOMIC_H_ */
