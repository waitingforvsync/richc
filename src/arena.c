#include <string.h>     // for memcpy etc
#include "richc/arena.h"


#define ARENA_BLOCK_HEADER_SIZE (32)
#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)

// Arenas are coarse allocations from which smaller allocations are made in a stack-like manner.
// Instead of freeing allocations individually, they may be freed in one go by resetting the arena.

typedef struct arena_block_header_t arena_block_header_t;

struct arena_block_header_t {
    arena_block_header_t *prev;
    arena_block_header_t *next;
    int32_t size;
    int32_t default_size;
};

static_check(ssizeof(arena_block_header_t) <= ARENA_BLOCK_HEADER_SIZE, "struct arena_block_header_t is too big");


arena_t arena_make(void) {
    arena_t arena = {0};
    arena_init(&arena, ARENA_DEFAULT_BLOCK_SIZE);
    return arena;
}


arena_t arena_make_with_size(int32_t initial_size) {
    arena_t arena = {0};
    arena_init(&arena, initial_size);
    return arena;
}


static arena_block_header_t *arena_add_block(int32_t size, arena_block_header_t *prev_block) {
    arena_block_header_t *block = calloc(size, 1);
    require(block);

    if (prev_block) {
        prev_block->next = block;
        block->prev = prev_block;
        block->default_size = prev_block->default_size;
    }
    block->size = size;
    return block;
}


static arena_block_header_t *arena_insert_block(int32_t size, arena_block_header_t *next_block) {
    check(next_block);
    arena_block_header_t *block = arena_add_block(size, next_block->prev);
    block->next = next_block;
    block->default_size = next_block->default_size;
    next_block->prev = block;
    return block;
}


void arena_init(arena_t *arena, int32_t initial_size) {
    check(arena);
    check(initial_size > 0);

    if (arena->base) {
        arena_deinit(arena);
    }

    arena_block_header_t *block = arena_add_block(initial_size, 0);
    block->default_size = block->size;

    // Initialize arena struct members
    arena->base = block;
    arena->offset = ARENA_BLOCK_HEADER_SIZE;
}


void arena_deinit(arena_t *arena) {
    check(arena);

    if (arena->base) {
        arena_block_header_t *block = arena->base;

        // Get the last block in the chain
        while (block->next) {
            block = block->next;
        }

        // Free in reverse order, to reflect the rough order they were allocated in, until we reach the new tail block
        while (block) {
            arena_block_header_t *prev = block->prev;
            free(block);
            block = prev;
        }

        arena->base = 0;
    }
    arena->offset = 0;
}


static inline int32_t arena_get_aligned_size(int32_t size) {
    return (size + 0x0F) & ~0x0F;
}


static inline bool arena_is_last_alloc(arena_t *arena, void *ptr, int32_t size) {
    check(size > 0);
    return (char *)ptr + size == (char *)arena->base + arena->offset;
}


void *arena_alloc(arena_t *arena, int32_t size) {
    check(arena);
    check(arena->base);
    check(size > 0);

    arena_block_header_t *block = arena->base;

    // Align the allocation to a multiple of 16 bytes
    int32_t aligned_size = arena_get_aligned_size(size);
    int32_t new_offset = arena->offset + aligned_size;

    // Simple case that we can just allocate directly at the end of the current block
    if (new_offset <= block->size) {
        void *ptr = (char *)block + arena->offset;
        arena->offset = new_offset;
        return ptr;
    }
    
    // If we got here, this is the case that we ran out of space in the current block

    // This is the minimum size of the block we will need in order to allocate this
    int32_t size_with_block_header = ARENA_BLOCK_HEADER_SIZE + aligned_size;

    // Check whether this is the first allocation in the block.
    // This could happen if we initialise an arena and then the first alloc is bigger than the initial block
    // If so, we can just reallocate the whole block.
    // Note we give this allocation a block to itself by allocating just enough and no more.
    // This gives it the ability to resize itself by reallocating the block, without affecting other allocs.
    if (arena->offset == ARENA_BLOCK_HEADER_SIZE) {
        block = arena_insert_block(size_with_block_header, block);
        arena->base = block;
        arena->offset = size_with_block_header;
        return (char *)block + ARENA_BLOCK_HEADER_SIZE;
    }

    // Check whether there is already a next block allocated.
    // This could happen if we restored an old arena state back to a previous block.
    // If there isn't one, or if there is, but it's too small for the alloc, insert one
    arena_block_header_t *next_block = block->next;
    if (next_block && next_block->size < size_with_block_header) {
        next_block = arena_insert_block(size_with_block_header, next_block);
    }

    // Allocate a new block
    if (!next_block) {
        next_block = arena_add_block(block->default_size, block);
    }

    // And point the arena object at the new block
    arena->base = next_block;
    arena->offset = size_with_block_header;
    return (char *)next_block + ARENA_BLOCK_HEADER_SIZE;
}


void *arena_calloc(arena_t *arena, int32_t size) {
    check(arena);
    check(arena->base);
    check(size > 0);
    void *ptr = arena_alloc(arena, size);
    memset(ptr, 0, arena_get_aligned_size(size));
    return ptr;
}


void *arena_realloc(arena_t *arena, void *old_ptr, int32_t old_size, int32_t new_size) {
    check(arena);
    check(arena->base);
    check(old_size > 0);
    check(new_size > 0);

    if (!old_ptr) {
        return arena_alloc(arena, new_size);
    }

    int32_t old_aligned_size = arena_get_aligned_size(old_size);
    int32_t new_aligned_size = arena_get_aligned_size(new_size);

    if (new_aligned_size <= old_aligned_size) {
        return old_ptr;
    }

    // If the alloc we want to reallocate is the last one in the block...
    if (arena_is_last_alloc(arena, old_ptr, old_aligned_size)) {
        arena_block_header_t *block = arena->base;

        // Then, if it fits, just amend the arena offset and return the same pointer
        if (arena->offset + (new_aligned_size - old_aligned_size) <= block->size) {
            arena->offset += (new_aligned_size - old_aligned_size);
            return old_ptr;
        }

        // If it doesn't fit, but is the only allocation in the block, just reallocate the block
        if ((char *)old_ptr - (char *)block == ARENA_BLOCK_HEADER_SIZE) {
            arena_block_header_t *new_block = realloc(block, new_aligned_size + ARENA_BLOCK_HEADER_SIZE);
            check(new_block);

            if (new_block->prev) {
                new_block->prev->next = new_block;
            }
            if (new_block->next) {
                new_block->next->prev = new_block;
            }
            return (char *)new_block + ARENA_BLOCK_HEADER_SIZE;
        }
    }

    void *ptr = arena_alloc(arena, new_aligned_size);
    memcpy(ptr, old_ptr, old_size);
    return ptr;
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

    arena_block_header_t *block = arena->base;
    while (block->prev) {
        block = block->prev;
    }

    arena->base = block;
    arena->offset = ARENA_BLOCK_HEADER_SIZE;
}


int32_t arena_get_current_block_size(const arena_t *arena) {
    check(arena);

    arena_block_header_t *block = arena->base;
    return block->size;
}
