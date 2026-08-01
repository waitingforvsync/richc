/*
 * thread/tls.h - dynamic thread-local storage keys.
 *
 * An rc_tls is a key naming one void * slot that has an independent value in
 * each thread. Use it when the set of thread-local variables is not known at
 * compile time (a keyword-declared RC_THREAD_LOCAL is simpler when it is - see
 * thread/thread.h). A freshly observed slot reads back NULL in every thread
 * until that thread sets it; keys carry no destructor.
 *
 * The key is a small, copyable handle, so rc_tls_create returns it by value;
 * rc_tls_get / rc_tls_set take it by value. The public struct embeds the real OS
 * key, so this header includes the platform header.
 */

#ifndef RC_THREAD_TLS_H_
#define RC_THREAD_TLS_H_

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#  include <pthread.h>
#else
#  error "thread/tls.h: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

typedef struct rc_tls {
#if defined(_WIN32)
    DWORD key_;
#else
    pthread_key_t key_;
#endif
} rc_tls;

rc_tls rc_tls_create(void);          /* allocate a key; its slot starts NULL in every thread */
void   rc_tls_destroy(rc_tls *key);  /* release the key; using it afterwards is undefined */
void  *rc_tls_get(rc_tls key);       /* this thread's value for the key */
void   rc_tls_set(rc_tls key, void *value);

#endif /* RC_THREAD_TLS_H_ */
