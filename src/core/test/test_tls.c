#include "richc/thread/tls.h"

#include "richc/test.h"
#include "richc/thread/thread.h"

RC_TEST(tls, single_thread_get_set)
{
    rc_tls key = rc_tls_create();

    RC_CHECK_TRUE(rc_tls_get(key) == NULL);   // unset slot reads back NULL

    int x = 0;
    rc_tls_set(key, &x);
    RC_CHECK_TRUE(rc_tls_get(key) == &x);

    rc_tls_set(key, NULL);
    RC_CHECK_TRUE(rc_tls_get(key) == NULL);

    rc_tls_destroy(&key);
}

typedef struct tls_dyn_ctx {
    rc_tls  key;
    void   *value;
    void   *readback;
} tls_dyn_ctx;

static void tls_dyn_worker(void *p)
{
    tls_dyn_ctx *c = p;
    rc_tls_set(c->key, c->value);
    rc_thread_sleep_ns(5 * 1000 * 1000);   // all threads write before any reads
    c->readback = rc_tls_get(c->key);
}

RC_TEST(tls, per_thread_isolation)
{
    enum { N = 4 };
    rc_tls key = rc_tls_create();
    int values[N];
    tls_dyn_ctx ctx[N];
    rc_thread threads[N];

    for (int i = 0; i < N; ++i) {
        ctx[i] = (tls_dyn_ctx) {.key = key, .value = &values[i], .readback = NULL};
        RC_CHECK_TRUE(rc_thread_create(&threads[i], tls_dyn_worker, &ctx[i]));
    }
    for (int i = 0; i < N; ++i)
        rc_thread_join(&threads[i]);

    // Each thread read back its own value through the shared key.
    for (int i = 0; i < N; ++i)
        RC_CHECK_TRUE(ctx[i].readback == &values[i]);

    // This thread never set the key, so its own slot is still NULL.
    RC_CHECK_TRUE(rc_tls_get(key) == NULL);

    rc_tls_destroy(&key);
}
