#include "richc/thread/thread.h"

#include "richc/test.h"
#include "richc/thread/atomic.h"
#include "richc/thread/cond.h"
#include "richc/thread/mutex.h"
#include "richc/thread/rwlock.h"
#include "richc/thread/semaphore.h"
#include "richc/thread/spinlock.h"
#include "richc/time.h"

enum { THREADS = 8, ITERS = 100000 };

/* ---- basic create / join ---- */

typedef struct run_ctx {
    int input;
    int output;
} run_ctx;

static void run_worker(void *p)
{
    run_ctx *c = p;
    c->output = c->input * 2;
}

RC_TEST(thread, create_join_runs_fn)
{
    run_ctx ctx = {.input = 21, .output = 0};
    rc_thread t;
    RC_CHECK_TRUE(rc_thread_create(&t, run_worker, &ctx));
    rc_thread_join(&t);
    RC_CHECK(ctx.output, ==, 42);
}

/* ---- atomics under contention ---- */

typedef struct atomic_ctx {
    rc_atomic_i64 *counter;
    int iters;
} atomic_ctx;

static void atomic_worker(void *p)
{
    atomic_ctx *c = p;
    for (int i = 0; i < c->iters; ++i)
        rc_atomic_i64_fetch_add(c->counter, 1, RC_MEMORY_ORDER_RELAXED);
}

RC_TEST(thread, atomic_concurrent_sum)
{
    rc_atomic_i64 counter = {0};
    atomic_ctx ctx = {.counter = &counter, .iters = ITERS};
    rc_thread threads[THREADS];
    for (int i = 0; i < THREADS; ++i)
        RC_CHECK_TRUE(rc_thread_create(&threads[i], atomic_worker, &ctx));
    for (int i = 0; i < THREADS; ++i)
        rc_thread_join(&threads[i]);
    RC_CHECK(rc_atomic_i64_load(&counter, RC_MEMORY_ORDER_RELAXED), ==, (int64_t)THREADS * ITERS);
}

/* ---- mutex mutual exclusion ---- */

typedef struct mutex_ctx {
    rc_mutex *m;
    int64_t  *counter;
    int       iters;
} mutex_ctx;

static void mutex_worker(void *p)
{
    mutex_ctx *c = p;
    for (int i = 0; i < c->iters; ++i) {
        rc_mutex_lock(c->m);
        ++*c->counter;                 // unguarded increment; correct only under the lock
        rc_mutex_unlock(c->m);
    }
}

RC_TEST(thread, mutex_mutual_exclusion)
{
    rc_mutex m;
    rc_mutex_init(&m);
    int64_t counter = 0;
    mutex_ctx ctx = {.m = &m, .counter = &counter, .iters = ITERS};
    rc_thread threads[THREADS];
    for (int i = 0; i < THREADS; ++i)
        RC_CHECK_TRUE(rc_thread_create(&threads[i], mutex_worker, &ctx));
    for (int i = 0; i < THREADS; ++i)
        rc_thread_join(&threads[i]);
    RC_CHECK(counter, ==, (int64_t)THREADS * ITERS);
    rc_mutex_deinit(&m);
}

/* ---- spinlock mutual exclusion ---- */

typedef struct spin_ctx {
    rc_spinlock *s;
    int64_t     *counter;
    int          iters;
} spin_ctx;

static void spin_worker(void *p)
{
    spin_ctx *c = p;
    for (int i = 0; i < c->iters; ++i) {
        rc_spinlock_lock(c->s);
        ++*c->counter;
        rc_spinlock_unlock(c->s);
    }
}

RC_TEST(thread, spinlock_mutual_exclusion)
{
    rc_spinlock s = {0};
    int64_t counter = 0;
    // Fewer iterations: a spinlock burns CPU under contention.
    spin_ctx ctx = {.s = &s, .counter = &counter, .iters = ITERS / 10};
    rc_thread threads[THREADS];
    for (int i = 0; i < THREADS; ++i)
        RC_CHECK_TRUE(rc_thread_create(&threads[i], spin_worker, &ctx));
    for (int i = 0; i < THREADS; ++i)
        rc_thread_join(&threads[i]);
    RC_CHECK(counter, ==, (int64_t)THREADS * (ITERS / 10));
}

/* ---- rwlock: writer exclusion and reader parallelism ---- */

typedef struct rwlock_ctx {
    rc_rwlock     *rw;
    int64_t       *counter;      // written under the write lock
    int            iters;
    rc_atomic_i32 *active_readers;
    rc_atomic_i32 *peak_readers;
} rwlock_ctx;

static void rwlock_writer(void *p)
{
    rwlock_ctx *c = p;
    for (int i = 0; i < c->iters; ++i) {
        rc_rwlock_write_lock(c->rw);
        ++*c->counter;
        rc_rwlock_write_unlock(c->rw);
    }
}

