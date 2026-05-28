#include "richc/arena.h"

#include <string.h>

/*
 * OS virtual memory.
 *
 * The platform-specific page operations live here as file-local statics
 * rather than in a public header, so that no OS headers leak into the
 * library's API.  All addresses and sizes passed to commit/decommit/release
 * must be page-aligned; use rc_platform_round_up_to_page to align.
 */

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#  include <sys/mman.h>
#  include <unistd.h>
#else
#  error "arena.c: unsupported platform (expected _WIN32, __APPLE__, or __linux__)"
#endif

/* System page size in bytes, queried once and cached.  Always a power of two. */
static size_t rc_platform_page_size(void)
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

/* Round size up to the next page boundary. */
static size_t rc_platform_round_up_to_page(size_t size)
{
    size_t mask = rc_platform_page_size() - 1;
    return (size + mask) & ~mask;
}

/* Reserve a contiguous region of virtual address space.  No physical memory
 * is committed; the pages are inaccessible until committed.  NULL on failure. */
static void *rc_platform_reserve(size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
#else
    void *p = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
#endif
}

/* Commit a page-aligned range within a reserved region, making it readable
 * and writable.  Returns false on failure (e.g. out of physical memory). */
static bool rc_platform_commit(void *addr, size_t size)
{
#if defined(_WIN32)
    return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
    return mprotect(addr, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

/* Return physical pages for a page-aligned range to the OS and mark them
 * inaccessible.  The virtual range stays reserved and can be recommitted. */
static void rc_platform_decommit(void *addr, size_t size)
{
#if defined(_WIN32)
    VirtualFree(addr, size, MEM_DECOMMIT);
#else
    // madvise lets the kernel reclaim the pages (re-zeroed on next access);
    // mprotect then faults any stale pointer into the range instead of
    // silently reading old data.
    madvise(addr, size, MADV_DONTNEED);
    mprotect(addr, size, PROT_NONE);
#endif
}

/* Release the entire reserved region.  addr must be the base from reserve;
 * size must match the value passed to reserve. */
static void rc_platform_release(void *addr, size_t size)
{
#if defined(_WIN32)
    (void)size;                    // MEM_RELEASE requires dwSize == 0
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, size);
#endif
}

/* ---- private helpers ---- */

static uint32_t rc_arena_align_up(uint32_t n)
{
    uint32_t align = RC_MAX_ALIGN;
    return (n + align - 1) & ~(align - 1);
}

/*
 * Commit enough pages to cover [committed, new_top).
 * Returns false if the OS refuses to commit (OOM or past the reservation).
 * No explicit bounds check against reserved: rc_platform_commit fails
 * naturally if new_top exceeds the reserved region.
 */
static bool rc_arena_grow_committed(rc_arena *a, uint32_t new_top)
{
    size_t to_commit = rc_platform_round_up_to_page((size_t)(new_top - a->committed));
    if (!rc_platform_commit(a->base + a->committed, to_commit))
        return false;
    a->committed += (uint32_t)to_commit;
    return true;
}

/* Shared implementation for rc_arena_realloc / rc_arena_realloc_zero.
 * Requires ptr != NULL. */
static void *rc_arena_realloc_impl(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(new_size != 0);
    RC_ASSERT((char *)ptr >= a->base && (char *)ptr <= a->base + a->top);

    uint32_t aligned_old = rc_arena_align_up(old_size);
    uint32_t aligned_new = rc_arena_align_up(new_size);
    uint32_t ptr_offset  = (uint32_t)(size_t)((char *)ptr - a->base);
    bool     is_last     = (ptr_offset + aligned_old == a->top);

    if (is_last) {
        if (aligned_new <= aligned_old) {
            // Shrink: move top down.
            a->top = ptr_offset + aligned_new;
            return ptr;
        }

        // Grow in place.
        uint32_t new_top = ptr_offset + aligned_new;
        RC_PANIC(new_top <= a->reserved);           // the grown block fits the reservation
        if (new_top > a->committed) {
            bool committed = rc_arena_grow_committed(a, new_top);
            RC_PANIC(committed);                    // the OS committed the pages
        }
        a->top = new_top;
        return ptr;
    }

    // Not the last allocation.
    if (aligned_new <= aligned_old)
        return ptr;             // shrink: no-op

    void *new_ptr = rc_arena_alloc(a, new_size);    // panics on OOM, never NULL
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

/* ---- public API ---- */

rc_arena rc_arena_make(uint32_t reserve_size)
{
    rc_arena a = {0};

    reserve_size = (uint32_t)rc_platform_round_up_to_page(reserve_size);
    if (!reserve_size) return a;

    char *mem = rc_platform_reserve(reserve_size);
    if (!mem) return a;

    uint32_t initial = (uint32_t)rc_platform_page_size();
    if (!rc_platform_commit(mem, initial)) {
        rc_platform_release(mem, reserve_size);
        return a;
    }

    a.base      = mem;
    a.top       = 0;
    a.committed = initial;
    a.reserved  = reserve_size;
    return a;
}

void rc_arena_destroy(rc_arena *a)
{
    RC_ASSERT(a);
    if (a->base)
        rc_platform_release(a->base, a->reserved);
    *a = (rc_arena) {0};
}

void *rc_arena_alloc(rc_arena *a, uint32_t size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->base);    // the arena must have been created
    RC_ASSERT(size != 0);

    uint32_t aligned = rc_arena_align_up(size);
    RC_PANIC(aligned >= size);                  // alignment rounding did not overflow
    RC_PANIC(aligned <= a->reserved - a->top);  // the request fits the reservation

    uint32_t new_top = a->top + aligned;
    if (new_top > a->committed) {
        bool committed = rc_arena_grow_committed(a, new_top);
        RC_PANIC(committed);                    // the OS committed the pages
    }

    void *result = a->base + a->top;
    a->top = new_top;
    return result;
}

void *rc_arena_alloc_zero(rc_arena *a, uint32_t size)
{
    void *p = rc_arena_alloc(a, size);
    memset(p, 0, size);
    return p;
}

bool rc_arena_free(rc_arena *a, void *ptr, uint32_t size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->base);
    RC_ASSERT(ptr);
    RC_ASSERT((char *)ptr >= a->base && (char *)ptr <= a->base + a->top);

    uint32_t ptr_offset = (uint32_t)(size_t)((char *)ptr - a->base);
    if (ptr_offset + rc_arena_align_up(size) == a->top) {
        a->top = ptr_offset;
        return true;
    }
    return false;
}

void *rc_arena_realloc(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->base);
    if (!ptr) return rc_arena_alloc(a, new_size);
    return rc_arena_realloc_impl(a, ptr, old_size, new_size);
}

void *rc_arena_realloc_zero(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    RC_ASSERT(a);
    RC_ASSERT(a->base);
    if (!ptr) return rc_arena_alloc_zero(a, new_size);
    void *result = rc_arena_realloc_impl(a, ptr, old_size, new_size);
    if (new_size > old_size)
        memset((char *)result + old_size, 0, new_size - old_size);
    return result;
}

void rc_arena_reset(rc_arena *a)
{
    RC_ASSERT(a);
    if (!a->base) return;
    uint32_t keep = (uint32_t)rc_platform_page_size();
    if (a->committed > keep) {
        rc_platform_decommit(a->base + keep, a->committed - keep);
        a->committed = keep;
    }
    a->top = 0;
}
