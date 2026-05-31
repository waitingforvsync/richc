/*
 * test.h - homegrown unit-test framework for richc.
 *
 * Tests register themselves automatically: each RC_TEST (and friends) emits a
 * pointer to a descriptor into a dedicated linker section, and rc_test_run
 * walks that section at run time.  No explicit registration list is needed.
 *
 * Defining tests
 * --------------
 *   RC_TEST(group, name) { ... }        a simple test
 *   RC_TEST_SKIP(group, name) { ... }   registered but not run (reported SKIP)
 *
 * Tests with a fixture
 * --------------------
 *   RC_TEST_GROUP_DATA(group) { ... };       declare the per-group fixture struct
 *   RC_TEST_GROUP_INIT(group, fix) { ... }   runs before each test in the group
 *   RC_TEST_GROUP_DEINIT(group, fix) { ... } runs after each test in the group
 *   RC_TEST_STEP(group, name, fix) { ... }   a test that receives the fixture
 *   RC_TEST_STEP_SKIP(group, name, fix)      registered fixtured test, not run
 * The last argument to INIT/DEINIT/STEP names the fixture pointer made available
 * inside the body; it has type struct rc_test_group_data_<group> *.
 *
 * Assertions
 * ----------
 *   RC_CHECK(a, op, b)   compare a op b; op is one of == != < > <= >=
 *                        (and ~= for float/double and the float vectors, a
 *                        fixed-epsilon compare)
 *   RC_CHECK_TRUE(a)     assert a is truthy
 *   RC_CHECK_FALSE(a)    assert a is falsy
 * The left operand selects the comparison via _Generic: bool, the fixed-width
 * integer types, float, double, rc_str, the integer vectors rc_vec2i/rc_vec3i
 * (== and !=), the float vectors rc_vec2f/rc_vec3f/rc_vec4f (==, !=, and ~=, the
 * latter a per-component fixed-epsilon compare), and rc_rational (the full set
 * == != < > <= >=) are supported.  A failing assertion prints file:line and the
 * actual value, then aborts the current test.
 *
 * Running
 * -------
 *   int rc_test_run(const char *filter);  run all tests whose group name starts
 *                                         with filter ("" runs everything);
 *                                         returns the number of failures.
 *   RC_TEST_MAIN()                        emit a main() that calls rc_test_run
 *                                         with the first argument as the filter.
 */

#ifndef RC_TEST_H_
#define RC_TEST_H_

#include "richc/math/rational.h"
#include "richc/math/vec4f.h"   // transitively brings the whole vec2f/3f/4f and vec2i/3i family
#include "richc/str.h"          // also provides <stdint.h> / <stdbool.h>

/* ---- linker-section placement (compiler/platform specific) ---- */

#if defined(_WIN32)
#  if defined(__clang__) || defined(__GNUC__)
#    define RC_TEST_SECTION_(s) __attribute__((used, section(s)))
#  elif defined(_MSC_VER)
#    define RC_TEST_SECTION_(s) __pragma(section(s)); __declspec(allocate(s))
#  else
#    error "richc test: unsupported compiler on Windows"
#  endif
/* On Windows the linker concatenates sections in name order, so test items go
 * in rc_test$b, bracketed by the rc_test$a and rc_test$c sentinels. */
#  define RC_TEST_SECTION_NAME_ "rc_test$b"
#elif defined(__linux__)
#  define RC_TEST_SECTION_(s) __attribute__((used, section(s)))
/* The ELF linker synthesises __start_rc_test / __stop_rc_test for this name. */
#  define RC_TEST_SECTION_NAME_ "rc_test"
#else
/* macOS / Mach-O needs a "segment,section" name and getsectiondata rather than
 * __start_/__stop_ symbols; that is separate work, not handled here. */
#  error "richc test: unsupported platform"
#endif

/* ---- descriptor ---- */

typedef struct rc_test rc_test;

struct rc_test {
    const char *group_name;
    const char *test_name;
    void (*fn)(void);
    void (*step_fn)(void *);
    void (*init_fn)(void *);
    void (*deinit_fn)(void *);
    void *context;
};

/* ---- defining tests ---- */

#define RC_TEST(group, name) \
    static void rc_test_fn_##group##_##name(void); \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .fn = rc_test_fn_##group##_##name \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(void)

#define RC_TEST_SKIP(group, name) \
    static void rc_test_fn_##group##_##name(void); \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(void)

/* ---- fixtures ---- */

#define RC_TEST_GROUP_DATA(group) \
    struct rc_test_group_data_##group

#define RC_TEST_GROUP_INIT(group, fixture) \
    static void rc_test_init_##group(struct rc_test_group_data_##group *fixture); \
    static void rc_test_init_##group##_wrapper(void *fixture) { \
        rc_test_init_##group((struct rc_test_group_data_##group *)fixture); \
    } \
    static void rc_test_init_##group(struct rc_test_group_data_##group *fixture)

