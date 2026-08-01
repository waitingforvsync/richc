/*
 * thread/thread.h - OS threads and thread utilities.
 *
 * Threads
 * -------
 * rc_thread_create starts a thread running fn(arg). The rc_thread object is
 * caller-owned and initialised in place: it stores fn and arg and is itself
 * handed to the OS thread, so no allocation is needed to launch one. Because the
 * running thread reads through the object, it MUST stay live at a stable address
 * until joined - store it somewhere durable (a pool slot, a static, a long-lived
 * struct), not a temporary that goes out of scope. Join is the normal path;
 * rc_thread_detach is supported but then nothing signals when the object may be
 * reused, so it must remain valid for as long as the thread runs.
 *
 * Utilities
 * ---------
 * current id, cooperative yield, sleep, and the logical-core count (for sizing a
 * pool). rc_once runs an initialiser exactly once across all threads. A
 * zero-initialised rc_once is ready to use (rc_once o = {0}).
 *
 * RC_THREAD_LOCAL declares a variable with per-thread storage (the compiler's
 * thread-local keyword); for dynamically-created keys see thread/tls.h.
 *
 * Initialised-in-place objects embed the real OS handle, so this header includes
 * the platform header (see thread/mutex.h for the rationale).
 */

#ifndef RC_THREAD_THREAD_H_
#define RC_THREAD_THREAD_H_

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
#elif defined(__APPLE__) || defined(__linux__)
#  include <pthread.h>
#else
#  error "thread/thread.h: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

/* ---- thread-local storage keyword ---- */

#if defined(_MSC_VER)
#  define RC_THREAD_LOCAL __declspec(thread)
#else
#  define RC_THREAD_LOCAL _Thread_local
#endif

/* ---- threads ---- */

typedef void (*rc_thread_func)(void *arg);

typedef struct rc_thread {
    rc_thread_func fn_;
    void          *arg_;
#if defined(_WIN32)
    HANDLE         handle_;
#else
    pthread_t      handle_;
#endif
} rc_thread;

/* Start a thread running fn(arg). Returns false if the OS refused to create it.
 * t must remain live at a stable address until joined (or, if detached, for the
 * lifetime of the thread). */
bool rc_thread_create(rc_thread *t, rc_thread_func fn, void *arg);

/* Wait for the thread to finish, then release its resources. */
void rc_thread_join(rc_thread *t);

/* Release the thread to run independently; it can no longer be joined. */
void rc_thread_detach(rc_thread *t);

/* ---- utilities ---- */

uint64_t rc_thread_current_id(void);         /* opaque id, unique among live threads */
void     rc_thread_yield(void);              /* hint the scheduler to run another thread */
void     rc_thread_sleep_ns(uint64_t ns);    /* sleep for at least ns nanoseconds */
uint32_t rc_thread_hardware_concurrency(void); /* number of logical cores (>= 1) */

/* ---- run-once ---- */

typedef struct rc_once {
#if defined(_WIN32)
    INIT_ONCE      handle_;
#else
    pthread_once_t handle_;
#endif
} rc_once;

/* Run fn() exactly once for a given once object, even if called concurrently
 * from many threads; later calls return after the first has completed. */
void rc_once_run(rc_once *once, void (*fn)(void));

#endif /* RC_THREAD_THREAD_H_ */
