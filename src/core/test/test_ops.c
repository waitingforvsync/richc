#include "richc/ops.h"
#include "richc/test.h"

RC_TEST(ops, bitcast)
{
    RC_CHECK(rc_bitcast_f32(1.0f), ==, 0x3F800000u);          // IEEE-754 bits of 1.0f
    RC_CHECK(rc_bitcast_f32(0.0f), ==, 0u);
    RC_CHECK(rc_bitcast_f64(1.0), ==, 0x3FF0000000000000ull);
    RC_CHECK(rc_bitcast_f64(0.0), ==, 0ull);
}

RC_TEST(ops, min_max_sgn)
{
    RC_CHECK(rc_min_i32(3, 7), ==, 3);
    RC_CHECK(rc_max_i32(3, 7), ==, 7);
    RC_CHECK(rc_min_i64(-5, 2), ==, -5);
    RC_CHECK(rc_max_i64(-5, 2), ==, 2);
    RC_CHECK(rc_min_f32(-1.5f, 2.0f), ==, -1.5f);
    RC_CHECK(rc_max_f32(-1.5f, 2.0f), ==, 2.0f);
    RC_CHECK(rc_min_f64(3.0, 3.0), ==, 3.0);
    RC_CHECK(rc_max_f64(-2.0, -7.0), ==, -2.0);
    RC_CHECK(rc_sgn_i32(-9), ==, -1);
    RC_CHECK(rc_sgn_i32(0), ==, 0);
    RC_CHECK(rc_sgn_i32(9), ==, 1);
    RC_CHECK(rc_sgn_i64(-1), ==, -1);
}

RC_TEST(ops, gcd)
{
    RC_CHECK(rc_gcd_i32(12, 8), ==, 4);
    RC_CHECK(rc_gcd_i32(0, 5), ==, 5);
    RC_CHECK(rc_gcd_i32(-12, 8), ==, 4);     // always non-negative
    RC_CHECK(rc_gcd_i32(17, 5), ==, 1);      // coprime
    RC_CHECK(rc_gcd_i64(1000000000000ll, 500000000000ll), ==, 500000000000ll);
}

RC_TEST(ops, clz)
{
    RC_CHECK(rc_clz_u32(0u), ==, 32u);
    RC_CHECK(rc_clz_u32(1u), ==, 31u);
    RC_CHECK(rc_clz_u32(0x80000000u), ==, 0u);
    RC_CHECK(rc_clz_u64(0ull), ==, 64u);
    RC_CHECK(rc_clz_u64(1ull), ==, 63u);
    RC_CHECK(rc_clz_u64(0x8000000000000000ull), ==, 0u);
}

RC_TEST(ops, ctz)
{
    RC_CHECK(rc_ctz_u32(0u), ==, 32u);
    RC_CHECK(rc_ctz_u32(1u), ==, 0u);
    RC_CHECK(rc_ctz_u32(0x80000000u), ==, 31u);
    RC_CHECK(rc_ctz_u32(0x18u), ==, 3u);            // lowest set bit at 3
    RC_CHECK(rc_ctz_u64(0ull), ==, 64u);
    RC_CHECK(rc_ctz_u64(1ull), ==, 0u);
    RC_CHECK(rc_ctz_u64(0x8000000000000000ull), ==, 63u);
    RC_CHECK(rc_ctz_u64(0x100000000ull), ==, 32u);  // crosses the 32-bit boundary
}

RC_TEST(ops, popcount)
{
    RC_CHECK(rc_popcount_u32(0u), ==, 0u);
    RC_CHECK(rc_popcount_u32(1u), ==, 1u);
    RC_CHECK(rc_popcount_u32(0xFFFFFFFFu), ==, 32u);
    RC_CHECK(rc_popcount_u32(0x80000001u), ==, 2u);
    RC_CHECK(rc_popcount_u32(0xA5u), ==, 4u);        // 1010 0101
}

RC_TEST(ops, overflow)
{
    RC_CHECK_TRUE(rc_add_overflows_u64(UINT64_MAX, 1));
    RC_CHECK_FALSE(rc_add_overflows_u64(UINT64_MAX - 1, 1));
    RC_CHECK_TRUE(rc_mul_overflows_u64(UINT64_MAX, 2));
    RC_CHECK_FALSE(rc_mul_overflows_u64(0, UINT64_MAX));
    RC_CHECK_TRUE(rc_add_overflows_i64(INT64_MAX, 1));
    RC_CHECK_TRUE(rc_sub_overflows_i64(INT64_MIN, 1));
    RC_CHECK_TRUE(rc_mul_overflows_i64(INT64_MAX, 2));
    RC_CHECK_FALSE(rc_mul_overflows_i64(1000, 1000));
}

RC_TEST(ops, deg_to_rad)
{
    RC_CHECK(rc_deg_to_rad(180.0f), ~=, 3.14159265f);
    RC_CHECK(rc_deg_to_rad(0.0f), ==, 0.0f);
}
