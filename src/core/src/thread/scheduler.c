#include "richc/thread/scheduler.h"

#include <string.h>

#include "richc/arena.h"
#include "richc/thread/atomic.h"
#include "richc/thread/cond.h"
#include "richc/thread/mutex.h"
#include "richc/thread/semaphore.h"
#include "richc/thread/thread.h"
#include "richc/thread/tls.h"

/*
 * Task-graph thread pool. See scheduler.h for the model; this file holds the
 * private slot layout, the single global run queue, and the completion protocol.
 *
 * Concurrency in one place
 * ------------------------
 * The result buffer is plain bytes published by a release-store / acquire-load
 * pair on slot->state (PENDING -> COMPLETE): everything the task wrote before the
 * release-store is visible to any thread that observes COMPLETE. A predecessor's
 * dep_count fetch_sub is acq_rel and the newly runnable successor reaches its
 * worker through the queue mutex (enqueue releases, dequeue acquires), so a
 * successor sees whatever its predecessors wrote. A slot is freed only when its
 * retain count hits zero, and complete() reads slot->successors before its own
 * final retain drop, so a concurrent collector can never free the slot out from
 * under it; the generation field traps a stale handle used after free.
 */

enum {
    RC_TASK_MAX_SUCCESSORS_ = 16,   /* inline fan-out per task; wider fan-out asserts */
};

enum rc_task_state_ {
    RC_TASK_PENDING_ = 0,
    RC_TASK_COMPLETE_ = 1,
};

/* Default per-worker scratch reserve when the config leaves it 0. */
#define RC_SCHEDULER_SCRATCH_DEFAULT_ ((uint32_t)16 * 1024 * 1024)

/* How long a participating waiter parks on the work semaphore between re-checks of
 * its target when no task is ready (a new task also posts the semaphore, so this is
 * only an upper bound on the re-check latency, not added latency on the happy path). */
#define RC_SCHEDULER_PARK_NS_ ((uint64_t)500 * 1000)   /* 500 us */

typedef struct rc_task_slot_ {
    rc_task_func  fn;
    void         *ctx;
    rc_atomic_u32 dep_count;    /* unfinished predecessors + a self-hold; runnable at 0 */
    rc_atomic_u32 retain;       /* lifecycle holds; slot is freed at 0 */
    rc_atomic_u32 state;        /* rc_task_state_; release on set, acquire on read */
    rc_atomic_u32 generation;   /* bumped on free; validates handles */
    uint32_t      num_successors;
    uint32_t      successors[RC_TASK_MAX_SUCCESSORS_];
    uint32_t      next_free_;   /* free-list link (index, or RC_INDEX_NONE); under free_lock */
    void         *result_ptr;   /* where the result goes: caller storage, or &result below */
    _Alignas(RC_MAX_ALIGN) unsigned char result[RC_TASK_RESULT_SIZE];   /* inline fallback */
} rc_task_slot_;

struct rc_scheduler {
    rc_arena         *arena;        /* not owned; where this and the buffers live */
    uint32_t          num_threads;
    uint32_t          max_tasks;

    rc_task_slot_    *slots;        /* [max_tasks] */

    /* slot free list */
    rc_mutex          free_mutex;   /* a blocking lock, not a spinlock: under heavy
                                        alloc/free churn a spinlock live-locks (and is
                                        pathological under TSan, which serialises threads) */
    uint32_t          free_head;    /* index, or RC_INDEX_NONE when empty */

    /* global run queue: a ring of runnable slot indices */
    rc_mutex          queue_mutex;
    uint32_t         *ring;         /* [max_tasks] */
    uint32_t          ring_head;
    uint32_t          ring_tail;
    uint32_t          ring_count;
    rc_semaphore      work_sem;     /* wakes blocked workers; counts posted work */
    rc_atomic_bool    stopping;

    /* completion signalling for outside (blocking) waiters */
    rc_mutex          completion_mutex;
    rc_cond           completion_cond;
    rc_atomic_u32     outstanding;        /* submitted-but-not-complete tasks */

    /* workers */
    rc_thread        *workers;      /* [num_threads] */
    rc_task_context  *worker_ctx;   /* [num_threads] */
    rc_arena         *scratch;      /* [num_threads] */
    rc_tls            worker_tls;   /* holds this thread's rc_task_context*, else NULL */
};

/* ---- slot pool ---- */

