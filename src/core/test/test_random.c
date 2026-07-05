#include "richc/test.h"

#include "richc/random.h"

RC_TEST(random, deterministic)
{
    // The same seed replays the same stream, draw for draw.
    rc_random a = rc_random_make(0x1234abcdu);
    rc_random b = rc_random_make(0x1234abcdu);
    for (int i = 0; i < 16; i++) {
        RC_CHECK(rc_random_next(&a), ==, rc_random_next(&b));
    }
}

RC_TEST(random, diverges_and_moves)
{
    // Consecutive draws are not stuck on one value...
    rc_random p = rc_random_make(1);
    uint32_t v0 = rc_random_next(&p);
    uint32_t v1 = rc_random_next(&p);
    uint32_t v2 = rc_random_next(&p);
    RC_CHECK_TRUE(v0 != v1 || v1 != v2);

    // ...and a different seed gives a different stream.
    rc_random q = rc_random_make(2);
    RC_CHECK_TRUE(rc_random_next(&q) != v0);
}
