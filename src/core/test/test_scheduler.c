#include "richc/thread/scheduler.h"

#include <string.h>

#include "richc/arena.h"
#include "richc/test.h"
#include "richc/thread/atomic.h"

// Typed futures used by the tests.
#define RC_FUTURE_TYPE int
#include "richc/template/future.h"

typedef struct vec3 {
    int x;
    int y;
    int z;
} vec3;

#define RC_FUTURE_TYPE vec3
#include "richc/template/future.h"

// Larger than RC_TASK_RESULT_SIZE (64B): only the caller-storage path can hold it.
typedef struct big {
    int v[40];
} big;

#define RC_FUTURE_TYPE big
#include "richc/template/future.h"

// ---- helpers ----

static rc_scheduler *make_sched(rc_arena *arena, uint32_t num_threads, uint32_t max_tasks)
{
    return rc_scheduler_create(
        (rc_scheduler_config) {
            .num_threads = num_threads,
            .max_tasks = max_tasks,
            .scratch_reserve = 0,
        },
        arena);
}

// ---- a single typed future ----

typedef struct add_args {
    int a;
    int b;
} add_args;

static void add_task(rc_task_context *tc, void *ctx)
{
    add_args *in = ctx;
    rc_future_int_set(tc, in->a + in->b);
}

RC_TEST(scheduler, single_future_inline)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 256);

    add_args in = {.a = 20, .b = 22};
    rc_future_int f = rc_scheduler_run_int(s, add_task, &in, NULL);   // inline slot buffer
    RC_CHECK(rc_future_int_get(f), ==, 42);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

RC_TEST(scheduler, single_future_caller_storage)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 256);

    add_args in = {.a = 20, .b = 22};
    int out = 0;
    rc_future_int f = rc_scheduler_run_int(s, add_task, &in, &out);   // caller storage
    RC_CHECK(rc_future_int_get(f), ==, 42);
    RC_CHECK(out, ==, 42);   // landed in the caller's own variable

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// A result larger than the inline buffer, which only caller storage can hold.
static void fill_big(rc_task_context *tc, void *ctx)
{
    (void)ctx;
    big *r = rc_future_big_result(tc);
    for (int i = 0; i < 40; ++i)
        r->v[i] = i * i;
}

RC_TEST(scheduler, large_result_caller_storage)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 256);

    big out;
    rc_future_big f = rc_scheduler_run_big(s, fill_big, NULL, &out);
    big v = rc_future_big_get(f);
    for (int i = 0; i < 40; ++i)
        RC_CHECK(v.v[i], ==, i * i);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- a struct result built in place ----

static void make_vec_task(rc_task_context *tc, void *ctx)
{
    (void)ctx;
    vec3 *r = rc_future_vec3_result(tc);
    r->x = 1;
    r->y = 2;
    r->z = 3;
}

RC_TEST(scheduler, struct_future)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 256);

    rc_future_vec3 f = rc_scheduler_run_vec3(s, make_vec_task, NULL, NULL);
    vec3 v = rc_future_vec3_get(f);
    RC_CHECK(v.x, ==, 1);
    RC_CHECK(v.y, ==, 2);
    RC_CHECK(v.z, ==, 3);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- fire and forget, drained by wait_all ----

static void inc_task(rc_task_context *tc, void *ctx)
{
    (void)tc;
    rc_atomic_i32_fetch_add((rc_atomic_i32 *)ctx, 1, RC_MEMORY_ORDER_SEQ_CST);
}

RC_TEST(scheduler, fire_and_forget_wait_all)
{
    enum { N = 1000 };
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 2048);

    rc_atomic_i32 counter = {0};
    for (int i = 0; i < N; ++i)
        rc_scheduler_run(s, inc_task, &counter);
    rc_scheduler_wait_all(s);

    RC_CHECK(rc_atomic_i32_load(&counter, RC_MEMORY_ORDER_SEQ_CST), ==, N);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- dependency DAG: diamond A -> {B, C} -> D ----

typedef struct diamond {
    rc_atomic_i32 done_a;
    rc_atomic_i32 done_b;
    rc_atomic_i32 done_c;
    rc_atomic_i32 done_d;
    rc_atomic_i32 violations;
} diamond;

static bool is_set(rc_atomic_i32 *a)
{
    return rc_atomic_i32_load(a, RC_MEMORY_ORDER_SEQ_CST) != 0;
}

static void diamond_a(rc_task_context *tc, void *ctx)
{
    (void)tc;
    diamond *d = ctx;
    rc_atomic_i32_store(&d->done_a, 1, RC_MEMORY_ORDER_SEQ_CST);
}

