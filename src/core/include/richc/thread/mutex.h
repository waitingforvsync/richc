/*
 * thread/mutex.h - mutual-exclusion locks.
 *
 * rc_mutex is a non-recursive lock (relocking by the owning thread is undefined);
 * it is the lock that pairs with rc_cond. rc_mutex_recursive may be locked
 * repeatedly by the thread that already owns it, provided each lock is matched by
 * an unlock; it does not pair with rc_cond.
 *
 * Both are initialised in place (rc_*_init) rather than returned by value: they
 * wrap OS objects that must be constructed in their final storage and are not
 * copyable or movable. Cleanup is rc_*_deinit; using a mutex after deinit, or
 * destroying one that is still locked, is undefined.
 *
 * The public struct embeds the real platform lock as a private member (its size
 * and layout differ per platform, which is fine), so this header includes the OS
 * header - a deliberate, local exception to the "no OS headers in the public API"
 * rule that the rest of the library follows.
 */

#ifndef RC_THREAD_MUTEX_H_
#define RC_THREAD_MUTEX_H_

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
#  error "thread/mutex.h: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

/* ---- non-recursive mutex (backs rc_cond) ---- */

typedef struct rc_mutex {
#if defined(_WIN32)
    SRWLOCK handle_;
#else
    pthread_mutex_t handle_;
#endif
} rc_mutex;

void rc_mutex_init(rc_mutex *m);
void rc_mutex_deinit(rc_mutex *m);
void rc_mutex_lock(rc_mutex *m);
bool rc_mutex_trylock(rc_mutex *m);   /* true if the lock was acquired */
void rc_mutex_unlock(rc_mutex *m);

/* ---- recursive mutex ---- */

typedef struct rc_mutex_recursive {
#if defined(_WIN32)
    CRITICAL_SECTION handle_;
#else
    pthread_mutex_t handle_;
#endif
} rc_mutex_recursive;

void rc_mutex_recursive_init(rc_mutex_recursive *m);
void rc_mutex_recursive_deinit(rc_mutex_recursive *m);
void rc_mutex_recursive_lock(rc_mutex_recursive *m);
bool rc_mutex_recursive_trylock(rc_mutex_recursive *m);
void rc_mutex_recursive_unlock(rc_mutex_recursive *m);

#endif /* RC_THREAD_MUTEX_H_ */
