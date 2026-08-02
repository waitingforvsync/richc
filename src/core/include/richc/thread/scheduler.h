/*
 * thread/scheduler.h - task-graph thread pool.
 *
 * A scheduler owns a set of worker threads and a bounded pool of task slots. You
 * create tasks, optionally wire them into a dependency graph, submit them, and
 * wait for completion; typed results come back through the future template in
 * richc/template/future.h.
 *
 * Allocation model
 * ----------------
 * Like everything in richc the scheduler never owns its allocation: create()
 * takes an rc_arena and carves the scheduler and all its buffers from it in one
 * shot. The block never grows, so it may share an arena. The returned pointer is
 * stable for the arena's lifetime (the reserved VA range never moves), which the
 * worker threads rely on. deinit() stops and joins the workers and destroys the
 * per-worker scratch arenas, but never frees the block itself - the arena owner
 * reclaims that on reset/deinit. (It returns a pointer, hence _create rather than
 * _make, which richc reserves for by-value constructors.)
 *
 * Tasks and the dependency graph
 * ------------------------------
 * A task is a function plus a void* context. rc_scheduler_task_make creates one
 * holding a single "self-hold"; rc_task_after(before, after) makes `after` wait
 * for `before`; rc_scheduler_submit releases the self-hold. A task runs once it
 * has been submitted and every predecessor has completed, so wiring order versus
 * submit order does not matter. rc_scheduler_run is make + submit with no deps.
 * Wire a task's edges before you submit it (from one thread, or synchronise the
 * wiring against the submit yourself).
 *
 * The task function receives an rc_task_context: the scheduler (to spawn child
 * tasks), a per-worker scratch arena that is reset after the task returns, the
 * task's own handle, and the worker index.
 *
 * Waiting
 * -------
 * rc_scheduler_wait blocks until a task completes. Called from inside a task (on
 * a worker) it *participates* - it runs other ready tasks while it waits - so
 * recursive fork/join keeps the cores busy and cannot deadlock the pool. Called
 * from an outside thread (e.g. main) it simply blocks. rc_scheduler_wait_all
 * blocks (from an outside thread) until all outstanding work has drained.
 *
 * Results
 * -------
 * A task writes its result through rc_task_context_result (or the typed future
 * template's _result/_set). By default that points at caller-provided storage:
 * the future's run_T takes an `out` pointer and the result lands there, so the
 * caller decides where it lives (any size). Passing out = NULL falls back to a
 * fixed inline buffer in the task's slot (bounded by RC_TASK_RESULT_SIZE, drawn
 * from the pool the caller's arena already backs) - convenient when you would
 * rather not manage result storage yourself. A task can also just write through
 * a caller struct reached via its ctx and skip futures entirely.
 */

#ifndef RC_THREAD_SCHEDULER_H_
#define RC_THREAD_SCHEDULER_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/macros.h"

typedef struct rc_arena rc_arena;

/* ---- handles and the task function ---- */

typedef struct rc_scheduler rc_scheduler;   /* opaque; defined in scheduler.c */

/*
 * A task handle: a slot index plus the generation the slot held when the task was
 * created. The generation lets a stale handle (to a slot since freed and reused)
 * be detected rather than silently aliasing a different task.
 */
typedef struct rc_task {
    uint32_t slot;
    uint32_t generation;
} rc_task;

#define RC_TASK_NONE ((rc_task) {.slot = RC_INDEX_NONE})

typedef struct rc_task_context rc_task_context;
typedef void (*rc_task_func)(rc_task_context *tc, void *ctx);

struct rc_task_context {
    rc_scheduler *scheduler;    /* spawn/submit child tasks */
    rc_arena     *scratch;      /* this worker's scratch arena; reset after the task returns */
    rc_task       self;         /* the running task's own handle */
    uint32_t      worker_index;
};

/*
 * Size in bytes of each task's inline result buffer. A fixed part of the ABI
 * (the scheduler and the future template must agree on it), so it is not
 * overridable per translation unit. Larger results use the ctx path instead.
 */
#define RC_TASK_RESULT_SIZE 64u

/* ---- configuration ---- */

typedef struct rc_scheduler_config {
    uint32_t num_threads;      /* 0 => rc_thread_hardware_concurrency() - 1 (at least 1) */
    uint32_t max_tasks;        /* task-pool capacity; the bound on outstanding tasks */
    uint32_t scratch_reserve;  /* per-worker scratch arena VA reserve in bytes (0 => a default) */
} rc_scheduler_config;

/* ---- lifecycle ---- */

/* Create a scheduler and its buffers from `arena`, and start the worker threads.
 * The returned pointer is stable for the arena's lifetime. */
rc_scheduler *rc_scheduler_create(rc_scheduler_config config, rc_arena *arena);

/* Stop and join the workers and destroy the scratch arenas. Does not free the
 * scheduler block (the arena owner reclaims it). */
void rc_scheduler_deinit(rc_scheduler *s);

/* ---- create / wire / submit ---- */

/* Create a task (not yet runnable) holding a single self-hold. */
rc_task rc_scheduler_task_make(rc_scheduler *s, rc_task_func fn, void *ctx);

/* Make `run_after` wait for `before` to complete. Wire before submitting. */
void rc_task_after(rc_scheduler *s, rc_task before, rc_task run_after);

/* Release the self-hold; the task runs once all its predecessors have completed. */
void rc_scheduler_submit(rc_scheduler *s, rc_task task);

/* make + submit with no dependencies (fire-and-forget). */
rc_task rc_scheduler_run(rc_scheduler *s, rc_task_func fn, void *ctx);

/* ---- waiting ---- */

/* Wait for `task` to complete. On a worker this participates in running ready
 * tasks; on an outside thread it blocks. */
void rc_scheduler_wait(rc_scheduler *s, rc_task task);

/* Wait (from an outside thread) until all submitted work has completed. */
void rc_scheduler_wait_all(rc_scheduler *s);

/* ---- results ---- */

/* Pointer to where the running task's result goes - the caller's `out` storage if
 * one was given to the future, otherwise the inline slot buffer. Call from inside
 * the task; the typed future accessors build on this. */
void *rc_task_context_result(rc_task_context *tc);

/* ---- internals used by the future template (trailing _ = exposed but private) ---- */

/* Submit a future task and return its handle. `out` chooses the result storage:
 * non-NULL is caller-owned (any size; the task is detached, its result read
 * straight from *out), NULL uses the inline slot buffer (result_size must be
 * <= RC_TASK_RESULT_SIZE) and retains the slot until rc_scheduler_get_result_. */
rc_task rc_scheduler_run_future_(rc_scheduler *s, rc_task_func fn, void *ctx, void *out, uint32_t result_size);

/* Collect an inline-buffer result: wait for `task`, copy `size` bytes into `dst`,
 * then drop the retain hold (freeing the slot). Only for the out == NULL path. */
void rc_scheduler_get_result_(rc_scheduler *s, rc_task task, void *dst, uint32_t size);

#endif /* RC_THREAD_SCHEDULER_H_ */
