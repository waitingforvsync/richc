#include "richc/thread/cond.h"

#include "richc/test.h"
#include "richc/time.h"

// Single-threaded coverage: the timeout path (no signaller), plus signal and
// broadcast being safe no-ops with no waiters. The full wait/signal handshake
// between threads is exercised in test_thread.c.

RC_TEST(cond, timeout_elapses)
{
    rc_cond c;
    rc_cond_init(&c);
    rc_mutex m;
    rc_mutex_init(&m);

    const uint64_t timeout = 20 * 1000 * 1000;   // 20 ms

    rc_mutex_lock(&m);
    uint64_t start = rc_time_now_ns();
    bool timed_out = false;
    for (;;) {
        uint64_t elapsed = rc_time_now_ns() - start;
        if (elapsed >= timeout) { timed_out = true; break; }   // safety net
        // Nothing ever signals c, so this must eventually report a timeout;
        // a spurious wake just loops and waits out the remaining time.
        if (!rc_cond_wait_for(&c, &m, timeout - elapsed)) { timed_out = true; break; }
    }
    uint64_t total = rc_time_now_ns() - start;
    rc_mutex_unlock(&m);

    RC_CHECK_TRUE(timed_out);
    RC_CHECK_TRUE(total >= timeout - 2 * 1000 * 1000);   // blocked ~the whole timeout

    rc_cond_signal(&c);      // no waiters -> no-op, must not crash
    rc_cond_broadcast(&c);

    rc_mutex_deinit(&m);
    rc_cond_deinit(&c);
}