static void diamond_b(rc_task_context *tc, void *ctx)
{
    (void)tc;
    diamond *d = ctx;
    if (!is_set(&d->done_a))
        rc_atomic_i32_fetch_add(&d->violations, 1, RC_MEMORY_ORDER_SEQ_CST);
    rc_atomic_i32_store(&d->done_b, 1, RC_MEMORY_ORDER_SEQ_CST);
}

static void diamond_c(rc_task_context *tc, void *ctx)
{
    (void)tc;
    diamond *d = ctx;
    if (!is_set(&d->done_a))
        rc_atomic_i32_fetch_add(&d->violations, 1, RC_MEMORY_ORDER_SEQ_CST);
    rc_atomic_i32_store(&d->done_c, 1, RC_MEMORY_ORDER_SEQ_CST);
}

static void diamond_d(rc_task_context *tc, void *ctx)
{
    (void)tc;
    diamond *d = ctx;
    if (!is_set(&d->done_b) || !is_set(&d->done_c))
        rc_atomic_i32_fetch_add(&d->violations, 1, RC_MEMORY_ORDER_SEQ_CST);
    rc_atomic_i32_store(&d->done_d, 1, RC_MEMORY_ORDER_SEQ_CST);
}

RC_TEST(scheduler, dependency_diamond)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 256);

    diamond d = {0};
    rc_task a = rc_scheduler_task_make(s, diamond_a, &d);
    rc_task b = rc_scheduler_task_make(s, diamond_b, &d);
    rc_task c = rc_scheduler_task_make(s, diamond_c, &d);
    rc_task dd = rc_scheduler_task_make(s, diamond_d, &d);

    rc_task_after(s, a, b);
    rc_task_after(s, a, c);
    rc_task_after(s, b, dd);
    rc_task_after(s, c, dd);

    rc_scheduler_submit(s, a);
    rc_scheduler_submit(s, b);
    rc_scheduler_submit(s, c);
    rc_scheduler_submit(s, dd);

    rc_scheduler_wait(s, dd);

    RC_CHECK(rc_atomic_i32_load(&d.done_d, RC_MEMORY_ORDER_SEQ_CST), ==, 1);
    RC_CHECK(rc_atomic_i32_load(&d.violations, RC_MEMORY_ORDER_SEQ_CST), ==, 0);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- fan-in: many predecessors into one join ----

RC_TEST(scheduler, fan_in_join)
{
    enum { N = 64 };
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 256);

    rc_atomic_i32 counter = {0};
    diamond join_state = {0};   // reuse: done_d flags the join, done_a counts leaves seen

    rc_task join = rc_scheduler_task_make(s, diamond_d, &join_state);
    // Make the join observe all leaves: it checks done_b/done_c, so pre-set those
    // and instead assert via the counter below; here we only need ordering.
    rc_atomic_i32_store(&join_state.done_b, 1, RC_MEMORY_ORDER_SEQ_CST);
    rc_atomic_i32_store(&join_state.done_c, 1, RC_MEMORY_ORDER_SEQ_CST);

    rc_task leaves[N];
    for (int i = 0; i < N; ++i) {
        leaves[i] = rc_scheduler_task_make(s, inc_task, &counter);
        rc_task_after(s, leaves[i], join);
    }
    for (int i = 0; i < N; ++i)
        rc_scheduler_submit(s, leaves[i]);
    rc_scheduler_submit(s, join);

    rc_scheduler_wait(s, join);

    // The join ran, and every leaf had incremented before it (dep edges).
    RC_CHECK(rc_atomic_i32_load(&counter, RC_MEMORY_ORDER_SEQ_CST), ==, N);
    RC_CHECK(rc_atomic_i32_load(&join_state.done_d, RC_MEMORY_ORDER_SEQ_CST), ==, 1);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- recursive fork-join: parallel sum ----

typedef struct sum_job {
    const int *v;
    uint32_t   lo;
    uint32_t   hi;
    int64_t    out;
} sum_job;

static void parallel_sum(rc_task_context *tc, void *ctx)
{
    sum_job *j = ctx;
    if (j->hi - j->lo <= 4096) {
        int64_t sum = 0;
        for (uint32_t i = j->lo; i < j->hi; ++i)
            sum += j->v[i];
        j->out = sum;
        return;
    }
    uint32_t mid = j->lo + (j->hi - j->lo) / 2;
    sum_job *l = rc_arena_alloc_type(tc->scratch, sum_job, 1);
    sum_job *r = rc_arena_alloc_type(tc->scratch, sum_job, 1);
    *l = (sum_job) {.v = j->v, .lo = j->lo, .hi = mid};
    *r = (sum_job) {.v = j->v, .lo = mid, .hi = j->hi};

    rc_task lt = rc_scheduler_run(tc->scheduler, parallel_sum, l);
    rc_task rt = rc_scheduler_run(tc->scheduler, parallel_sum, r);
    rc_scheduler_wait(tc->scheduler, lt);
    rc_scheduler_wait(tc->scheduler, rt);
    j->out = l->out + r->out;
}

