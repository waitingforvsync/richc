#include "richc/thread/tls.h"

#include "richc/macros.h"

/*
 * Dynamic thread-local storage keys: Win32 TLS indices (TlsAlloc et al.) or
 * POSIX pthread keys. No per-key destructor is registered.
 */

rc_tls rc_tls_create(void)
{
#if defined(_WIN32)
    DWORD key = TlsAlloc();
    RC_PANIC(key != TLS_OUT_OF_INDEXES);
    return (rc_tls) {.key_ = key};
#else
    pthread_key_t key;
    int rc = pthread_key_create(&key, NULL);
    RC_PANIC(rc == 0);
    return (rc_tls) {.key_ = key};
#endif
}

void rc_tls_destroy(rc_tls *key)
{
    RC_ASSERT(key);
#if defined(_WIN32)
    BOOL ok = TlsFree(key->key_);
    RC_PANIC(ok);
#else
    int rc = pthread_key_delete(key->key_);
    RC_PANIC(rc == 0);
#endif
}

void *rc_tls_get(rc_tls key)
{
#if defined(_WIN32)
    return TlsGetValue(key.key_);
#else
    return pthread_getspecific(key.key_);
#endif
}

void rc_tls_set(rc_tls key, void *value)
{
#if defined(_WIN32)
    BOOL ok = TlsSetValue(key.key_, value);
    RC_PANIC(ok);
#else
    int rc = pthread_setspecific(key.key_, value);
    RC_PANIC(rc == 0);
#endif
}
