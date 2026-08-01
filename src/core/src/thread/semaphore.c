#include "richc/thread/semaphore.h"

#include "richc/macros.h"

/*
 * OS counting semaphore. Windows uses a semaphore HANDLE; Linux a POSIX sem_t;
 * Apple a Grand Central Dispatch semaphore (POSIX unnamed semaphores are
 * unsupported there). The Linux timed wait is built on sem_timedwait, whose
 * deadline is on CLOCK_REALTIME, so a wall-clock adjustment can lengthen or
 * shorten an outstanding timeout - unavoidable with the POSIX semaphore API.
 */

#if defined(_WIN32)
#  include <limits.h>
#elif defined(__linux__)
#  include <errno.h>
#  include <time.h>
#endif

void rc_semaphore_init(rc_semaphore *s, uint32_t initial_count)
{
    RC_ASSERT(s);
#if defined(_WIN32)
    s->handle_ = CreateSemaphoreW(NULL, (LONG)initial_count, LONG_MAX, NULL);
    RC_PANIC(s->handle_ != NULL);
#elif defined(__APPLE__)
    s->handle_ = dispatch_semaphore_create((long)initial_count);
    RC_PANIC(s->handle_ != NULL);
#else
    int rc = sem_init(&s->handle_, 0, (unsigned)initial_count);
    RC_PANIC(rc == 0);
#endif
}

void rc_semaphore_deinit(rc_semaphore *s)
{
    RC_ASSERT(s);
#if defined(_WIN32)
    CloseHandle(s->handle_);
#elif defined(__APPLE__)
    dispatch_release(s->handle_);
#else
    int rc = sem_destroy(&s->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_semaphore_wait(rc_semaphore *s)
{
    RC_ASSERT(s);
#if defined(_WIN32)
    DWORD r = WaitForSingleObject(s->handle_, INFINITE);
    RC_PANIC(r == WAIT_OBJECT_0);
#elif defined(__APPLE__)
    (void)dispatch_semaphore_wait(s->handle_, DISPATCH_TIME_FOREVER);
#else
    int rc;
    while ((rc = sem_wait(&s->handle_)) != 0)
        RC_PANIC(errno == EINTR);   // retry if interrupted by a signal
#endif
}

bool rc_semaphore_try_wait(rc_semaphore *s)
{
    RC_ASSERT(s);
#if defined(_WIN32)
    DWORD r = WaitForSingleObject(s->handle_, 0);
    if (r == WAIT_OBJECT_0) return true;
    RC_PANIC(r == WAIT_TIMEOUT);
    return false;
#elif defined(__APPLE__)
    return dispatch_semaphore_wait(s->handle_, DISPATCH_TIME_NOW) == 0;
#else
    for (;;) {
        if (sem_trywait(&s->handle_) == 0) return true;
        if (errno == EAGAIN) return false;
        RC_PANIC(errno == EINTR);   // retry only on interruption
    }
#endif
}

bool rc_semaphore_wait_for(rc_semaphore *s, uint64_t timeout_ns)
{
    RC_ASSERT(s);
#if defined(_WIN32)
    uint64_t ms = (timeout_ns + 999999ull) / 1000000ull;   // round up to whole ms
    if (ms > INFINITE - 1) ms = INFINITE - 1;
    DWORD r = WaitForSingleObject(s->handle_, (DWORD)ms);
    if (r == WAIT_OBJECT_0) return true;
    RC_PANIC(r == WAIT_TIMEOUT);
    return false;
#elif defined(__APPLE__)
    dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeout_ns);
    return dispatch_semaphore_wait(s->handle_, deadline) == 0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);   // sem_timedwait deadlines are on CLOCK_REALTIME
    uint64_t ns = (uint64_t)ts.tv_nsec + timeout_ns;
    ts.tv_sec += (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    for (;;) {
        if (sem_timedwait(&s->handle_, &ts) == 0) return true;
        if (errno == ETIMEDOUT) return false;
        RC_PANIC(errno == EINTR);   // retry with the same absolute deadline
    }
#endif
}

void rc_semaphore_post(rc_semaphore *s, uint32_t count)
{
    RC_ASSERT(s);
#if defined(_WIN32)
    BOOL ok = ReleaseSemaphore(s->handle_, (LONG)count, NULL);
    RC_PANIC(ok);
#elif defined(__APPLE__)
    for (uint32_t i = 0; i < count; ++i)
        (void)dispatch_semaphore_signal(s->handle_);
#else
    for (uint32_t i = 0; i < count; ++i) {
        int rc = sem_post(&s->handle_);
        RC_PANIC(rc == 0);
    }
#endif
}
