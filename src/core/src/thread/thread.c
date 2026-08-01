#include "richc/thread/thread.h"

#include "richc/macros.h"

/*
 * OS threads. rc_thread_create stores fn/arg in the caller-owned object and
 * passes the object itself as the OS thread argument; a small trampoline reads
 * them back and adapts the platform entry-point signature, so launching a thread
 * needs no allocation.
 */

#if defined(__APPLE__) || defined(__linux__)
#  include <errno.h>
#  include <sched.h>
#  include <time.h>
#  include <unistd.h>
#endif

/* ---- threads ---- */

#if defined(_WIN32)

static DWORD WINAPI rc_thread_trampoline_(LPVOID param)
{
    rc_thread *t = (rc_thread *)param;
    t->fn_(t->arg_);
    return 0;
}

bool rc_thread_create(rc_thread *t, rc_thread_func fn, void *arg)
{
    RC_ASSERT(t);
    RC_ASSERT(fn);
    t->fn_ = fn;
    t->arg_ = arg;
    t->handle_ = CreateThread(NULL, 0, rc_thread_trampoline_, t, 0, NULL);
    return t->handle_ != NULL;
}

void rc_thread_join(rc_thread *t)
{
    RC_ASSERT(t);
    DWORD r = WaitForSingleObject(t->handle_, INFINITE);
    RC_PANIC(r == WAIT_OBJECT_0);
    CloseHandle(t->handle_);
    t->handle_ = NULL;
}

void rc_thread_detach(rc_thread *t)
{
    RC_ASSERT(t);
    CloseHandle(t->handle_);
    t->handle_ = NULL;
}

#else /* POSIX */

static void *rc_thread_trampoline_(void *param)
{
    rc_thread *t = (rc_thread *)param;
    t->fn_(t->arg_);
    return NULL;
}

bool rc_thread_create(rc_thread *t, rc_thread_func fn, void *arg)
{
    RC_ASSERT(t);
    RC_ASSERT(fn);
    t->fn_ = fn;
    t->arg_ = arg;
    return pthread_create(&t->handle_, NULL, rc_thread_trampoline_, t) == 0;
}

void rc_thread_join(rc_thread *t)
{
    RC_ASSERT(t);
    int rc = pthread_join(t->handle_, NULL);
    RC_PANIC(rc == 0);
}

void rc_thread_detach(rc_thread *t)
{
    RC_ASSERT(t);
    int rc = pthread_detach(t->handle_);
    RC_PANIC(rc == 0);
}

#endif

/* ---- utilities ---- */

uint64_t rc_thread_current_id(void)
{
#if defined(_WIN32)
    return (uint64_t)GetCurrentThreadId();
#elif defined(__APPLE__)
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    return tid;
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

void rc_thread_yield(void)
{
#if defined(_WIN32)
    SwitchToThread();
#else
    sched_yield();
#endif
}

void rc_thread_sleep_ns(uint64_t ns)
{
#if defined(_WIN32)
    // Sleep resolves in milliseconds; round up so we never sleep short.
    uint64_t ms = (ns + 999999ull) / 1000000ull;
    Sleep((DWORD)(ms > 0xFFFFFFFEull ? 0xFFFFFFFEull : ms));
#else
    struct timespec req;
    req.tv_sec = (time_t)(ns / 1000000000ull);
    req.tv_nsec = (long)(ns % 1000000000ull);
    // nanosleep writes the unslept remainder back into req on interruption.
    while (nanosleep(&req, &req) != 0)
        RC_PANIC(errno == EINTR);
#endif
}

uint32_t rc_thread_hardware_concurrency(void)
{
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (uint32_t)si.dwNumberOfProcessors : 1u;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (uint32_t)n : 1u;
#endif
}

/* ---- run-once ---- */

#if defined(_WIN32)

static BOOL CALLBACK rc_once_trampoline_(PINIT_ONCE once, PVOID param, PVOID *context)
{
    (void)once;
    (void)context;
    ((void (*)(void))param)();
    return TRUE;
}

void rc_once_run(rc_once *once, void (*fn)(void))
{
    RC_ASSERT(once);
    RC_ASSERT(fn);
    BOOL ok = InitOnceExecuteOnce(&once->handle_, rc_once_trampoline_, (PVOID)fn, NULL);
    RC_PANIC(ok);
}

#else

void rc_once_run(rc_once *once, void (*fn)(void))
{
    RC_ASSERT(once);
    RC_ASSERT(fn);
    int rc = pthread_once(&once->handle_, fn);
    RC_PANIC(rc == 0);
}

#endif