static void rwlock_reader(void *p)
{
    rwlock_ctx *c = p;
    rc_rwlock_read_lock(c->rw);
    int32_t now = rc_atomic_i32_fetch_add(c->active_readers, 1, RC_MEMORY_ORDER_SEQ_CST) + 1;
    // Record the peak number of readers seen holding the lock simultaneously.
    int32_t peak = rc_atomic_i32_load(c->peak_readers, RC_MEMORY_ORDER_SEQ_CST);
    while (now > peak &&
           !rc_atomic_i32_compare_exchange_weak(c->peak_readers, &peak, now,
               RC_MEMORY_ORDER_SEQ_CST, RC_MEMORY_ORDER_SEQ_CST)) {
    }
    rc_thread_sleep_ns(2 * 1000 * 1000);   // hold long enough for readers to overlap
    rc_atomic_i32_fetch_sub(c->active_readers, 1, RC_MEMORY_ORDER_SEQ_CST);
    rc_rwlock_read_unlock(c->rw);
}

RC_TEST(thread, rwlock_writer_exclusion)
{
    rc_rwlock rw;
    rc_rwlock_init(&rw);
    int64_t counter = 0;
    rwlock_ctx ctx = {.rw = &rw, .counter = &counter, .iters = ITERS / 10};
    rc_thread threads[THREADS];
    for (int i = 0; i < THREADS; ++i)
        RC_CHECK_TRUE(rc_thread_create(&threads[i], rwlock_writer, &ctx));
    for (int i = 0; i < THREADS; ++i)
        rc_thread_join(&threads[i]);
    RC_CHECK(counter, ==, (int64_t)THREADS * (ITERS / 10));
    rc_rwlock_deinit(&rw);
}

RC_TEST(thread, rwlock_reader_parallelism)
{
    rc_rwlock rw;
    rc_rwlock_init(&rw);
    rc_atomic_i32 active = {0};
    rc_atomic_i32 peak = {0};
    rwlock_ctx ctx = {.rw = &rw, .active_readers = &active, .peak_readers = &peak};
    rc_thread threads[THREADS];
    for (int i = 0; i < THREADS; ++i)
        RC_CHECK_TRUE(rc_thread_create(&threads[i], rwlock_reader, &ctx));
    for (int i = 0; i < THREADS; ++i)
        rc_thread_join(&threads[i]);
    // Readers hold the shared lock concurrently, so more than one overlapped.
    RC_CHECK_TRUE(rc_atomic_i32_load(&peak, RC_MEMORY_ORDER_SEQ_CST) >= 2);
    rc_rwlock_deinit(&rw);
}

/* ---- condition variable handshake ---- */

typedef struct cond_ctx {
    rc_mutex m;
    rc_cond  c;
    bool     ready;
    int      value;
} cond_ctx;

static void cond_worker(void *p)
{
    cond_ctx *s = p;
    rc_mutex_lock(&s->m);
    while (!s->ready)
        rc_cond_wait(&s->c, &s->m);
    s->value = 42;
    rc_mutex_unlock(&s->m);
}

RC_TEST(thread, cond_handshake)
{
    cond_ctx s = {.ready = false, .value = 0};
    rc_mutex_init(&s.m);
    rc_cond_init(&s.c);

    rc_thread t;
    RC_CHECK_TRUE(rc_thread_create(&t, cond_worker, &s));
    rc_thread_sleep_ns(10 * 1000 * 1000);   // let the worker reach the wait

    rc_mutex_lock(&s.m);
    s.ready = true;
    rc_cond_signal(&s.c);
    rc_mutex_unlock(&s.m);

    rc_thread_join(&t);
    RC_CHECK(s.value, ==, 42);

    rc_cond_deinit(&s.c);
    rc_mutex_deinit(&s.m);
}

/* ---- semaphore handshake ---- */

typedef struct sem_ctx {
    rc_semaphore  *sem;
    rc_atomic_i32 *flag;
} sem_ctx;

static void sem_worker(void *p)
{
    sem_ctx *c = p;
    rc_semaphore_wait(c->sem);
    rc_atomic_i32_store(c->flag, 1, RC_MEMORY_ORDER_SEQ_CST);
}

RC_TEST(thread, semaphore_handshake)
{
    rc_semaphore sem;
    rc_semaphore_init(&sem, 0);
    rc_atomic_i32 flag = {0};
    sem_ctx ctx = {.sem = &sem, .flag = &flag};

    rc_thread t;
    RC_CHECK_TRUE(rc_thread_create(&t, sem_worker, &ctx));
    rc_thread_sleep_ns(10 * 1000 * 1000);
    // The worker cannot set the flag until it acquires the (empty) semaphore.
    RC_CHECK(rc_atomic_i32_load(&flag, RC_MEMORY_ORDER_SEQ_CST), ==, 0);

    rc_semaphore_post(&sem, 1);
    rc_thread_join(&t);
    RC_CHECK(rc_atomic_i32_load(&flag, RC_MEMORY_ORDER_SEQ_CST), ==, 1);
    rc_semaphore_deinit(&sem);
}