RC_TEST(scheduler, recursive_fork_join)
{
    enum { N = 200000 };
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 4096);

    int *data = rc_arena_alloc_type(&arena, int, N);
    for (int i = 0; i < N; ++i)
        data[i] = 1;

    sum_job top = {.v = data, .lo = 0, .hi = N, .out = 0};
    rc_scheduler_wait(s, rc_scheduler_run(s, parallel_sum, &top));

    RC_CHECK(top.out, ==, (int64_t)N);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- per-worker scratch is reset between tasks ----

// One worker, a 16 MB scratch reserve, and many 1 MB allocations: without the
// per-task reset the arena would overflow its reserve and panic well before the
// end, so completing all of them proves the reset happens.
static void scratch_task(rc_task_context *tc, void *ctx)
{
    void *p = rc_arena_alloc(tc->scratch, 1024 * 1024);
    memset(p, 0xAB, 1024 * 1024);
    rc_atomic_i32_fetch_add((rc_atomic_i32 *)ctx, 1, RC_MEMORY_ORDER_SEQ_CST);
}

RC_TEST(scheduler, scratch_reset_between_tasks)
{
    enum { N = 256 };   // 256 MB of allocations against a 16 MB reserve
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = rc_scheduler_create(
        (rc_scheduler_config) {.num_threads = 1, .max_tasks = 512, .scratch_reserve = 0},
        &arena);

    rc_atomic_i32 counter = {0};
    for (int i = 0; i < N; ++i)
        rc_scheduler_run(s, scratch_task, &counter);
    rc_scheduler_wait_all(s);

    RC_CHECK(rc_atomic_i32_load(&counter, RC_MEMORY_ORDER_SEQ_CST), ==, N);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- single worker still runs everything ----

RC_TEST(scheduler, single_worker)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 1, 256);

    rc_atomic_i32 counter = {0};
    for (int i = 0; i < 100; ++i)
        rc_scheduler_run(s, inc_task, &counter);
    rc_scheduler_wait_all(s);
    RC_CHECK(rc_atomic_i32_load(&counter, RC_MEMORY_ORDER_SEQ_CST), ==, 100);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

// ---- edge cases ----

RC_TEST(scheduler, wait_on_none_returns)
{
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 16);
    rc_scheduler_wait(s, RC_TASK_NONE);   // must not block or crash
    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}

RC_TEST(scheduler, deinit_drains_queued_work)
{
    enum { N = 500 };
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 1024);

    rc_atomic_i32 counter = {0};
    for (int i = 0; i < N; ++i)
        rc_scheduler_run(s, inc_task, &counter);
    // No wait_all: deinit must drain the queue before joining.
    rc_scheduler_deinit(s);

    RC_CHECK(rc_atomic_i32_load(&counter, RC_MEMORY_ORDER_SEQ_CST), ==, N);
    rc_arena_deinit(&arena);
}

// ---- stress: many tasks each depending on a shared root ----

RC_TEST(scheduler, stress_wide_dependencies)
{
    enum { N = 2000 };
    rc_arena arena = rc_arena_make_default();
    rc_scheduler *s = make_sched(&arena, 0, 4096);

    rc_atomic_i32 counter = {0};
    // A chain of small fan-ins: each round of leaves feeds a join, repeated.
    for (int round = 0; round < N / 16; ++round) {
        diamond join_state = {0};
        rc_atomic_i32_store(&join_state.done_b, 1, RC_MEMORY_ORDER_SEQ_CST);
        rc_atomic_i32_store(&join_state.done_c, 1, RC_MEMORY_ORDER_SEQ_CST);
        rc_task join = rc_scheduler_task_make(s, diamond_d, &join_state);
        rc_task leaves[16];
        for (int i = 0; i < 16; ++i) {
            leaves[i] = rc_scheduler_task_make(s, inc_task, &counter);
            rc_task_after(s, leaves[i], join);
        }
        for (int i = 0; i < 16; ++i)
            rc_scheduler_submit(s, leaves[i]);
        rc_scheduler_submit(s, join);
        rc_scheduler_wait(s, join);   // join_state is stack-local: collect it before reuse
    }

    RC_CHECK(rc_atomic_i32_load(&counter, RC_MEMORY_ORDER_SEQ_CST), ==, (N / 16) * 16);

    rc_scheduler_deinit(s);
    rc_arena_deinit(&arena);
}
