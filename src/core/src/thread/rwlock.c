#include "richc/thread/rwlock.h"

#include "richc/macros.h"

/*
 * OS reader/writer lock: an SRWLOCK on Windows (shared vs exclusive acquisition),
 * a pthread_rwlock_t on POSIX (where a single unlock serves both modes).
 */

#if defined(__APPLE__) || defined(__linux__)
#  include <errno.h>
#endif

void rc_rwlock_init(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    InitializeSRWLock(&rw->handle_);
#else
    int rc = pthread_rwlock_init(&rw->handle_, NULL);
    RC_PANIC(rc == 0);
#endif
}

void rc_rwlock_deinit(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    (void)rw;   // an SRWLOCK requires no destruction
#else
    int rc = pthread_rwlock_destroy(&rw->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_rwlock_read_lock(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    AcquireSRWLockShared(&rw->handle_);
#else
    int rc = pthread_rwlock_rdlock(&rw->handle_);
    RC_PANIC(rc == 0);
#endif
}

bool rc_rwlock_read_trylock(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    return TryAcquireSRWLockShared(&rw->handle_) != 0;
#else
    int rc = pthread_rwlock_tryrdlock(&rw->handle_);
    if (rc == 0) return true;
    RC_PANIC(rc == EBUSY);
    return false;
#endif
}

void rc_rwlock_read_unlock(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    ReleaseSRWLockShared(&rw->handle_);
#else
    int rc = pthread_rwlock_unlock(&rw->handle_);
    RC_PANIC(rc == 0);
#endif
}

void rc_rwlock_write_lock(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    AcquireSRWLockExclusive(&rw->handle_);
#else
    int rc = pthread_rwlock_wrlock(&rw->handle_);
    RC_PANIC(rc == 0);
#endif
}

bool rc_rwlock_write_trylock(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    return TryAcquireSRWLockExclusive(&rw->handle_) != 0;
#else
    int rc = pthread_rwlock_trywrlock(&rw->handle_);
    if (rc == 0) return true;
    RC_PANIC(rc == EBUSY);
    return false;
#endif
}

void rc_rwlock_write_unlock(rc_rwlock *rw)
{
    RC_ASSERT(rw);
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&rw->handle_);
#else
    int rc = pthread_rwlock_unlock(&rw->handle_);
    RC_PANIC(rc == 0);
#endif
}
