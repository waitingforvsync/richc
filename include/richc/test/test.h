#ifndef RICHC_TEST_TEST_H_
#define RICHC_TEST_TEST_H_

#include "richc/str.h"

#if defined(_WIN32)
#if defined(__clang__) || defined(__GNUC__)
#define SECTION(s) __attribute__((used, section(s)))
#elif defined(_MSC_VER)
#define SECTION(s) __pragma(section(s)); __declspec(allocate(s))
#define SECTION_NAME "test$b"
#endif
#else
#define SECTION(s) __attribute__((used, section(s)))
#define SECTION_NAME "test"
#endif

typedef struct test_item test_item;

struct test_item {
    const char *group_name;
	const char *test_name;
	void (*test_fn)(void);
	void (*test_group_fn)(void *);
	void (*init_fn)(void *);
	void (*deinit_fn)(void *);
	void *context;
};


#define DEF_TEST(group, test) \
	static void test_##group##_##test(void); \
	static const test_item test_item_##group##_##test = { \
		.group_name = #group, \
		.test_name = #test, \
		.test_fn = test_##group##_##test \
	}; \
    SECTION(SECTION_NAME) const test_item *test_item_ptr_##group##_##test = &test_item_##group##_##test; \
	static void test_##group##_##test(void)

#define DEF_TEST_SKIP(group, test) \
	static void test_##group##_##test(void); \
	static const test_item test_item_##group##_##test = { \
		.group_name = #group, \
		.test_name = #test, \
	}; \
    SECTION(SECTION_NAME) const test_item *test_item_ptr_##group##_##test = &test_item_##group##_##test; \
	static void test_##group##_##test(void)

#define DEF_TEST_GROUP_DATA(group) \
	struct test_group_data_##group##_t

#define DEF_TEST_GROUP_INIT(group) \
	static void test_group_init_##group(struct test_group_data_##group##_t *data); \
	static void test_group_init_##group##_wrapper(void *data) { \
		test_group_init_##group((struct test_group_data_##group##_t *)data); \
	} \
	static void test_group_init_##group(struct test_group_data_##group##_t *data)

#define DEF_TEST_GROUP_DEINIT(group) \
	static void test_group_deinit_##group(struct test_group_data_##group##_t *data); \
	static void test_group_deinit_##group##_wrapper(void *data) { \
		test_group_deinit_##group((struct test_group_data_##group##_t *)data); \
	} \
	static void test_group_deinit_##group(struct test_group_data_##group##_t *data)

#define DEF_TEST_STEP(group, test) \
	static void test_##group##_##test(struct test_group_data_##group##_t *); \
	static void test_##group##_##test##_wrapper(void *data) { \
		test_##group##_##test((struct test_group_data_##group##_t *)data); \
	} \
	static struct test_group_data_##group##_t test_group_data_##group##_##test; \
	static const test_item test_item_##group##_##test = { \
		.group_name = #group, \
		.test_name = #test, \
		.test_group_fn = test_##group##_##test##_wrapper, \
		.init_fn = test_group_init_##group##_wrapper, \
		.deinit_fn = test_group_deinit_##group##_wrapper, \
		.context = &test_group_data_##group##_##test \
	}; \
    SECTION(SECTION_NAME) const test_item *test_item_ptr_##group##_##test = &test_item_##group##_##test; \
	static void test_##group##_##test(struct test_group_data_##group##_t *data)

#define DEF_TEST_STEP_SKIP(group, test) \
	static void test_##group##_##test(struct test_group_data_##group##_t *); \
	static struct test_group_data_##group##_t test_group_data_##group##_##test; \
	static const test_item test_item_##group##_##test = { \
		.group_name = #group, \
		.test_name = #test, \
		.context = &test_group_data_##group##_##test \
	}; \
    SECTION(SECTION_NAME) const test_item *test_item_ptr_##group##_##test = &test_item_##group##_##test; \
	static void test_##group##_##test(struct test_group_data_##group##_t *data)


#define TEST_REQUIRE(a, op, b)  TEST_REQUIRE_IMPL(a, op, b, __FILE__, __LINE__)
#define TEST_REQUIRE_TRUE(a)    TEST_REQUIRE_IMPL(!!(a),==,true, __FILE__, __LINE__)
#define TEST_REQUIRE_FALSE(a)   TEST_REQUIRE_IMPL(!(a),==,true, __FILE__, __LINE__)

#define TEST_REQUIRE_IMPL(a, op, b, file, line) \
	_Generic((a), \
		bool: test_require_bool, \
		int8_t: test_require_int, \
		uint8_t: test_require_int, \
		int16_t: test_require_int, \
		uint16_t: test_require_int, \
		int32_t: test_require_int, \
		uint32_t: test_require_int, \
		int64_t: test_require_int, \
		uint64_t: test_require_int, \
		float: test_require_float, \
		double: test_require_float, \
		str_t: test_require_str \
	)(a, #op, b, #a " " #op " " #b, file, line)

void test_require_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line);
void test_require_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line);
void test_require_float(double actual, const char *op, double expected, const char *expr, const char *file, int line);
void test_require_str(str_t actual, const char *op, str_t expected, const char *expr, const char *file, int line);

int test_run(const char *filter);


#endif // ifndef RICHC_TEST_TEST_H_
