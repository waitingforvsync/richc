#include "richc/thread/mutex.h"

#include "richc/test.h"

// Single-threaded coverage only. A non-recursive mutex must never be relocked by
// its owner (undefined), so mutual exclusion under contention is tested with real
// threads in test_thread.c; here we cover the lifecycle and the free-lock paths.

RC_TEST(mutex, lifecycle)
{
    rc_mutex m;
    rc_mutex_init(&m);

    rc_mutex_lock(&m);
    rc_mutex_unlock(&m);

    RC_CHECK_TRUE(rc_mutex_trylock(&m));   // acquire the free lock
    rc_mutex_unlock(&m);

    rc_mutex_deinit(&m);
}

RC_TEST(mutex, recursive_reentry)
{
    rc_mutex_recursive m;
    rc_mutex_recursive_init(&m);

    rc_mutex_recursive_lock(&m);
    rc_mutex_recursive_lock(&m);                    // re-lock by the same thread
    RC_CHECK_TRUE(rc_mutex_recursive_trylock(&m));  // and again via trylock

    rc_mutex_recursive_unlock(&m);                  // must unlock once per lock
    rc_mutex_recursive_unlock(&m);
    rc_mutex_recursive_unlock(&m);

    rc_mutex_recursive_deinit(&m);
}