/* ---- run-once ---- */

static rc_atomic_i32 once_counter;

static void once_fn(void)
{
    rc_atomic_i32_fetch_add(&once_counter, 1, RC_MEMORY_ORDER_SEQ_CST);
}

static void once_worker(void *p)
{
    rc_once_run((rc_once *)p, once_fn);
}

RC_TEST(thread, once_runs_exactly_once)
{
    once_counter = (rc_atomic_i32) {0};
    rc_once once = {0};
    rc_thread threads[THREADS];
    for (int i = 0; i < THREADS; ++i)
        RC_CHECK_TRUE(rc_thread_create(&threads[i], once_worker, &once));
    for (int i = 0; i < THREADS; ++i)
        rc_thread_join(&threads[i]);
    RC_CHECK(rc_atomic_i32_load(&once_counter, RC_MEMORY_ORDER_SEQ_CST), ==, 1);
}

/* ---- RC_THREAD_LOCAL isolation ---- */

static RC_THREAD_LOCAL int tls_slot;

typedef struct tls_ctx {
    int  set;
    int *out;
} tls_ctx;

static void tls_worker(void *p)
{
    tls_ctx *c = p;
    tls_slot = c->set;
    rc_thread_sleep_ns(5 * 1000 * 1000);   // all threads write before any reads back
    *c->out = tls_slot;
}

RC_TEST(thread, thread_local_isolation)
{
    enum { N = 4 };
    rc_thread threads[N];
    tls_ctx ctx[N];
    int out[N];
    for (int i = 0; i < N; ++i) {
        ctx[i] = (tls_ctx) {.set = i * 10 + 1, .out = &out[i]};
        RC_CHECK_TRUE(rc_thread_create(&threads[i], tls_worker, &ctx[i]));
    }
    for (int i = 0; i < N; ++i)
        rc_thread_join(&threads[i]);
    for (int i = 0; i < N; ++i)
        RC_CHECK(out[i], ==, i * 10 + 1);   // each thread saw only its own value
}

/* ---- detach ---- */

static void detach_worker(void *p)
{
    rc_thread_sleep_ns(1 * 1000 * 1000);
    rc_atomic_i32_store((rc_atomic_i32 *)p, 1, RC_MEMORY_ORDER_SEQ_CST);
}

RC_TEST(thread, detach)
{
    rc_atomic_i32 done = {0};
    rc_thread t;
    RC_CHECK_TRUE(rc_thread_create(&t, detach_worker, &done));
    rc_thread_detach(&t);

    // t stays live (this frame) until we observe completion; bound the wait.
    uint64_t start = rc_time_now_ns();
    while (rc_atomic_i32_load(&done, RC_MEMORY_ORDER_SEQ_CST) == 0 &&
           rc_time_now_ns() - start < 2000000000ull)
        rc_thread_yield();
    RC_CHECK(rc_atomic_i32_load(&done, RC_MEMORY_ORDER_SEQ_CST), ==, 1);
}

/* ---- misc utilities ---- */

typedef struct id_ctx {
    uint64_t id;
} id_ctx;

static void id_worker(void *p)
{
    ((id_ctx *)p)->id = rc_thread_current_id();
}

RC_TEST(thread, current_id_distinct)
{
    uint64_t main_id = rc_thread_current_id();
    id_ctx a = {0};
    id_ctx b = {0};
    rc_thread ta;
    rc_thread tb;
    RC_CHECK_TRUE(rc_thread_create(&ta, id_worker, &a));
    RC_CHECK_TRUE(rc_thread_create(&tb, id_worker, &b));
    rc_thread_join(&ta);
    rc_thread_join(&tb);
    RC_CHECK_TRUE(a.id != b.id);
    RC_CHECK_TRUE(a.id != main_id);
    RC_CHECK_TRUE(rc_thread_current_id() == main_id);   // stable within a thread
}

RC_TEST(thread, sleep_elapses)
{
    uint64_t start = rc_time_now_ns();
    rc_thread_sleep_ns(10 * 1000 * 1000);
    uint64_t elapsed = rc_time_now_ns() - start;
    RC_CHECK_TRUE(elapsed >= 8 * 1000 * 1000);
}

RC_TEST(thread, hardware_concurrency_positive)
{
    RC_CHECK_TRUE(rc_thread_hardware_concurrency() >= 1);
}