static uint32_t rc_scheduler_alloc_slot_(rc_scheduler *s)
{
    rc_mutex_lock(&s->free_mutex);
    uint32_t idx = s->free_head;
    RC_PANIC(idx != RC_INDEX_NONE);   /* pool exhausted: size max_tasks to peak outstanding */
    s->free_head = s->slots[idx].next_free_;
    rc_mutex_unlock(&s->free_mutex);
    return idx;
}

static void rc_scheduler_free_slot_(rc_scheduler *s, uint32_t idx)
{
    rc_mutex_lock(&s->free_mutex);
    // Bump the generation first so any handle to this slot is now stale, then
    // return it to the free list for reuse.
    rc_atomic_u32_fetch_add(&s->slots[idx].generation, 1, RC_MEMORY_ORDER_RELEASE);
    s->slots[idx].next_free_ = s->free_head;
    s->free_head = idx;
    rc_mutex_unlock(&s->free_mutex);
}

/* ---- run queue ---- */

static void rc_scheduler_enqueue_(rc_scheduler *s, uint32_t idx)
{
    rc_mutex_lock(&s->queue_mutex);
    RC_ASSERT(s->ring_count < s->max_tasks);
    s->ring[s->ring_tail] = idx;
    s->ring_tail = (s->ring_tail + 1) % s->max_tasks;
    ++s->ring_count;
    rc_mutex_unlock(&s->queue_mutex);
    rc_semaphore_post(&s->work_sem, 1);
}

/* Non-blocking pop; RC_INDEX_NONE when the queue is empty. */
static uint32_t rc_scheduler_try_pop_(rc_scheduler *s)
{
    rc_mutex_lock(&s->queue_mutex);
    uint32_t idx = RC_INDEX_NONE;
    if (s->ring_count > 0) {
        idx = s->ring[s->ring_head];
        s->ring_head = (s->ring_head + 1) % s->max_tasks;
        --s->ring_count;
    }
    rc_mutex_unlock(&s->queue_mutex);
    return idx;
}

/* ---- completion signalling ---- */

// Wake outside threads blocked in wait / wait_all; they re-check their own
// predicate. We broadcast unconditionally under the mutex on every completion: the
// mutex hand-off (unlock here -> the waiter's re-lock on return from cond_wait) is
// what makes the state/outstanding update visible and rules out a lost wakeup - a
// waiter cannot slip between its predicate check and its sleep, both of which it
// does holding this mutex. (A count-guarded fast path is unsafe here because a
// detached task can free and recycle its slot, flipping the state a waiter reads;
// an eventcount is the eventual optimisation, noted with the work-stealing queue.)
static void rc_scheduler_wake_waiters_(rc_scheduler *s)
{
    rc_mutex_lock(&s->completion_mutex);
    rc_cond_broadcast(&s->completion_cond);
    rc_mutex_unlock(&s->completion_mutex);
}

static void rc_scheduler_complete_(rc_scheduler *s, uint32_t idx)
{
    rc_task_slot_ *slot = &s->slots[idx];

    // Publish the result and mark complete (release pairs with the acquire in
    // is_done, for the participating waiter that polls without the mutex).
    rc_atomic_u32_store(&slot->state, RC_TASK_COMPLETE_, RC_MEMORY_ORDER_RELEASE);

    // Release successors: whichever predecessor drives dep_count to 0 enqueues it.
    for (uint32_t k = 0; k < slot->num_successors; ++k) {
        uint32_t si = slot->successors[k];
        if (rc_atomic_u32_fetch_sub(&s->slots[si].dep_count, 1, RC_MEMORY_ORDER_ACQ_REL) == 1)
            rc_scheduler_enqueue_(s, si);
    }

    // Execution is done (this drives wait_all's outstanding count to 0).
    rc_atomic_u32_fetch_sub(&s->outstanding, 1, RC_MEMORY_ORDER_ACQ_REL);

    // Wake every blocking waiter once, after state and outstanding are updated;
    // they re-check their own predicate (a specific task done, or all drained).
    rc_scheduler_wake_waiters_(s);

    // Drop the completion hold; free when the last hold goes (after the reads above).
    if (rc_atomic_u32_fetch_sub(&slot->retain, 1, RC_MEMORY_ORDER_ACQ_REL) == 1)
        rc_scheduler_free_slot_(s, idx);
}

/* ---- running tasks ---- */

