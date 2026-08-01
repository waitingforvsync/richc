#include "richc/thread/rwlock.h"

#include "richc/test.h"

// Single-threaded lifecycle coverage; concurrent reader-parallelism and
// writer-exclusion are checked with real threads in test_thread.c.

RC_TEST(rwlock, lifecycle)
{
    rc_rwlock rw;
    rc_rwlock_init(&rw);

    rc_rwlock_read_lock(&rw);
    rc_rwlock_read_unlock(&rw);

    RC_CHECK_TRUE(rc_rwlock_read_trylock(&rw));   // free -> shared acquired
    rc_rwlock_read_unlock(&rw);

    rc_rwlock_write_lock(&rw);
    rc_rwlock_write_unlock(&rw);

    RC_CHECK_TRUE(rc_rwlock_write_trylock(&rw));  // free -> exclusive acquired
    rc_rwlock_write_unlock(&rw);

    rc_rwlock_deinit(&rw);
}
