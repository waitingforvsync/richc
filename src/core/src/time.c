#include "richc/time.h"

/*
 * Monotonic clock. The platform-specific query lives here as file-local code so
 * that no OS headers leak into the public header.
 */

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#  include <time.h>
#else
#  error "time.c: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

uint64_t rc_time_now_ns(void)
{
#if defined(_WIN32)
    // QueryPerformanceCounter is monotonic; scale ticks to nanoseconds without
    // overflow by splitting into whole seconds and the sub-second remainder.
    static LARGE_INTEGER freq;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);              // fixed for the session

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    uint64_t f = (uint64_t)freq.QuadPart;
    uint64_t t = (uint64_t)now.QuadPart;
    uint64_t secs = t / f;
    uint64_t rem = t % f;
    return secs * 1000000000ull + (rem * 1000000000ull) / f;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}
