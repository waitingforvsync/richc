/*
 * thread/cond.h - condition variable.
 *
 * A condition variable lets a thread wait, with the associated rc_mutex
 * released, until another thread signals a change. The classic pattern:
 *
 *   rc_mutex_lock(&m);
 *   while (!predicate)
 *       rc_cond_wait(&c, &m);      // atomically unlocks m, sleeps, re-locks m
 *   ... predicate now holds, m held ...
 *   rc_mutex_unlock(&m);
 *
 * Always re-check the predicate in a loop: waits may wake spuriously, and
 * rc_cond_wait_for returns on either a signal or a timeout. It pairs only with
 * the non-recursive rc_mutex.
 *
 * Initialised in place; see thread/mutex.h for the rationale. The OS condition
 * type is obtained transitively through mutex.h (which pulls in the platform
 * header for the lock it embeds).
 */

#ifndef RC_THREAD_COND_H_
#define RC_THREAD_COND_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/thread/mutex.h"

typedef struct rc_cond {
#if defined(_WIN32)
    CONDITION_VARIABLE handle_;
#else
    pthread_cond_t handle_;
#endif
} rc_cond;

void rc_cond_init(rc_cond *c);
void rc_cond_deinit(rc_cond *c);

/* Wait until signalled (may also wake spuriously). m must be held on entry and
 * is held again on return. */
void rc_cond_wait(rc_cond *c, rc_mutex *m);

/* Like rc_cond_wait but gives up after timeout_ns. Returns true if woken by a
 * signal (or spuriously), false if the timeout elapsed. m is held on return
 * either way. */
bool rc_cond_wait_for(rc_cond *c, rc_mutex *m, uint64_t timeout_ns);

void rc_cond_signal(rc_cond *c);      /* wake at least one waiter */
void rc_cond_broadcast(rc_cond *c);   /* wake all waiters */

#endif /* RC_THREAD_COND_H_ */
