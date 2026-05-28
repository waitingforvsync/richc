#include "richc/test.h"

#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>

/* Section boundaries.  On Windows the sentinels are placed either side of the
 * test items in name order ($a < $b < $c); on ELF the linker provides the
 * __start_/__stop_ symbols for the named section. */
#if defined(_WIN32)
RC_TEST_SECTION_("rc_test$a") static const rc_test *rc_test_section_start;
RC_TEST_SECTION_("rc_test$c") static const rc_test *rc_test_section_stop;
#  define RC_TEST_BEGIN (&rc_test_section_start)
#  define RC_TEST_END   (&rc_test_section_stop)
#else
extern const rc_test *__start_rc_test;
extern const rc_test *__stop_rc_test;
#  define RC_TEST_BEGIN (&__start_rc_test)
#  define RC_TEST_END   (&__stop_rc_test)
#endif

static jmp_buf rc_test_jmp;
static const double rc_test_epsilon = 0.0001;

/* Match a one- or two-character operator string exactly. */
static bool rc_test_op1(const char *a, const char *b)
{
    return a[0] == b[0] && a[1] == 0;
}

static bool rc_test_op2(const char *a, const char *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == 0;
}

void rc_test_check_bool(bool actual, const char *op, bool expected, const char *expr, const char *file, int line)
{
    if (rc_test_op2(op, "==")) {
        if (actual == expected) return;
    }
    else if (rc_test_op2(op, "!=")) {
        if (actual != expected) return;
    }
    else {
        printf("[FAIL]\n%s:%d: unsupported operation: %s\n", file, line, op);
    }

    printf("[FAIL]\n%s:%d: %s: actual %s\n", file, line, expr, actual ? "true" : "false");
    longjmp(rc_test_jmp, 1);
}

void rc_test_check_int(int64_t actual, const char *op, int64_t expected, const char *expr, const char *file, int line)
{
    if (rc_test_op2(op, "==")) {
        if (actual == expected) return;
    }
    else if (rc_test_op2(op, "!=")) {
        if (actual != expected) return;
    }
    else if (rc_test_op1(op, "<")) {
        if (actual < expected) return;
    }
    else if (rc_test_op1(op, ">")) {
        if (actual > expected) return;
    }
    else if (rc_test_op2(op, "<=")) {
        if (actual <= expected) return;
    }
    else if (rc_test_op2(op, ">=")) {
        if (actual >= expected) return;
    }
    else {
        printf("[FAIL]\n%s:%d: unsupported operation: %s\n", file, line, op);
    }

    printf("[FAIL]\n%s:%d: %s: actual %" PRId64 "\n", file, line, expr, actual);
    longjmp(rc_test_jmp, 1);
}

void rc_test_check_float(double actual, const char *op, double expected, const char *expr, const char *file, int line)
{
    if (rc_test_op2(op, "==")) {
        if (actual == expected) return;
    }
    else if (rc_test_op2(op, "~=")) {
        if (fabs(actual - expected) < rc_test_epsilon) return;
    }
    else if (rc_test_op2(op, "!=")) {
        if (actual != expected) return;
    }
    else if (rc_test_op1(op, "<")) {
        if (actual < expected) return;
    }
    else if (rc_test_op1(op, ">")) {
        if (actual > expected) return;
    }
    else if (rc_test_op2(op, "<=")) {
        if (actual <= expected) return;
    }
    else if (rc_test_op2(op, ">=")) {
        if (actual >= expected) return;
    }
    else {
        printf("[FAIL]\n%s:%d: unsupported operation: %s\n", file, line, op);
    }

    printf("[FAIL]\n%s:%d: %s: actual %f\n", file, line, expr, actual);
    longjmp(rc_test_jmp, 1);
}

void rc_test_check_str(rc_str actual, const char *op, rc_str expected, const char *expr, const char *file, int line)
{
    if (rc_test_op2(op, "==")) {
        if (rc_str_is_equal(actual, expected)) return;
    }
    else if (rc_test_op2(op, "!=")) {
        if (!rc_str_is_equal(actual, expected)) return;
    }
    else {
        printf("[FAIL]\n%s:%d: unsupported operation: %s\n", file, line, op);
    }

    printf("[FAIL]\n%s:%d: %s: actual \"%.*s\"\n", file, line, expr, (int)actual.len, actual.data);
    longjmp(rc_test_jmp, 1);
}

int rc_test_run(const char *filter_string)
{
    rc_str filter = rc_str_make(filter_string);
    int num_fail = 0;
    int num_pass = 0;
    int num_skip = 0;

    // First pass: count the matching tests and find the widest name.
    int total = 0;
    int max_width = 0;
    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        const rc_test *t = *it;
        if (!t) continue;
        rc_str group = rc_str_make(t->group_name);
        if (rc_str_starts_with(group, filter)) {
            rc_str name = rc_str_make(t->test_name);
            int width = (int)(group.len + name.len);
            if (width > max_width) {
                max_width = width;
            }
            total++;
        }
    }

    // Second pass: run each matching test.
    int count = 1;
    for (const rc_test **it = RC_TEST_BEGIN; it != RC_TEST_END; ++it) {
        const rc_test *t = *it;
        if (!t) continue;
        rc_str group = rc_str_make(t->group_name);
        if (!rc_str_starts_with(group, filter)) continue;

        rc_str name = rc_str_make(t->test_name);
        int padding = max_width + 3 - (int)(group.len + name.len);
        if (padding < 0) {
            padding = 0;
        }

        printf("%4d/%d:  %.*s.%.*s%-*s",
            count, total, (int)group.len, group.data, (int)name.len, name.data, padding, ":");

        if (t->fn || t->step_fn) {
            if (!setjmp(rc_test_jmp)) {
                if (t->init_fn) {
                    t->init_fn(t->context);
                }
                if (t->context) {
                    t->step_fn(t->context);
                }
                else {
                    t->fn();
                }
                if (t->deinit_fn) {
                    t->deinit_fn(t->context);
                }
                printf("ok\n");
                num_pass++;
            }
            else {
                num_fail++;
            }
        }
        else {
            printf("[SKIP]\n");
            num_skip++;
        }

        count++;
    }

    printf("\nRESULTS: %d tests (%d ok, %d failed, %d skipped)\n", total, num_pass, num_fail, num_skip);
    fflush(stdout);
    return num_fail;
}
