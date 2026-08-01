#include "richc/thread/spinlock.h"

#include "richc/test.h"

RC_TEST(spinlock, basic)
{
    rc_spinlock s = {0};                     // zero-init is a valid unlocked lock

    RC_CHECK_TRUE(rc_spinlock_trylock(&s));  // acquire the free lock
    RC_CHECK_FALSE(rc_spinlock_trylock(&s)); // already held -> fails
    rc_spinlock_unlock(&s);

    RC_CHECK_TRUE(rc_spinlock_trylock(&s));  // reacquire after unlock
    rc_spinlock_unlock(&s);

    rc_spinlock_lock(&s);                    // blocking lock on a free lock returns
    RC_CHECK_FALSE(rc_spinlock_trylock(&s)); // held by us
    rc_spinlock_unlock(&s);
}
