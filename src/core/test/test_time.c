#include "richc/time.h"

#include "richc/test.h"

RC_TEST(time, monotonic_non_decreasing)
{
    uint64_t prev = rc_time_now_ns();
    for (int i = 0; i < 100000; ++i) {
        uint64_t now = rc_time_now_ns();
        RC_CHECK_TRUE(now >= prev);
        prev = now;
    }
}

RC_TEST(time, advances)
{
    // A working clock must eventually report a later time. Spin until it does,
    // with a generous cap so a stuck clock fails rather than hangs forever.
    uint64_t start = rc_time_now_ns();
    uint64_t now = start;
    for (int i = 0; i < 100000000 && now == start; ++i)
        now = rc_time_now_ns();
    RC_CHECK_TRUE(now > start);
}
