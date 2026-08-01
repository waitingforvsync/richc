#include "richc/thread/cond.h"

#include "richc/macros.h"

/*
 * OS condition variable. On Windows a CONDITION_VARIABLE drives the SRWLOCK that
 * backs rc_mutex. On POSIX, timed waits need an absolute deadline on the same
 * clock the condition uses: on Linux the condition is switched to CLOCK_MONOTONIC
 * so deadlines are immune to wall-clock changes; on Apple, which cannot retarget
 * the clock, the relative-timeout variant is used instead.
 */

#if defined(__APPLE__) || defined(__linux__)
#  include <errno.h>
#  include <time.h>
#endif

void rc_cond_init(rc_cond *c)
{
    RC_ASSERT(c);
#if defined(_WIN32)
    InitializeConditionVariable(&c->handle_);
#elif defined(__linux__)
    pthread_condattr_t attr;
    RC_PANIC(pthread_condattr_init(&attr) == 0);
    RC_PANIC(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0);
    int rc = pthread_cond_init(&c->handle_, &attr);
    RC_PANIC(rc == 0);
    pthread_condattr_destroy(&attr);
#else /* Apple */
    int rc = pthread_cond_init(&c->handle_, NULL);
    RC_PANIC(rc == 0);
#endif
}

void rc_cond_deinit(rc_cond *c)
{
    RC_ASSERT(c);
#if defined(_WIN32)
    (void)c;   // a CONDITION_VARIABLE requires no destruction
#else
    int rc = pthread_cond_destroy(&c->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_cond_wait(rc_cond *c, rc_mutex *m)
{
    RC_ASSERT(c);
    RC_ASSERT(m);
#if defined(_WIN32)
    BOOL ok = SleepConditionVariableSRW(&c->handle_, &m->handle_, INFINITE, 0);
    RC_PANIC(ok);
#else
    int rc = pthread_cond_wait(&c->handle_, &m->handle_);
    RC_PANIC(rc == 0);
#endif
}

bool rc_cond_wait_for(rc_cond *c, rc_mutex *m, uint64_t timeout_ns)
{
    RC_ASSERT(c);
    RC_ASSERT(m);
#if defined(_WIN32)
    // Round the timeout up to whole milliseconds so we never wake early.
    uint64_t ms = (timeout_ns + 999999ull) / 1000000ull;
    if (ms > INFINITE - 1) ms = INFINITE - 1;
    if (SleepConditionVariableSRW(&c->handle_, &m->handle_, (DWORD)ms, 0))
        return true;
    RC_PANIC(GetLastError() == ERROR_TIMEOUT);
    return false;
#elif defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_nsec + timeout_ns;
    ts.tv_sec += (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    int rc = pthread_cond_timedwait(&c->handle_, &m->handle_, &ts);
    if (rc == 0) return true;
    RC_PANIC(rc == ETIMEDOUT);
    return false;
#else /* Apple: relative timeout, no clock retargeting needed */
    struct timespec rel;
    rel.tv_sec = (time_t)(timeout_ns / 1000000000ull);
    rel.tv_nsec = (long)(timeout_ns % 1000000000ull);
    int rc = pthread_cond_timedwait_relative_np(&c->handle_, &m->handle_, &rel);
    if (rc == 0) return true;
    RC_PANIC(rc == ETIMEDOUT);
    return false;
#endif
}

void rc_cond_signal(rc_cond *c)
{
    RC_ASSERT(c);
#if defined(_WIN32)
    WakeConditionVariable(&c->handle_);
#else
    int rc = pthread_cond_signal(&c->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_cond_broadcast(rc_cond *c)
{
    RC_ASSERT(c);
#if defined(_WIN32)
    WakeAllConditionVariable(&c->handle_);
#else
    int rc = pthread_cond_broadcast(&c->handle_);
    RC_PANIC(rc == 0);
#endif
}
