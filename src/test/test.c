#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "richc/test/test.h"

#ifdef _WIN32
SECTION("test$a") static const test_item *__start_test;
SECTION("test$c") static const test_item *__stop_test;
#else
extern const test_item *__start_test;
extern const test_item *__stop_test;
#endif


static jmp_buf env;
static double epsilon = 0.0001;

static bool is_op1(const char *a, const char *b) {
	return a[0] == b[0] && a[1] == 0;
}

static bool is_op2(const char *a, const char *b) {
	return a[0] == b[0] && a[1] == b[1] && a[2] == 0;
}

void test_require_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line) {
	if (is_op2(op, "==")) {
		if (actual == expected) return;
	}
	else if (is_op2(op, "!=")) {
		if (actual != expected) return; 
	}
	else {
		printf("[FAIL]\n%s:%d: Unsupported operation: %s\n", file, line, op);
	}

	printf("[FAIL]\n%s:%d: %s: actual %s\n", file, line, expr, actual ? "true" : "false");
	longjmp(env, 1);
}

void test_require_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line) {
	if (is_op2(op, "==")) {
		if (actual == expected) return;
	}
	else if (is_op2(op, "!=")) {
		if (actual != expected) return;
	}
	else if (is_op1(op, "<" )) {
		if (actual <  expected) return;
	}
	else if (is_op1(op, ">" )) {
		if (actual >  expected) return;
	}
	else if (is_op2(op, "<=")) {
		if (actual <= expected) return;
	}
	else if (is_op2(op, ">=")) {
		if (actual >= expected) return;
	}
	else {
		printf("[FAIL]\n%s:%d: Unsupported operation: %s\n", file, line, op);
	}
	
	printf("[FAIL]\n%s:%d: %s: actual %" PRId64 "\n", file, line, expr, actual);
	longjmp(env, 1);
}

void test_require_float(double actual, const char *op, double expected, const char *expr, const char *file, int line) {
	if (is_op2(op, "==")) {
		if (actual == expected) return; 
	}
	else if (is_op2(op, "~=")) {
		if (fabs(actual - expected) < epsilon) return;
	}
	else if (is_op2(op, "!=")) {
		if (actual != expected) return; 
	}
	else if (is_op1(op, "<" )) {
		if (actual < expected) return; 
	}
	else if (is_op1(op, ">" )) {
		if (actual > expected) return; 
	}
	else if (is_op2(op, "<=")) {
		if (actual <= expected) return; 
	}
	else if (is_op2(op, ">=")) {
		if (actual >= expected) return; 
	}
	else {
		printf("[FAIL]\n%s:%d: Unsupported operation: %s\n", file, line, op);
	}
	printf("[FAIL]\n%s:%d: %s: actual %f\n", file, line, expr, actual);
	longjmp(env, 1);
}

void test_require_str(str_t actual, const char *op, str_t expected, const char *expr, const char *file, int line) {
	if (is_op2(op, "==")) {
		if (str_is_equal(actual, expected)) return;
	}
	else if (is_op2(op, "!=")) {
		if (!str_is_equal(actual, expected)) return;
	}
	else {
		printf("[FAIL]\n%s:%d: Unsupported operation: %s\n", file, line, op);
	}

	printf("[FAIL]\n%s:%d: %s: actual \"" STR_FORMAT "\"\n", file, line, expr, STR_PRINT(actual));
	longjmp(env, 1);
}


int test_run(const char *filter_string) {
    str_t filter = str_make(filter_string);
	int num_fail = 0;
	int num_pass = 0;
	int num_skip = 0;

	// Count number of tests we're going to run
	int total = 0;
	int max_width = 0;
	for (const test_item **start = &__start_test; start != &__stop_test; start++) {
		const test_item *test_item = *start;
		if (test_item) {
			str_t group_name = str_make(test_item->group_name);
			if (str_startswith(group_name, filter)) {
				str_t test_name = str_make(test_item->test_name);
				int width = group_name.len + test_name.len;
				if (width > max_width) {
					max_width = width;
				}
				total++;
			}
		}
	}
	
	// Iterate through tests
	int count = 1;
	for (const test_item **start = &__start_test; start != &__stop_test; start++) {
		const test_item *test_item = *start;
		if (test_item) {
			str_t group_name = str_make(test_item->group_name);
			if (str_startswith(group_name, filter)) {
				str_t test_name = str_make(test_item->test_name);
				int32_t padding = max_width + 3 - group_name.len - test_name.len;
				if (padding < 0) {
					padding = 0;
				}
				
				printf("%4d/%d:  " STR_FORMAT "." STR_FORMAT "%-*s", 
					count, total, STR_PRINT(group_name), STR_PRINT(test_name), padding, ":");
				
				if (test_item->test_fn || test_item->test_group_fn) {
				
					if (!setjmp(env)) {
						// Call the group init function if one defined
						if (test_item->init_fn) {
							test_item->init_fn(test_item->context);
						}

						// Call the test, with or without fixture context as appropriate
						if (test_item->context) {
							test_item->test_group_fn(test_item->context);
						}
						else {
							test_item->test_fn();
						}
						
						// Call the group deinit function if one defined
						if (test_item->deinit_fn) {
							test_item->deinit_fn(test_item->context);
						}
						
						// If we got here, it's a pass
						printf("ok\n");
						num_pass++;
					}
					else {
						// If we got here it's a fail
						num_fail++;
					}
				}
				else {
					// If we got here, we skipped
					printf("[SKIP]\n");
					num_skip++;
				}
				
				count++;
			}
		}
	}
	
	printf("\nRESULTS: %d tests (%d ok, %d failed, %d skipped)\n", total, num_pass, num_fail, num_skip);
	fflush(stdout);
	return num_fail;
}
