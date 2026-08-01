#include "richc/thread/atomic.h"

#include "richc/test.h"

/*
 * Exercise the full integer operation set for one type at a given memory order.
 * All values are chosen to stay within every type's range (including int8_t).
 */
#define CHECK_ATOMIC_INT(suffix, T, mo)                                                   \
    do {                                                                                  \
        rc_atomic_##suffix a = {0};                                                       \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)0);                            \
        rc_atomic_##suffix##_store(&a, (T)5, mo);                                          \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)5);                            \
        RC_CHECK(rc_atomic_##suffix##_exchange(&a, (T)9, mo), ==, (T)5);                  \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)9);                            \
        RC_CHECK(rc_atomic_##suffix##_fetch_add(&a, (T)3, mo), ==, (T)9);                 \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)12);                           \
        RC_CHECK(rc_atomic_##suffix##_fetch_sub(&a, (T)2, mo), ==, (T)12);                \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)10);                           \
        rc_atomic_##suffix##_store(&a, (T)0xC, mo);                                        \
        RC_CHECK(rc_atomic_##suffix##_fetch_and(&a, (T)0xA, mo), ==, (T)0xC);             \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)0x8);                          \
        RC_CHECK(rc_atomic_##suffix##_fetch_or(&a, (T)0x1, mo), ==, (T)0x8);              \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)0x9);                          \
        RC_CHECK(rc_atomic_##suffix##_fetch_xor(&a, (T)0xF, mo), ==, (T)0x9);             \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)0x6);                          \
        T expected = (T)0x6;                                                              \
        RC_CHECK_TRUE(rc_atomic_##suffix##_compare_exchange_strong(&a, &expected,         \
            (T)0x20, mo, RC_MEMORY_ORDER_RELAXED));                                       \
        RC_CHECK(rc_atomic_##suffix##_load(&a, mo), ==, (T)0x20);                         \
        T bad = (T)0x11;                                                                  \
        RC_CHECK_FALSE(rc_atomic_##suffix##_compare_exchange_strong(&a, &bad,             \
            (T)0x1, mo, RC_MEMORY_ORDER_RELAXED));                                        \
        RC_CHECK(bad, ==, (T)0x20);                                                       \
    } while (0)

RC_TEST(atomic, int_seq_cst)
{
    CHECK_ATOMIC_INT(u8,  uint8_t,  RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(i8,  int8_t,   RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(u16, uint16_t, RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(i16, int16_t,  RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(u32, uint32_t, RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(i32, int32_t,  RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(u64, uint64_t, RC_MEMORY_ORDER_SEQ_CST);
    CHECK_ATOMIC_INT(i64, int64_t,  RC_MEMORY_ORDER_SEQ_CST);
}

RC_TEST(atomic, int_relaxed)
{
    CHECK_ATOMIC_INT(u8,  uint8_t,  RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(i8,  int8_t,   RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(u16, uint16_t, RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(i16, int16_t,  RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(u32, uint32_t, RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(i32, int32_t,  RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(u64, uint64_t, RC_MEMORY_ORDER_RELAXED);
    CHECK_ATOMIC_INT(i64, int64_t,  RC_MEMORY_ORDER_RELAXED);
}

RC_TEST(atomic, wraparound)
{
    // Unsigned fetch_add/sub wrap modulo 2^width; signed store/load round-trip extremes.
    rc_atomic_u8 a = {0};
    rc_atomic_u8_store(&a, 0xFF, RC_MEMORY_ORDER_RELAXED);
    RC_CHECK(rc_atomic_u8_fetch_add(&a, 2, RC_MEMORY_ORDER_RELAXED), ==, (uint8_t)0xFF);
    RC_CHECK(rc_atomic_u8_load(&a, RC_MEMORY_ORDER_RELAXED), ==, (uint8_t)1);

    rc_atomic_i64 b = {0};
    rc_atomic_i64_store(&b, INT64_MIN, RC_MEMORY_ORDER_RELAXED);
    RC_CHECK(rc_atomic_i64_load(&b, RC_MEMORY_ORDER_RELAXED), ==, INT64_MIN);
    rc_atomic_i64_store(&b, INT64_MAX, RC_MEMORY_ORDER_RELAXED);
    RC_CHECK(rc_atomic_i64_load(&b, RC_MEMORY_ORDER_RELAXED), ==, INT64_MAX);
}

RC_TEST(atomic, ptr)
{
    int x = 0;
    int y = 0;
    rc_atomic_ptr p = {0};
    RC_CHECK_TRUE(rc_atomic_ptr_load(&p, RC_MEMORY_ORDER_SEQ_CST) == NULL);
    rc_atomic_ptr_store(&p, &x, RC_MEMORY_ORDER_SEQ_CST);
    RC_CHECK_TRUE(rc_atomic_ptr_load(&p, RC_MEMORY_ORDER_SEQ_CST) == &x);
    RC_CHECK_TRUE(rc_atomic_ptr_exchange(&p, &y, RC_MEMORY_ORDER_SEQ_CST) == &x);

    void *expected = &y;
    RC_CHECK_TRUE(rc_atomic_ptr_compare_exchange_strong(&p, &expected, &x,
        RC_MEMORY_ORDER_SEQ_CST, RC_MEMORY_ORDER_RELAXED));
    RC_CHECK_TRUE(rc_atomic_ptr_load(&p, RC_MEMORY_ORDER_SEQ_CST) == &x);

    void *bad = NULL;
    RC_CHECK_FALSE(rc_atomic_ptr_compare_exchange_strong(&p, &bad, &y,
        RC_MEMORY_ORDER_SEQ_CST, RC_MEMORY_ORDER_RELAXED));
    RC_CHECK_TRUE(bad == &x);
}

RC_TEST(atomic, boolean)
{
    rc_atomic_bool b = {0};
    RC_CHECK(rc_atomic_bool_load(&b, RC_MEMORY_ORDER_SEQ_CST), ==, false);
    rc_atomic_bool_store(&b, true, RC_MEMORY_ORDER_SEQ_CST);
    RC_CHECK(rc_atomic_bool_load(&b, RC_MEMORY_ORDER_SEQ_CST), ==, true);
    RC_CHECK(rc_atomic_bool_exchange(&b, false, RC_MEMORY_ORDER_SEQ_CST), ==, true);

    bool expected = false;
    RC_CHECK_TRUE(rc_atomic_bool_compare_exchange_strong(&b, &expected, true,
        RC_MEMORY_ORDER_SEQ_CST, RC_MEMORY_ORDER_RELAXED));
    RC_CHECK(rc_atomic_bool_load(&b, RC_MEMORY_ORDER_SEQ_CST), ==, true);

    bool bad = false;
    RC_CHECK_FALSE(rc_atomic_bool_compare_exchange_strong(&b, &bad, false,
        RC_MEMORY_ORDER_SEQ_CST, RC_MEMORY_ORDER_RELAXED));
    RC_CHECK(bad, ==, true);
}

RC_TEST(atomic, flag)
{
    rc_atomic_flag f = {0};
    RC_CHECK(rc_atomic_flag_test_and_set(&f, RC_MEMORY_ORDER_SEQ_CST), ==, false);
    RC_CHECK(rc_atomic_flag_test_and_set(&f, RC_MEMORY_ORDER_SEQ_CST), ==, true);
    rc_atomic_flag_clear(&f, RC_MEMORY_ORDER_SEQ_CST);
    RC_CHECK(rc_atomic_flag_test_and_set(&f, RC_MEMORY_ORDER_SEQ_CST), ==, false);
}

RC_TEST(atomic, compare_exchange_weak_loop)
{
    // weak may fail spuriously; a retry loop must still converge.
    rc_atomic_u32 a = {0};
    rc_atomic_u32_store(&a, 100, RC_MEMORY_ORDER_RELAXED);
    uint32_t expected = 100;
    while (!rc_atomic_u32_compare_exchange_weak(&a, &expected, 200,
            RC_MEMORY_ORDER_SEQ_CST, RC_MEMORY_ORDER_RELAXED)) {
        // expected is refreshed by the call on failure; here it never changes.
    }
    RC_CHECK(rc_atomic_u32_load(&a, RC_MEMORY_ORDER_SEQ_CST), ==, 200u);
}

RC_TEST(atomic, fences)
{
    // Fences have no observable single-threaded effect; verify they compile and run.
    rc_atomic_thread_fence(RC_MEMORY_ORDER_ACQUIRE);
    rc_atomic_thread_fence(RC_MEMORY_ORDER_RELEASE);
    rc_atomic_thread_fence(RC_MEMORY_ORDER_SEQ_CST);
    rc_atomic_signal_fence(RC_MEMORY_ORDER_ACQ_REL);
    RC_CHECK_TRUE(true);
}
