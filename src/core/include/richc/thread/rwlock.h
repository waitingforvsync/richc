/*
 * thread/rwlock.h - reader/writer lock.
 *
 * Allows any number of concurrent readers or a single exclusive writer. Not
 * recursive, and a read lock cannot be upgraded to a write lock (release then
 * re-acquire). Read locks and write locks are released by their own calls
 * (read_unlock / write_unlock) - matching the Windows SRWLOCK model, where the
 * two share no release primitive.
 *
 * Initialised in place; see thread/mutex.h for the rationale (OS object, not
 * copyable). This header includes the OS header to embed the real lock type.
 */

#ifndef RC_THREAD_RWLOCK_H_
#define RC_THREAD_RWLOCK_H_

#include <stdbool.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#  include <pthread.h>
#else
#  error "thread/rwlock.h: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

typedef struct rc_rwlock {
#if defined(_WIN32)
    SRWLOCK handle_;
#else
    pthread_rwlock_t handle_;
#endif
} rc_rwlock;

void rc_rwlock_init(rc_rwlock *rw);
void rc_rwlock_deinit(rc_rwlock *rw);

void rc_rwlock_read_lock(rc_rwlock *rw);
bool rc_rwlock_read_trylock(rc_rwlock *rw);   /* true if a shared lock was acquired */
void rc_rwlock_read_unlock(rc_rwlock *rw);

void rc_rwlock_write_lock(rc_rwlock *rw);
bool rc_rwlock_write_trylock(rc_rwlock *rw);  /* true if the exclusive lock was acquired */
void rc_rwlock_write_unlock(rc_rwlock *rw);

#endif /* RC_THREAD_RWLOCK_H_ */
