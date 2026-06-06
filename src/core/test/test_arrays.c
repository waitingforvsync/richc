#include "richc/array/f32.h"
#include "richc/array/f64.h"
#include "richc/array/i16.h"
#include "richc/array/i32.h"
#include "richc/array/i64.h"
#include "richc/array/i8.h"
#include "richc/array/mstr.h"
#include "richc/array/str.h"
#include "richc/array/u16.h"
#include "richc/array/u32.h"
#include "richc/array/u64.h"
#include "richc/array/u8.h"
#include "richc/math/array/box2f.h"
#include "richc/math/array/box2i.h"
#include "richc/math/array/mat22f.h"
#include "richc/math/array/mat23f.h"
#include "richc/math/array/mat33f.h"
#include "richc/math/array/mat34f.h"
#include "richc/math/array/mat44f.h"
#include "richc/math/array/quatf.h"
#include "richc/math/array/rational.h"
#include "richc/math/array/vec2f.h"
#include "richc/math/array/vec2i.h"
#include "richc/math/array/vec3f.h"
#include "richc/math/array/vec3i.h"
#include "richc/math/array/vec4f.h"
#include "richc/test.h"

// Re-include a few to confirm the include guards make a second inclusion a no-op
// (a missing guard would redefine the generated types and fail to compile).
#include "richc/array/u8.h"
#include "richc/math/array/vec2i.h"
#include "richc/array/str.h"

// These headers are stub instantiations of the array template; the container
// operations themselves are exercised exhaustively by test_array.c / test_bytes.c.
// The point here is that every stub compiles, the guards hold, and all 26
// instantiations coexist in one translation unit - plus a basic smoke test of one
// representative per family (scalar int, float, rc_str, a math type).

RC_TEST_GROUP_DATA(arrays) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(arrays, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(arrays, fix)
{
    rc_arena_deinit(&fix->a);
}

RC_TEST_STEP(arrays, scalar_int, fix)
{
    rc_array_u8 a = rc_array_u8_make(0, &fix->a);
    rc_array_u8_push(&a, 0xAB, &fix->a);
    rc_array_u8_push(&a, 0x12, &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_u8_get(&a, 0), ==, (uint8_t)0xAB);
    RC_CHECK(*rc_array_u8_at(&a, 1), ==, (uint8_t)0x12);
}

RC_TEST_STEP(arrays, scalar_float, fix)
{
    rc_array_f32 a = rc_array_f32_make(0, &fix->a);
    rc_array_f32_push(&a, 1.5f, &fix->a);
    rc_array_f32_push(&a, 2.5f, &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_f32_get(&a, 0), ==, 1.5f);
    RC_CHECK(rc_array_f32_get(&a, 1), ==, 2.5f);
}

RC_TEST_STEP(arrays, string, fix)
{
    rc_array_str a = rc_array_str_make(0, &fix->a);
    rc_array_str_push(&a, RC_STR("hello"), &fix->a);
    rc_array_str_push(&a, RC_STR("world"), &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_str_get(&a, 0), ==, RC_STR("hello"));
    RC_CHECK(rc_array_str_get(&a, 1), ==, RC_STR("world"));
}

RC_TEST_STEP(arrays, math_type, fix)
{
    rc_array_vec2i a = rc_array_vec2i_make(0, &fix->a);
    rc_array_vec2i_push(&a, (rc_vec2i) {.x = 1, .y = 2}, &fix->a);
    rc_array_vec2i_push(&a, (rc_vec2i) {.x = 3, .y = 4}, &fix->a);
    RC_CHECK(a.num, ==, 2u);
    RC_CHECK(rc_array_vec2i_get(&a, 0), ==, ((rc_vec2i) {.x = 1, .y = 2}));
    RC_CHECK(rc_array_vec2i_get(&a, 1), ==, ((rc_vec2i) {.x = 3, .y = 4}));
}
