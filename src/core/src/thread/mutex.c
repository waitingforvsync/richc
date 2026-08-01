#include "richc/thread/mutex.h"

#include "richc/macros.h"

/*
 * OS mutexes. rc_mutex uses the platform's lightest exclusive lock (SRWLOCK on
 * Windows, a default pthread mutex on POSIX); rc_mutex_recursive uses a lock
 * that permits re-entry (CRITICAL_SECTION / a recursive pthread mutex). OS
 * calls that can only fail on programmer error are guarded with RC_PANIC.
 */

#if defined(__APPLE__) || defined(__linux__)
#  include <errno.h>
#endif

/* ---- non-recursive mutex ---- */

void rc_mutex_init(rc_mutex *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    InitializeSRWLock(&m->handle_);
#else
    int rc = pthread_mutex_init(&m->handle_, NULL);
    RC_PANIC(rc == 0);
#endif
}

void rc_mutex_deinit(rc_mutex *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    (void)m;   // an SRWLOCK requires no destruction
#else
    int rc = pthread_mutex_destroy(&m->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_mutex_lock(rc_mutex *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    AcquireSRWLockExclusive(&m->handle_);
#else
    int rc = pthread_mutex_lock(&m->handle_);
    RC_PANIC(rc == 0);
#endif
}

bool rc_mutex_trylock(rc_mutex *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    return TryAcquireSRWLockExclusive(&m->handle_) != 0;
#else
    int rc = pthread_mutex_trylock(&m->handle_);
    if (rc == 0) return true;
    RC_PANIC(rc == EBUSY);   // EBUSY is the only non-acquire outcome we tolerate
    return false;
#endif
}

void rc_mutex_unlock(rc_mutex *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&m->handle_);
#else
    int rc = pthread_mutex_unlock(&m->handle_);
    RC_PANIC(rc == 0);
#endif
}

/* ---- recursive mutex ---- */

void rc_mutex_recursive_init(rc_mutex_recursive *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    InitializeCriticalSection(&m->handle_);
#else
    pthread_mutexattr_t attr;
    RC_PANIC(pthread_mutexattr_init(&attr) == 0);
    RC_PANIC(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) == 0);
    int rc = pthread_mutex_init(&m->handle_, &attr);
    RC_PANIC(rc == 0);
    pthread_mutexattr_destroy(&attr);
#endif
}

void rc_mutex_recursive_deinit(rc_mutex_recursive *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    DeleteCriticalSection(&m->handle_);
#else
    int rc = pthread_mutex_destroy(&m->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_mutex_recursive_lock(rc_mutex_recursive *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    EnterCriticalSection(&m->handle_);
#else
    int rc = pthread_mutex_lock(&m->handle_);
    RC_PANIC(rc == 0);
#endif
}

bool rc_mutex_recursive_trylock(rc_mutex_recursive *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    return TryEnterCriticalSection(&m->handle_) != 0;
#else
    int rc = pthread_mutex_trylock(&m->handle_);
    if (rc == 0) return true;
    RC_PANIC(rc == EBUSY);
    return false;
#endif
}

void rc_mutex_recursive_unlock(rc_mutex_recursive *m)
{
    RC_ASSERT(m);
#if defined(_WIN32)
    LeaveCriticalSection(&m->handle_);
#else
    int rc = pthread_mutex_unlock(&m->handle_);
    RC_PANIC(rc == 0);
#endif
}