static void rc_scheduler_run_task_(rc_task_context *tc, uint32_t idx)
{
    rc_scheduler *s = tc->scheduler;
    rc_task_slot_ *slot = &s->slots[idx];

    // Save/restore self and the scratch mark so a task run *nested* inside a
    // participating wait (see rc_scheduler_wait) leaves the outer task's context
    // and scratch high-water mark untouched - the nesting is stack-disciplined.
    rc_task saved_self = tc->self;
    uint32_t mark = tc->scratch->top;

    tc->self = (rc_task) {
        .slot = idx,
        .generation = rc_atomic_u32_load(&slot->generation, RC_MEMORY_ORDER_RELAXED),
    };
    slot->fn(tc, slot->ctx);

    rc_arena_free_to(tc->scratch, mark);
    tc->self = saved_self;

    rc_scheduler_complete_(s, idx);
}

/* ---- worker thread ---- */

static void rc_scheduler_worker_main_(void *arg)
{
    rc_task_context *tc = arg;
    rc_scheduler *s = tc->scheduler;
    rc_tls_set(s->worker_tls, tc);

    for (;;) {
        rc_semaphore_wait(&s->work_sem);

        if (rc_atomic_bool_load(&s->stopping, RC_MEMORY_ORDER_ACQUIRE)) {
            // Drain whatever remains (including successors freed while draining),
            // then exit. Each running task finishes and re-checks before leaving,
            // so no ready work is dropped.
            for (uint32_t idx = rc_scheduler_try_pop_(s); idx != RC_INDEX_NONE;
                 idx = rc_scheduler_try_pop_(s))
                rc_scheduler_run_task_(tc, idx);
            break;
        }

        uint32_t idx = rc_scheduler_try_pop_(s);
        if (idx != RC_INDEX_NONE)
            rc_scheduler_run_task_(tc, idx);
    }
}

/* ---- task creation ---- */

static rc_task rc_scheduler_task_make_impl_(rc_scheduler *s, rc_task_func fn, void *ctx, uint32_t retain)
{
    RC_ASSERT(s);
    RC_ASSERT(fn);
    uint32_t idx = rc_scheduler_alloc_slot_(s);
    rc_task_slot_ *slot = &s->slots[idx];

    // The slot is not shared until submit, so these stores can be relaxed.
    slot->fn = fn;
    slot->ctx = ctx;
    slot->num_successors = 0;
    slot->result_ptr = slot->result;   // default to the inline buffer; run_future_ may override

    rc_atomic_u32_store(&slot->dep_count, 1, RC_MEMORY_ORDER_RELAXED);   // self-hold
    rc_atomic_u32_store(&slot->retain, retain, RC_MEMORY_ORDER_RELAXED);
    rc_atomic_u32_store(&slot->state, RC_TASK_PENDING_, RC_MEMORY_ORDER_RELAXED);

    return (rc_task) {
        .slot = idx,
        .generation = rc_atomic_u32_load(&slot->generation, RC_MEMORY_ORDER_RELAXED),
    };
}

rc_task rc_scheduler_task_make(rc_scheduler *s, rc_task_func fn, void *ctx)
{
    return rc_scheduler_task_make_impl_(s, fn, ctx, 1);   // completion hold only
}

void rc_task_after(rc_scheduler *s, rc_task before, rc_task run_after)
{
    RC_ASSERT(s);
    rc_task_slot_ *b = &s->slots[before.slot];
    rc_task_slot_ *a = &s->slots[run_after.slot];
    RC_ASSERT(rc_atomic_u32_load(&b->generation, RC_MEMORY_ORDER_RELAXED) == before.generation);
    RC_ASSERT(rc_atomic_u32_load(&a->generation, RC_MEMORY_ORDER_RELAXED) == run_after.generation);
    RC_ASSERT(b->num_successors < RC_TASK_MAX_SUCCESSORS_);   // widen the array for wider fan-out

    b->successors[b->num_successors++] = run_after.slot;
    // run_after is not yet runnable (edges are wired before submit), so relaxed.
    rc_atomic_u32_fetch_add(&a->dep_count, 1, RC_MEMORY_ORDER_RELAXED);
}

void rc_scheduler_submit(rc_scheduler *s, rc_task task)
{
    RC_ASSERT(s);
    rc_task_slot_ *slot = &s->slots[task.slot];
    RC_ASSERT(rc_atomic_u32_load(&slot->generation, RC_MEMORY_ORDER_RELAXED) == task.generation);

    // Count as outstanding before it can run and decrement the counter again.
    rc_atomic_u32_fetch_add(&s->outstanding, 1, RC_MEMORY_ORDER_RELAXED);
    if (rc_atomic_u32_fetch_sub(&slot->dep_count, 1, RC_MEMORY_ORDER_ACQ_REL) == 1)
        rc_scheduler_enqueue_(s, task.slot);
}

