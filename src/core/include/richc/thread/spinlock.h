/*
 * thread/spinlock.h - lightweight busy-wait lock.
 *
 * A minimal test-and-set lock built directly on rc_atomic_flag: no OS object,
 * header-only. Intended for very short critical sections where the cost of a
 * full rc_mutex (a syscall on contention) would dominate. A contended waiter
 * spins on the CPU rather than sleeping, so do NOT hold it across a blocking
 * call or any lengthy work.
 *
 * A zero-initialised rc_spinlock is a valid, unlocked lock; no init call needed.
 */

#ifndef RC_THREAD_SPINLOCK_H_
#define RC_THREAD_SPINLOCK_H_

#include <stdbool.h>

#include "richc/thread/atomic.h"

typedef struct rc_spinlock {
    rc_atomic_flag flag_;
} rc_spinlock;

/* Acquire the lock, spinning until it is free. */
static inline void rc_spinlock_lock(rc_spinlock *s)
{
    while (rc_atomic_flag_test_and_set(&s->flag_, RC_MEMORY_ORDER_ACQUIRE)) {
        // Spin until the holder clears the flag.
    }
}

/* Try to acquire without spinning; return true if the lock was taken. */
static inline bool rc_spinlock_trylock(rc_spinlock *s)
{
    return !rc_atomic_flag_test_and_set(&s->flag_, RC_MEMORY_ORDER_ACQUIRE);
}

/* Release the lock. */
static inline void rc_spinlock_unlock(rc_spinlock *s)
{
    rc_atomic_flag_clear(&s->flag_, RC_MEMORY_ORDER_RELEASE);
}

#endif /* RC_THREAD_SPINLOCK_H_ */
