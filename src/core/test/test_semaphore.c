#include "richc/thread/semaphore.h"

#include "richc/test.h"
#include "richc/time.h"

// Single-threaded coverage of counts and the timeout path. Cross-thread wakeup
// (post on one thread releasing a waiter on another) is tested in test_thread.c.

RC_TEST(semaphore, counts)
{
    rc_semaphore s;
    rc_semaphore_init(&s, 2);

    RC_CHECK_TRUE(rc_semaphore_try_wait(&s));    // 2 -> 1
    RC_CHECK_TRUE(rc_semaphore_try_wait(&s));    // 1 -> 0
    RC_CHECK_FALSE(rc_semaphore_try_wait(&s));   // 0 -> fails

    rc_semaphore_post(&s, 3);                    // 0 -> 3
    RC_CHECK_TRUE(rc_semaphore_try_wait(&s));    // 3 -> 2
    rc_semaphore_wait(&s);                       // 2 -> 1 (count > 0, no block)
    RC_CHECK_TRUE(rc_semaphore_try_wait(&s));    // 1 -> 0
    RC_CHECK_FALSE(rc_semaphore_try_wait(&s));   // 0 -> fails

    rc_semaphore_deinit(&s);
}

RC_TEST(semaphore, timeout_elapses)
{
    rc_semaphore s;
    rc_semaphore_init(&s, 0);                     // empty: wait_for must time out

    const uint64_t timeout = 20 * 1000 * 1000;   // 20 ms
    uint64_t start = rc_time_now_ns();
    bool got = rc_semaphore_wait_for(&s, timeout);
    uint64_t elapsed = rc_time_now_ns() - start;

    RC_CHECK_FALSE(got);
    RC_CHECK_TRUE(elapsed >= timeout - 2 * 1000 * 1000);

    rc_semaphore_deinit(&s);
}