rc_task rc_scheduler_run(rc_scheduler *s, rc_task_func fn, void *ctx)
{
    rc_task t = rc_scheduler_task_make_impl_(s, fn, ctx, 1);
    rc_scheduler_submit(s, t);
    return t;
}

rc_task rc_scheduler_run_future_(rc_scheduler *s, rc_task_func fn, void *ctx, void *out, uint32_t result_size)
{
    // out != NULL: caller-owned storage of any size, so the task is detached - the
    // result lives in the caller's memory and needs no slot to be kept alive.
    // out == NULL: the inline slot buffer (size-bounded), retained until collected.
    RC_ASSERT(out || result_size <= RC_TASK_RESULT_SIZE);
    (void)result_size;

    rc_task t = rc_scheduler_task_make_impl_(s, fn, ctx, out ? 1 : 2);
    if (out)
        s->slots[t.slot].result_ptr = out;
    rc_scheduler_submit(s, t);
    return t;
}

/* ---- results ---- */

void *rc_task_context_result(rc_task_context *tc)
{
    RC_ASSERT(tc);
    RC_ASSERT(tc->self.slot != RC_INDEX_NONE);
    return tc->scheduler->slots[tc->self.slot].result_ptr;
}

/* ---- waiting ---- */

static bool rc_scheduler_is_done_(rc_scheduler *s, rc_task task)
{
    rc_task_slot_ *slot = &s->slots[task.slot];
    // A reused slot (generation moved on) means the task has long since finished;
    // the acquire pairs with the release in free_slot so its result is visible.
    if (rc_atomic_u32_load(&slot->generation, RC_MEMORY_ORDER_ACQUIRE) != task.generation)
        return true;
    return rc_atomic_u32_load(&slot->state, RC_MEMORY_ORDER_ACQUIRE) == RC_TASK_COMPLETE_;
}

void rc_scheduler_wait(rc_scheduler *s, rc_task task)
{
    RC_ASSERT(s);
    if (task.slot == RC_INDEX_NONE)
        return;

    rc_task_context *tc = rc_tls_get(s->worker_tls);
    if (tc) {
        // On a worker: keep the cores busy by running ready tasks while waiting.
        // Acquire a work-semaphore token first (a bounded wait, so we still re-check
        // the target), THEN pop. This keeps the token count equal to the queue depth:
        // every dequeue consumes exactly one token. Popping without consuming a token
        // (a bare try_pop) would leave "phantom" tokens behind, and those make the
        // wait return instantly - degenerating into a CPU-burning spin that, worse,
        // starves the very task we are waiting on. A newly enqueued task posts the
        // semaphore, so a waiter is woken promptly; the timeout only bounds how long
        // we go before re-checking a target that completes without enqueuing work.
        while (!rc_scheduler_is_done_(s, task)) {
            if (rc_semaphore_wait_for(&s->work_sem, RC_SCHEDULER_PARK_NS_)) {
                uint32_t idx = rc_scheduler_try_pop_(s);
                if (idx != RC_INDEX_NONE)
                    rc_scheduler_run_task_(tc, idx);
                // else the token was a phantom (empty queue); dropping it self-heals.
            }
        }
        return;
    }

    // Outside thread: block on the completion condition (broadcast on every
    // completion), re-checking the predicate under the mutex.
    rc_mutex_lock(&s->completion_mutex);
    while (!rc_scheduler_is_done_(s, task))
        rc_cond_wait(&s->completion_cond, &s->completion_mutex);
    rc_mutex_unlock(&s->completion_mutex);
}

void rc_scheduler_wait_all(rc_scheduler *s)
{
    RC_ASSERT(s);
    RC_ASSERT(!rc_tls_get(s->worker_tls));   // would wait for itself and deadlock

    rc_mutex_lock(&s->completion_mutex);
    while (rc_atomic_u32_load(&s->outstanding, RC_MEMORY_ORDER_ACQUIRE) != 0)
        rc_cond_wait(&s->completion_cond, &s->completion_mutex);
    rc_mutex_unlock(&s->completion_mutex);
}

