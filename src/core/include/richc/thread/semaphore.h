/*
 * thread/semaphore.h - counting semaphore.
 *
 * Holds a non-negative count. wait decrements it, blocking while it is zero;
 * post increments it (by a given amount), releasing that many waiters. Useful
 * for bounding a resource pool or as a wakeup primitive for a worker queue.
 *
 * Initialised in place; see thread/mutex.h for the rationale. The public struct
 * embeds the real platform object, so this header includes the OS header. The
 * backing object differs by platform: a Win32 semaphore HANDLE, a POSIX sem_t on
 * Linux, and a dispatch_semaphore_t on Apple (POSIX unnamed semaphores are not
 * supported there).
 */

#ifndef RC_THREAD_SEMAPHORE_H_
#define RC_THREAD_SEMAPHORE_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__)
#  include <dispatch/dispatch.h>
#elif defined(__linux__)
#  include <semaphore.h>
#else
#  error "thread/semaphore.h: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

typedef struct rc_semaphore {
#if defined(_WIN32)
    HANDLE handle_;
#elif defined(__APPLE__)
    dispatch_semaphore_t handle_;
#else
    sem_t handle_;
#endif
} rc_semaphore;

void rc_semaphore_init(rc_semaphore *s, uint32_t initial_count);
void rc_semaphore_deinit(rc_semaphore *s);

void rc_semaphore_wait(rc_semaphore *s);                        /* block until count > 0, then decrement */
bool rc_semaphore_try_wait(rc_semaphore *s);                    /* decrement if count > 0; true if it did */
bool rc_semaphore_wait_for(rc_semaphore *s, uint64_t timeout_ns); /* false if the timeout elapsed first */
void rc_semaphore_post(rc_semaphore *s, uint32_t count);        /* add count to the semaphore */

#endif /* RC_THREAD_SEMAPHORE_H_ */
