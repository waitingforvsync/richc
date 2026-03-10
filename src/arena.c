#include "richc/arena.h"

#include <stdbool.h>
#include <string.h>

/* ---- private helpers ---- */

static uint32_t rc_arena_align_up_(uint32_t n)
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
static bool rc_arena_grow_committed_(rc_arena *a, uint32_t new_top)
{
    size_t to_commit = rc_platform_round_up_to_page((size_t)(new_top - a->committed));
    if (!rc_platform_commit(a->base + a->committed, to_commit))
        return false;
    a->committed += (uint32_t)to_commit;
    return true;
}

/* Shared implementation for rc_arena_realloc / rc_arena_realloc_zero.
 * Requires ptr != NULL and new_size != 0. */
static void *rc_arena_realloc_(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    if (!new_size) {(void)rc_arena_free(a, ptr, old_size); return NULL;}

    uint32_t aligned_old = rc_arena_align_up_(old_size);
    uint32_t aligned_new = rc_arena_align_up_(new_size);
    uint32_t ptr_offset  = (uint32_t)(size_t)((char *)ptr - a->base);
    bool     is_last     = (ptr_offset + aligned_old == a->top);

    if (is_last) {
        if (aligned_new <= aligned_old) {
            /* Shrink: move top down. */
            a->top = ptr_offset + aligned_new;
            return ptr;
        }

        /* Grow in place. */
        uint32_t new_top = ptr_offset + aligned_new;
        if (new_top > a->reserved) return NULL;
        if (new_top > a->committed && !rc_arena_grow_committed_(a, new_top))
            return NULL;
        a->top = new_top;
        return ptr;
    }

    /* Not the last allocation. */
    if (aligned_new <= aligned_old)
        return ptr;             /* shrink: no-op */

    void *new_ptr = rc_arena_alloc(a, new_size);
    if (!new_ptr) return NULL;
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
    if (a->base)
        rc_platform_release(a->base, a->reserved);
    *a = (rc_arena) {0};
}

void *rc_arena_alloc(rc_arena *a, uint32_t size)
{
    if (!size) return NULL;

    uint32_t aligned = rc_arena_align_up_(size);
    if (aligned < size) return NULL;                       /* overflow     */
    if (aligned > a->reserved - a->top) return NULL;      /* out of space */

    uint32_t new_top = a->top + aligned;
    if (new_top > a->committed && !rc_arena_grow_committed_(a, new_top))
        return NULL;

    void *result = a->base + a->top;
    a->top = new_top;
    return result;
}

void *rc_arena_alloc_zero(rc_arena *a, uint32_t size)
{
    void *p = rc_arena_alloc(a, size);
    if (p) memset(p, 0, size);
    return p;
}

bool rc_arena_free(rc_arena *a, void *ptr, uint32_t size)
{
    uint32_t ptr_offset = (uint32_t)(size_t)((char *)ptr - a->base);
    if (ptr_offset + rc_arena_align_up_(size) == a->top) {
        a->top = ptr_offset;
        return true;
    }
    return false;
}

void *rc_arena_realloc(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    if (!ptr) return rc_arena_alloc(a, new_size);
    return rc_arena_realloc_(a, ptr, old_size, new_size);
}

void *rc_arena_realloc_zero(rc_arena *a, void *ptr, uint32_t old_size, uint32_t new_size)
{
    if (!ptr) return rc_arena_alloc_zero(a, new_size);
    void *result = rc_arena_realloc_(a, ptr, old_size, new_size);
    if (result && new_size > old_size)
        memset((char *)result + old_size, 0, new_size - old_size);
    return result;
}

void rc_arena_reset(rc_arena *a)
{
    if (!a->base) return;
    uint32_t keep = (uint32_t)rc_platform_page_size();
    if (a->committed > keep) {
        rc_platform_decommit(a->base + keep, a->committed - keep);
        a->committed = keep;
    }
    a->top = 0;
}
