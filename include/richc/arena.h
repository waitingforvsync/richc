/*
 * arena.h - virtual-memory-backed stack allocator.
 *
 * An rc_arena reserves a large region of virtual address space up front and
 * commits physical pages on demand as allocations require them.  Because
 * the reserved range never moves, pointers into the arena remain stable
 * for its entire lifetime.
 *
 * All allocations are aligned to alignof(max_align_t) and are therefore
 * suitable for any standard C type without further padding.
 *
 * Zeroing guarantee
 * -----------------
 * rc_arena_alloc and rc_arena_realloc do NOT guarantee zeroed memory.
 * Freshly committed pages are OS-zeroed, but space reclaimed via
 * rc_arena_free or rc_arena_free_to retains its previous contents.  Use
 * rc_arena_alloc_zero and rc_arena_realloc_zero when a clean allocation
 * is required.
 *
 * Typical usage
 * -------------
 *   rc_arena a = rc_arena_make_default();
 *
 *   uint32_t mark = a.top;
 *   int  *buf = rc_arena_alloc_type(&a, int, 256);
 *   char *str = rc_arena_alloc(&a, 128);
 *
 *   rc_arena_free_to(&a, mark);  // cheaply discard buf and str
 *   rc_arena_reset(&a);          // return physical pages to OS
 *   rc_arena_destroy(&a);        // release virtual address space
 *
 * Scratch pattern
 * ---------------
 * Passing an rc_arena by value gives the callee a snapshot of the current
 * bump pointer.  Allocations inside the callee advance only the local
 * copy; the caller's arena is unchanged when the function returns.
 *
 *   void build_temp(rc_arena scratch, rc_arena *out) {
 *       int *tmp = rc_arena_alloc_type(&scratch, int, 1024); // local only
 *       ...
 *       int *result = rc_arena_alloc_type(out, int, n);      // survives
 *   }
 *
 * Free-to semantics
 * -----------------
 * rc_arena_free_to() resets the bump pointer to any earlier offset,
 * effectively freeing everything allocated after that point in O(1).
 *
 *   uint32_t mark = arena->top;
 *   ... allocate temporary things ...
 *   rc_arena_free_to(arena, mark);
 */

#ifndef RC_ARENA_H_
#define RC_ARENA_H_

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "richc/debug.h"
#include "richc/platform.h"

/*
 * RC_MAX_ALIGN: alignment in bytes suitable for any standard C object.
 * Equals alignof(max_align_t) on platforms that provide it.  MSVC does not
 * expose max_align_t in C mode, so we hardcode 16, which matches the actual
 * value on x86-64 Windows (largest standard-type alignment: long double / __m128).
 */
#if defined(_MSC_VER)
#  define RC_MAX_ALIGN 16u
#else
#  define RC_MAX_ALIGN ((uint32_t)alignof(max_align_t))
#endif

/* ---- struct ---- */

/*
 * The arena stores byte offsets from base rather than absolute pointers.
 * All offsets are uint32_t, so arenas are limited to 4 GB — more than
 * enough for any cache-friendly allocation pattern.
 *
 * reserved is kept for rc_platform_release on POSIX (munmap requires the
 * original size).  On Windows, VirtualFree(MEM_RELEASE) ignores the size,
 * but we pass it uniformly through the platform abstraction.
 */
typedef struct {
    char     *base;      /* base of reserved VA region; first allocation address  */
    uint32_t  top;       /* offset of next free byte; always RC_MAX_ALIGN-aligned */
    uint32_t  committed; /* offset of one past the last committed (accessible) byte */
    uint32_t  reserved;  /* total reserved bytes; needed by rc_platform_release on POSIX */
} rc_arena;

/* Suggested reserve size for a general-purpose arena.
 * Virtual address space is cheap; physical pages are only committed on use. */
#define RC_ARENA_DEFAULT_RESERVE ((uint32_t)256 * 1024 * 1024)   /* 256 MB */

/* ---- function declarations ---- */

/* Create an arena by reserving reserve_size bytes of virtual address space.
 * Commits the first page immediately.  Returns {0} on failure; check base. */
rc_arena  rc_arena_make(uint32_t reserve_size);

/* Release all virtual memory and zero the struct. */
void      rc_arena_destroy(rc_arena *a);

/* Bump-allocate size bytes.  Returns NULL on failure or if size is 0.
 * The returned memory is NOT guaranteed to be zeroed. */
void     *rc_arena_alloc(rc_arena *a, uint32_t size);

/* Like rc_arena_alloc, but guarantees the returned memory is zeroed. */
void     *rc_arena_alloc_zero(rc_arena *a, uint32_t size);

/* If ptr is the last allocation, reclaim it by moving top back and return true.
 * Otherwise a no-op returning false (the space cannot be recovered mid-arena). */
bool      rc_arena_free(rc_arena *a, void *ptr, uint32_t size);

/* Resize the allocation at ptr.  New bytes when growing are NOT zeroed.
 * See rc_arena_realloc_zero for a zeroing variant. */
void     *rc_arena_realloc(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size);

/* Like rc_arena_realloc, but zeroes any newly added bytes when growing. */
void     *rc_arena_realloc_zero(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size);

/* Reset to base and decommit all pages except the first, returning
 * physical memory to the OS.  The arena remains valid afterwards. */
void      rc_arena_reset(rc_arena *a);

/* ---- trivial inline functions ---- */

/* Convenience wrapper: rc_arena_make(RC_ARENA_DEFAULT_RESERVE). */
static inline rc_arena rc_arena_make_default(void)
{
    return rc_arena_make(RC_ARENA_DEFAULT_RESERVE);
}

/* Reset the bump pointer to offset, which must be <= top.
 * Committed pages are retained; no OS call is made. */
static inline void rc_arena_free_to(rc_arena *a, uint32_t offset)
{
    RC_ASSERT(offset <= a->top);
    a->top = offset;
}

/* ---- convenience macros ---- */

/* Allocate n elements of type T (no zeroing). */
#define rc_arena_alloc_type(arena, T, n) \
    ((T *)rc_arena_alloc((arena), (uint32_t)sizeof(T) * (uint32_t)(n)))

/* Allocate n elements of type T, guaranteed zeroed. */
#define rc_arena_alloc_zero_type(arena, T, n) \
    ((T *)rc_arena_alloc_zero((arena), (uint32_t)sizeof(T) * (uint32_t)(n)))

#endif /* RC_ARENA_H_ */
