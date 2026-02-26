/*
 * platform.h - virtual memory page operations.
 *
 * Provides a thin, uniform interface over OS virtual memory APIs:
 *
 *   rc_platform_page_size()            system page size in bytes (cached)
 *   rc_platform_round_up_to_page(n)    round n up to next page boundary
 *   rc_platform_reserve(size)          reserve virtual address space
 *   rc_platform_commit(addr, size)     back reserved pages with physical memory
 *   rc_platform_decommit(addr, size)   return physical pages to OS, keep VA range
 *   rc_platform_release(addr, size)    release the entire reserved region
 *
 * Typical arena lifetime:
 *   void *mem = rc_platform_reserve(4ULL << 30);   // 4 GB virtual
 *   rc_platform_commit(mem, rc_platform_page_size());  // touch first page
 *   ...grow by committing more pages as needed...
 *   rc_platform_release(mem, 4ULL << 30);
 *
 * addr and size passed to commit/decommit/release must be multiples of
 * rc_platform_page_size().  Use rc_platform_round_up_to_page() to align.
 *
 * Supported platforms: Windows, macOS, Linux.
 */

#ifndef RC_PLATFORM_H_
#define RC_PLATFORM_H_

#include <stddef.h>
#include <stdbool.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#  include <sys/mman.h>
#  include <unistd.h>
#else
#  error "platform.h: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

/* ---- page size ---- */

/*
 * Returns the system page size in bytes.  The OS is queried once per
 * translation unit and the result is cached.  The value is always a
 * power of two; on Apple Silicon it is 16384, elsewhere typically 4096.
 */
static inline size_t rc_platform_page_size(void)
{
    static size_t cached = 0;
    if (!cached) {
#if defined(_WIN32)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        cached = (size_t)si.dwPageSize;
#else
        cached = (size_t)sysconf(_SC_PAGE_SIZE);
#endif
    }
    return cached;
}

/*
 * Round size up to the next page boundary.
 * Assumes page_size is a power of two (always true on real OSes).
 */
static inline size_t rc_platform_round_up_to_page(size_t size)
{
    size_t mask = rc_platform_page_size() - 1;
    return (size + mask) & ~mask;
}

/* ---- virtual memory ---- */

/*
 * Reserve a contiguous region of virtual address space of at least
 * 'size' bytes.  No physical memory is allocated; the pages are not
 * accessible until rc_platform_commit() is called on them.
 * Returns NULL on failure.
 */
static inline void *rc_platform_reserve(size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
#else
    void *p = mmap(NULL, size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
#endif
}

/*
 * Commit a page-aligned range [addr, addr+size) within a reserved
 * region, backing it with physical memory and making it readable and
 * writable.  Returns false on failure (e.g. out of physical memory).
 */
static inline bool rc_platform_commit(void *addr, size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
    return mprotect(addr, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

/*
 * Decommit a page-aligned range: hint to the OS that the physical pages
 * may be reclaimed, then mark them inaccessible.  The virtual address
 * range remains reserved and can be recommitted later with
 * rc_platform_commit().
 */
static inline void rc_platform_decommit(void *addr, size_t size)
{
#if defined(_WIN32)
    VirtualFree(addr, size, MEM_DECOMMIT);
#else
    /*
     * madvise hints the kernel that the pages can be reclaimed; on the
     * next access after recommit they will be zero-filled.
     * mprotect then makes them inaccessible so any stale pointer into
     * the decommitted range faults immediately rather than silently
     * reading stale or zero data.
     */
    madvise(addr, size, MADV_DONTNEED);
    mprotect(addr, size, PROT_NONE);
#endif
}

/*
 * Release the entire reserved region back to the OS.  addr must be the
 * base pointer returned by rc_platform_reserve(); size must be the same
 * value passed to that call.  All committed pages are implicitly
 * decommitted.
 */
static inline void rc_platform_release(void *addr, size_t size)
{
#if defined(_WIN32)
    (void)size;                    /* MEM_RELEASE requires dwSize == 0 */
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, size);
#endif
}

#endif /* RC_PLATFORM_H_ */
