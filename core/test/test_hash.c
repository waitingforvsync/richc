#include "richc/hash.h"
#include "richc/test.h"

RC_TEST(hash, integer)
{
    // deterministic
    RC_CHECK(rc_hash_u32(0xDEADBEEFu), ==, rc_hash_u32(0xDEADBEEFu));
    RC_CHECK(rc_hash_u64(0x0123456789ABCDEFull), ==, rc_hash_u64(0x0123456789ABCDEFull));
    // distinct inputs -> distinct outputs (for these particular values)
    RC_CHECK_TRUE(rc_hash_u32(0u) != rc_hash_u32(1u));
    RC_CHECK_TRUE(rc_hash_u64(0u) != rc_hash_u64(1u));
    // the signed wrappers hash the unsigned bit pattern
    RC_CHECK(rc_hash_i32(-1), ==, rc_hash_u32(0xFFFFFFFFu));
    RC_CHECK(rc_hash_i64(-1), ==, rc_hash_u64(0xFFFFFFFFFFFFFFFFull));
}

RC_TEST(hash, floats)
{
    RC_CHECK(rc_hash_f32(3.14f), ==, rc_hash_f32(3.14f));
    RC_CHECK(rc_hash_f64(3.14), ==, rc_hash_f64(3.14));
    // -0.0 and +0.0 are equal under ==, so they hash the same
    RC_CHECK(rc_hash_f32(-0.0f), ==, rc_hash_f32(0.0f));
    RC_CHECK(rc_hash_f64(-0.0), ==, rc_hash_f64(0.0));
    RC_CHECK_TRUE(rc_hash_f32(1.0f) != rc_hash_f32(2.0f));
}

RC_TEST(hash, pointer)
{
    int x;
    RC_CHECK(rc_hash_ptr(&x), ==, rc_hash_ptr(&x));
    RC_CHECK(rc_hash_ptr(NULL), ==, rc_hash_u64(0u));
}

RC_TEST(hash, bytes_and_str)
{
    uint8_t data[] = {1, 2, 3, 4};
    RC_CHECK(rc_hash_bytes(data, 4), ==, rc_hash_bytes(data, 4));   // deterministic

    // rc_hash_str hashes the string's bytes
    RC_CHECK(rc_hash_str(RC_STR("hello")), ==, rc_hash_bytes("hello", 5));
    RC_CHECK_TRUE(rc_hash_str(RC_STR("hello")) != rc_hash_str(RC_STR("world")));

    // the empty sequence and an invalid string both hash to the FNV basis
    RC_CHECK(rc_hash_bytes(NULL, 0), ==, rc_hash_str(rc_str_make(NULL)));
}

RC_TEST(hash, combine)
{
    uint32_t a = rc_hash_u32(1u);
    uint32_t b = rc_hash_u32(2u);
    RC_CHECK(rc_hash_combine(a, b), ==, rc_hash_combine(a, b));      // deterministic
    RC_CHECK_TRUE(rc_hash_combine(a, b) != rc_hash_combine(b, a));   // order matters
}
