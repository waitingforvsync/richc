#include <string.h>
#include "richc/arena.h"
#if RICHC_PLATFORM_WINDOWS
#include <memoryapi.h>
#endif
#if RICHC_PLATFORM_LINUX || RICHC_PLATFORM_MACOS
#include <unistd.h>
#include <sys/mman.h>
#endif


#define RESERVE_SIZE (1ULL<<30)
#define COMMIT_SIZE (4096ULL)       // standard page size for all supported OSes
#define ALIGNMENT (16)


static void arena_reserve_region(arena_t *arena) {
    check(arena);
    check(!arena->base);
#if RICHC_PLATFORM_WINDOWS
    void *ptr = VirtualAlloc(0, RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
    require(ptr);
#endif
#if RICHC_PLATFORM_LINUX
    void *ptr = mmap(0, RESERVE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(ptr != MAP_FAILED);
#endif
#if RICHC_PLATFORM_MACOS
    void *ptr = mmap(0, RESERVE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    require(ptr != MAP_FAILED);
#endif
    arena->base = ptr;
    arena->offset = 0;
    arena->size = 0;
}


static void arena_commit_region(arena_t *arena, int32_t size) {
    check(arena);
    check(arena->base);
    size_t aligned_size = ((size_t)size + COMMIT_SIZE - 1) & ~(COMMIT_SIZE - 1);
    if ((size_t)arena->size < aligned_size) {
#if RICHC_PLATFORM_WINDOWS
        void *ptr = VirtualAlloc(arena->base, aligned_size, MEM_COMMIT, PAGE_READWRITE);
        require(ptr);
#endif
#if RICHC_PLATFORM_LINUX || RICHC_PLATFORM_MACOS
        int error = mprotect(arena->base, aligned_size, PROT_READ | PROT_WRITE);
        require(!error);
#endif
        arena->size = (int32_t)aligned_size;
    }
}


static void arena_decommit_region(arena_t *arena) {
    check(arena);
    check(arena->base);
#if RICHC_PLATFORM_WINDOWS
    int success = VirtualFree(arena->base, arena->size, MEM_DECOMMIT);
    require(success);
#endif
#if RICHC_PLATFORM_LINUX || RICHC_PLATFORM_MACOS
    int error = mprotect(arena->base, arena->size, PROT_NONE) || madvise(arena->base, arena->size, MADV_DONTNEED);
    require(!error);
#endif
    arena->offset = 0;
    arena->size = 0;
}


static inline int32_t arena_get_aligned_size(int32_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}


arena_t arena_make(void) {
    arena_t arena = {0};
    arena_reserve_region(&arena);
    return arena;
}


arena_t arena_make_with_size(int32_t initial_size) {
    arena_t arena = arena_make();
    arena_commit_region(&arena, initial_size);
    return arena;
}


void arena_init(arena_t *arena, int32_t initial_size) {
    check(arena);
    // check that this is a sensible address, not uninitialized junk
    check(((uintptr_t)arena->base & 0xFF) == 0);

    if (arena->base) {
        arena_deinit(arena);
    }
    else {
        arena_reserve_region(arena);
    }
    arena_commit_region(arena, initial_size);
}


void arena_deinit(arena_t *arena) {
    check(arena);
    check(((uintptr_t)arena->base & 0xFF) == 0);
    arena_decommit_region(arena);
}


void *arena_alloc(arena_t *arena, int32_t size) {
    check(arena);
    check(arena->base);
    check(size > 0);

    int32_t aligned_size = arena_get_aligned_size(size);
    int32_t new_offset = arena->offset + aligned_size;
    arena_commit_region(arena, new_offset);

    void *ptr = (char *)arena->base + arena->offset;
    arena->offset = new_offset;
    return ptr;
}


void *arena_calloc(arena_t *arena, int32_t size) {
    void *ptr = arena_alloc(arena, size);
    memset(ptr, 0, (size_t)arena_get_aligned_size(size));
    return ptr;
}


static inline bool arena_is_last_alloc(const arena_t *arena, const void *ptr, int32_t aligned_size) {
    check(aligned_size > 0 && (aligned_size & (ALIGNMENT - 1)) == 0);
    return (const char *)ptr + aligned_size == (const char *)arena->base + arena->offset;
}


void *arena_realloc(arena_t *arena, void *old_ptr, int32_t old_size, int32_t new_size) {
    check(arena);
    check(arena->base);
    check(old_size > 0);
    check(new_size > 0);

    // reallocing with a null old ptr is equivalent to an alloc
    if (!old_ptr) {
        return arena_alloc(arena, new_size);
    }

    int32_t old_aligned_size = arena_get_aligned_size(old_size);
    int32_t new_aligned_size = arena_get_aligned_size(new_size);

    // If the aligned size is no bigger, do nothing
    if (new_aligned_size <= old_aligned_size) {
        return old_ptr;
    }

    // If the old ptr is the last allocation in the arena, just grow it in-place
    if (arena_is_last_alloc(arena, old_ptr, old_aligned_size)) {
        arena_commit_region(arena, new_aligned_size);
        arena->offset += (new_aligned_size - old_aligned_size);
        return old_ptr;
    }

    // Otherwise allocate and copy
    void *new_ptr = arena_alloc(arena, new_aligned_size);
    memcpy(new_ptr, old_ptr, old_aligned_size);
    return new_ptr;
}


void arena_free(arena_t *arena, void *ptr, int32_t size) {
    check(arena);
    check(arena->base);
    check(size > 0);

    int32_t aligned_size = arena_get_aligned_size(size);

    // If we're freeing the last alloc in the block, adjust the offset.
    // Otherwise free does nothing.
    if (arena_is_last_alloc(arena, ptr, aligned_size)) {
        arena->offset -= aligned_size;
    }
}


void arena_reset(arena_t *arena) {
    check(arena);
    check(arena->base);
    arena->offset = 0;
}
