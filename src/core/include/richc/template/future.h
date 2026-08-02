/*
 * future.h - template header: a typed future over a scheduler task.
 *
 * A future is a handle to a submitted task whose result the caller will collect.
 * Because outstanding tasks live in the scheduler's bounded pool, a future needs
 * no separate allocation or reference counting: it is just the scheduler plus the
 * task handle, and the result lives in a fixed inline buffer in the task's slot.
 * There is deliberately no separate "promise" type - the slot IS the shared
 * state, so the producer side is simply a typed write into it.
 *
 * Define before including:
 *   RC_FUTURE_TYPE   result type (required)
 *   RC_FUTURE_NAME   base name for the generated type and functions
 *                    (optional; default is the spelling of RC_FUTURE_TYPE, which
 *                    must then be a single identifier).
 *
 * The result goes to caller-owned storage passed as `out`. Passing out = NULL
 * uses a fixed inline buffer in the task's slot instead, which then requires
 * RC_FUTURE_TYPE to be no larger than RC_TASK_RESULT_SIZE (checked at run time).
 * All macros defined before inclusion are undefined by this header.
 *
 * Generated (all static inline), for RC_FUTURE_TYPE int / RC_FUTURE_NAME int:
 *   rc_future_int                                                  the future
 *
 *   // submit a task and get a future for its result; out is caller storage
 *   // (NULL => the inline slot buffer):
 *   rc_future_int  rc_scheduler_run_int(rc_scheduler *s, rc_task_func fn, void *ctx, int *out)
 *
 *   // producer side, called from inside the task:
 *   int  *rc_future_int_result(rc_task_context *tc)   // typed ptr to the result storage
 *   void  rc_future_int_set(rc_task_context *tc, int v)
 *
 *   // consumer side, called from the waiter:
 *   int   rc_future_int_get(rc_future_int f)          // wait, then read the result
 *
 * Example:
 *   #define RC_FUTURE_TYPE int
 *   #include "richc/template/future.h"
 *
 *   static void add(rc_task_context *tc, void *ctx) {
 *       args *in = ctx;
 *       rc_future_int_set(tc, in->a + in->b);
 *   }
 *   int result;
 *   rc_future_int f = rc_scheduler_run_int(sched, add, &in, &result);
 *   int sum = rc_future_int_get(f);   // or pass NULL for out to use the slot buffer
 */

#ifndef RC_TEMPLATE_FUTURE_H_
#define RC_TEMPLATE_FUTURE_H_

#include "richc/thread/scheduler.h"   // also provides RC_CONCAT, <stdint.h>

#endif /* RC_TEMPLATE_FUTURE_H_ */

/* ===== per-type section ===== */

#ifndef RC_FUTURE_TYPE
#  define RC_FUTURE_TYPE int   // to keep intellisense happy
#  error "RC_FUTURE_TYPE must be defined before including richc/template/future.h"
#endif

#ifndef RC_FUTURE_NAME
#  define RC_FUTURE_NAME RC_FUTURE_TYPE
#endif

/* ---- generated names ---- */

#define RC_FUTURE_        RC_CONCAT(rc_future_, RC_FUTURE_NAME)
#define RC_FUTURE_RUN_    RC_CONCAT(rc_scheduler_run_, RC_FUTURE_NAME)
#define RC_FUTURE_RESULT_ RC_CONCAT(RC_FUTURE_, _result)
#define RC_FUTURE_SET_    RC_CONCAT(RC_FUTURE_, _set)
#define RC_FUTURE_GET_    RC_CONCAT(RC_FUTURE_, _get)

/* ---- generated type ---- */

typedef struct RC_FUTURE_ {
    rc_scheduler   *scheduler;
    rc_task         task;
    RC_FUTURE_TYPE *out;   /* caller storage, or NULL to use the inline slot buffer */
} RC_FUTURE_;

/* ---- submit ---- */

/* Submit a task and return a future for its result. `out` is caller-owned result
 * storage (kept alive until the get below); pass NULL to use the task's inline slot
 * buffer instead, which requires sizeof(RC_FUTURE_TYPE) <= RC_TASK_RESULT_SIZE
 * (checked at run time by the core). The task fn writes its result with
 * RC_FUTURE_SET_ / RC_FUTURE_RESULT_ either way. */
static inline RC_FUTURE_ RC_FUTURE_RUN_(rc_scheduler *s, rc_task_func fn, void *ctx, RC_FUTURE_TYPE *out)
{
    return (RC_FUTURE_) {
        .scheduler = s,
        .task = rc_scheduler_run_future_(s, fn, ctx, out, (uint32_t)sizeof(RC_FUTURE_TYPE)),
        .out = out,
    };
}

/* ---- producer side (call from inside the task) ---- */

/* Typed pointer into the running task's result slot (build a struct in place). */
static inline RC_FUTURE_TYPE *RC_FUTURE_RESULT_(rc_task_context *tc)
{
    return (RC_FUTURE_TYPE *)rc_task_context_result(tc);
}

/* Store a scalar result. */
static inline void RC_FUTURE_SET_(rc_task_context *tc, RC_FUTURE_TYPE value)
{
    *(RC_FUTURE_TYPE *)rc_task_context_result(tc) = value;
}

/* ---- consumer side ---- */

/* Wait for the task and return its result. With caller storage the result is read
 * straight from *out; otherwise it is copied out of the inline slot buffer, which
 * also releases the slot. */
static inline RC_FUTURE_TYPE RC_FUTURE_GET_(RC_FUTURE_ f)
{
    if (f.out) {
        rc_scheduler_wait(f.scheduler, f.task);
        return *f.out;
    }
    RC_FUTURE_TYPE value;
    rc_scheduler_get_result_(f.scheduler, f.task, &value, (uint32_t)sizeof value);
    return value;
}

/* ---- cleanup ---- */

#undef RC_FUTURE_
#undef RC_FUTURE_RUN_
#undef RC_FUTURE_RESULT_
#undef RC_FUTURE_SET_
#undef RC_FUTURE_GET_

#undef RC_FUTURE_NAME
#undef RC_FUTURE_TYPE