void rc_scheduler_get_result_(rc_scheduler *s, rc_task task, void *dst, uint32_t size)
{
    // Only the inline-buffer path reaches here (the caller-storage path reads *out
    // directly in the template); such a task is retained, so its slot is still alive.
    RC_ASSERT(size <= RC_TASK_RESULT_SIZE);
    rc_scheduler_wait(s, task);

    rc_task_slot_ *slot = &s->slots[task.slot];
    // The retain hold kept the slot valid, so the handle must still match; a
    // mismatch means a double-collect or a stale handle.
    RC_ASSERT(rc_atomic_u32_load(&slot->generation, RC_MEMORY_ORDER_ACQUIRE) == task.generation);
    memcpy(dst, slot->result, size);

    if (rc_atomic_u32_fetch_sub(&slot->retain, 1, RC_MEMORY_ORDER_ACQ_REL) == 1)
        rc_scheduler_free_slot_(s, task.slot);
}

/* ---- lifecycle ---- */

rc_scheduler *rc_scheduler_create(rc_scheduler_config config, rc_arena *arena)
{
    RC_ASSERT(arena);
    RC_ASSERT(config.max_tasks > 0);

    uint32_t num_threads = config.num_threads;
    if (num_threads == 0) {
        uint32_t hw = rc_thread_hardware_concurrency();
        num_threads = hw > 1 ? hw - 1 : 1;
    }
    uint32_t scratch_reserve = config.scratch_reserve ? config.scratch_reserve
                                                      : RC_SCHEDULER_SCRATCH_DEFAULT_;

    rc_scheduler *s = rc_arena_alloc_zero_type(arena, rc_scheduler, 1);
    s->arena = arena;
    s->num_threads = num_threads;
    s->max_tasks = config.max_tasks;

    s->slots = rc_arena_alloc_zero_type(arena, rc_task_slot_, config.max_tasks);
    s->ring = rc_arena_alloc_type(arena, uint32_t, config.max_tasks);

    // Thread every slot onto the free list (0 -> 1 -> ... -> NONE).
    for (uint32_t i = 0; i < config.max_tasks; ++i)
        s->slots[i].next_free_ = (i + 1 < config.max_tasks) ? (i + 1) : RC_INDEX_NONE;
    s->free_head = 0;

    rc_mutex_init(&s->free_mutex);
    rc_mutex_init(&s->queue_mutex);
    rc_semaphore_init(&s->work_sem, 0);
    rc_mutex_init(&s->completion_mutex);
    rc_cond_init(&s->completion_cond);
    s->worker_tls = rc_tls_create();

    s->workers = rc_arena_alloc_type(arena, rc_thread, num_threads);
    s->worker_ctx = rc_arena_alloc_zero_type(arena, rc_task_context, num_threads);
    s->scratch = rc_arena_alloc_type(arena, rc_arena, num_threads);
    for (uint32_t w = 0; w < num_threads; ++w) {
        s->scratch[w] = rc_arena_make(scratch_reserve);
        RC_PANIC(s->scratch[w].base != NULL);
        s->worker_ctx[w] = (rc_task_context) {
            .scheduler = s,
            .scratch = &s->scratch[w],
            .self = RC_TASK_NONE,
            .worker_index = w,
        };
    }

    // Everything is initialised before any worker can observe it.
    for (uint32_t w = 0; w < num_threads; ++w)
        RC_PANIC(rc_thread_create(&s->workers[w], rc_scheduler_worker_main_, &s->worker_ctx[w]));

    return s;
}

void rc_scheduler_deinit(rc_scheduler *s)
{
    RC_ASSERT(s);

    rc_atomic_bool_store(&s->stopping, true, RC_MEMORY_ORDER_RELEASE);
    rc_semaphore_post(&s->work_sem, s->num_threads);   // wake every worker to see it
    for (uint32_t w = 0; w < s->num_threads; ++w)
        rc_thread_join(&s->workers[w]);

    for (uint32_t w = 0; w < s->num_threads; ++w)
        rc_arena_deinit(&s->scratch[w]);

    rc_tls_destroy(&s->worker_tls);
    rc_cond_deinit(&s->completion_cond);
    rc_mutex_deinit(&s->completion_mutex);
    rc_semaphore_deinit(&s->work_sem);
    rc_mutex_deinit(&s->queue_mutex);
    rc_mutex_deinit(&s->free_mutex);
    // The arena owner reclaims the scheduler block itself.
}