#define RC_TEST_GROUP_DEINIT(group, fixture) \
    static void rc_test_deinit_##group(struct rc_test_group_data_##group *fixture); \
    static void rc_test_deinit_##group##_wrapper(void *fixture) { \
        rc_test_deinit_##group((struct rc_test_group_data_##group *)fixture); \
    } \
    static void rc_test_deinit_##group(struct rc_test_group_data_##group *fixture)

#define RC_TEST_STEP(group, name, fixture) \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *); \
    static void rc_test_step_##group##_##name##_wrapper(void *fixture) { \
        rc_test_fn_##group##_##name((struct rc_test_group_data_##group *)fixture); \
    } \
    static struct rc_test_group_data_##group rc_test_data_##group##_##name; \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .step_fn = rc_test_step_##group##_##name##_wrapper, \
        .init_fn = rc_test_init_##group##_wrapper, \
        .deinit_fn = rc_test_deinit_##group##_wrapper, \
        .context = &rc_test_data_##group##_##name \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *fixture)

#define RC_TEST_STEP_SKIP(group, name, fixture) \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *); \
    static struct rc_test_group_data_##group rc_test_data_##group##_##name; \
    static const rc_test rc_test_desc_##group##_##name = { \
        .group_name = #group, \
        .test_name = #name, \
        .context = &rc_test_data_##group##_##name \
    }; \
    RC_TEST_SECTION_(RC_TEST_SECTION_NAME_) const rc_test *rc_test_ptr_##group##_##name = &rc_test_desc_##group##_##name; \
    static void rc_test_fn_##group##_##name(struct rc_test_group_data_##group *fixture)

/* ---- assertions ---- */

#define RC_CHECK(a, op, b)  RC_CHECK_IMPL_(a, op, b, __FILE__, __LINE__)
#define RC_CHECK_TRUE(a)    RC_CHECK_IMPL_(!!(a), ==, true, __FILE__, __LINE__)
#define RC_CHECK_FALSE(a)   RC_CHECK_IMPL_(!(a), ==, true, __FILE__, __LINE__)

/* The fixed-width integer entries also cover plain int / unsigned literals
 * (int32_t is int, uint32_t is unsigned on the supported targets).  Do not add
 * long / char: on LP64 int64_t is long, which would make _Generic ambiguous. */
#define RC_CHECK_IMPL_(a, op, b, file, line) \
    _Generic((a), \
        bool: rc_test_check_bool, \
        int8_t: rc_test_check_int, \
        uint8_t: rc_test_check_int, \
        int16_t: rc_test_check_int, \
        uint16_t: rc_test_check_int, \
        int32_t: rc_test_check_int, \
        uint32_t: rc_test_check_int, \
        int64_t: rc_test_check_int, \
        uint64_t: rc_test_check_int, \
        float: rc_test_check_float, \
        double: rc_test_check_float, \
        rc_str: rc_test_check_str, \
        rc_vec2i: rc_test_check_vec2i, \
        rc_vec3i: rc_test_check_vec3i, \
        rc_vec2f: rc_test_check_vec2f, \
        rc_vec3f: rc_test_check_vec3f, \
        rc_vec4f: rc_test_check_vec4f, \
        rc_rational: rc_test_check_rational \
    )(a, #op, b, #a " " #op " " #b, file, line)

void rc_test_check_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line);
void rc_test_check_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line);
void rc_test_check_float(double actual, const char *op, double expected, const char *expr, const char *file, int line);
void rc_test_check_str(rc_str actual, const char *op, rc_str expected, const char *expr, const char *file, int line);
void rc_test_check_vec2i(rc_vec2i actual, const char *op, rc_vec2i expected, const char *expr, const char *file, int line);
void rc_test_check_vec3i(rc_vec3i actual, const char *op, rc_vec3i expected, const char *expr, const char *file, int line);
void rc_test_check_vec2f(rc_vec2f actual, const char *op, rc_vec2f expected, const char *expr, const char *file, int line);
void rc_test_check_vec3f(rc_vec3f actual, const char *op, rc_vec3f expected, const char *expr, const char *file, int line);
void rc_test_check_vec4f(rc_vec4f actual, const char *op, rc_vec4f expected, const char *expr, const char *file, int line);
void rc_test_check_rational(rc_rational actual, const char *op, rc_rational expected, const char *expr, const char *file, int line);

/* ---- running ---- */

int rc_test_run(const char *filter);

#define RC_TEST_MAIN() \
    int main(int argc, char **argv) { \
        return rc_test_run(argc > 1 ? argv[1] : ""); \
    }

#endif /* RC_TEST_H_ */
