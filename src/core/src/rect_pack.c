/*
 * rect_pack.c - Maximal Rectangles bin packing, Best Short Side Fit.
 *
 * The free-rect list is carved around each placed rectangle: every free rect it
 * overlaps (after inflating by spacing) is removed and replaced by the strips of
 * itself that lie outside the inflated region, with strips already contained by
 * another free rect pruned.  Placement picks the free rect leaving the smallest
 * short-side remainder (BSSF).
 */

#include "richc/rect_pack.h"

#include <limits.h>

#include "richc/array/u32.h"

/* ---- batch ordering: sort indices by decreasing max(width, height) ---- */

static inline int32_t max_side(rc_vec2i s)
{
    return s.x > s.y ? s.x : s.y;
}

#define RC_SORT_TYPE            uint32_t
#define RC_SORT_SPAN            rc_span_u32
#define RC_SORT_NAME            rc_sort_by_max_side
#define RC_SORT_CTX             rc_view_vec2i
#define RC_SORT_CMP(ctx, a, b)  (max_side(rc_view_vec2i_get(*ctx, a)) > max_side(rc_view_vec2i_get(*ctx, b)))
#include "richc/template/algorithm/sort.h"

/* ---- free-rect splitting ---- */

/*
 * Remove every free rect that intersects the inflated placed region, split each
 * one into up to four axis-aligned strips around the inflated region, and append
 * any strip not already contained within an existing free rect.
 */
static void split_free_rects(rc_array_box2i *free, rc_box2i placed,
                              int32_t spacing, rc_arena *arena)
{
    rc_box2i infl = rc_box2i_make_with_margin(rc_box2i_min(placed), rc_box2i_max(placed), spacing);

    // Swap-remove pass over free->num.  Non-intersecting rects advance i;
    // intersecting rects are swap-removed and their splits appended.  Splits lie
    // strictly outside infl by construction, so they are skipped (i++) when
    // reached.  Bounding on free->num (not a fixed count) stops correctly even
    // when every rect is removed and none re-added.
    uint32_t i = 0;
    while (i < free->num) {
        rc_box2i f = rc_array_box2i_get(free, i);

        if (!rc_box2i_intersects(f, infl)) {
            i++;
            continue;
        }

        // Swap-remove: pop the last element and, unless i was itself the last,
        // overwrite slot i with it.  Popping first keeps the index in range.
        rc_box2i last = rc_array_box2i_pop(free);
        if (i < free->num)
            rc_array_box2i_set(free, i, last);

        rc_vec2i fmin = rc_box2i_min(f);
        rc_vec2i fmax = rc_box2i_max(f);
        rc_vec2i imin = rc_box2i_min(infl);
        rc_vec2i imax = rc_box2i_max(infl);

        // Up to four strips, one per side of infl that lies inside f.
        rc_box2i splits[4];
        uint32_t sc = 0;

        if (imin.x > fmin.x)
            splits[sc++] = rc_box2i_make(fmin, rc_vec2i_make(imin.x, fmax.y));
        if (imax.x < fmax.x)
            splits[sc++] = rc_box2i_make(rc_vec2i_make(imax.x, fmin.y), fmax);
        if (imin.y > fmin.y)
            splits[sc++] = rc_box2i_make(fmin, rc_vec2i_make(fmax.x, imin.y));
        if (imax.y < fmax.y)
            splits[sc++] = rc_box2i_make(rc_vec2i_make(fmin.x, imax.y), fmax);

        // Append each split not already dominated by a current free rect.
        for (uint32_t j = 0; j < sc; j++) {
            bool dominated = false;
            for (uint32_t k = 0; k < free->num && !dominated; k++)
                dominated = rc_box2i_contains(rc_array_box2i_get(free, k), splits[j]);
            if (!dominated)
                rc_array_box2i_push(free, splits[j], arena);
        }
    }
}

/* ---- public API ---- */

rc_rect_pack rc_rect_pack_make(rc_vec2i container, int32_t spacing, rc_arena *arena)
{
    RC_ASSERT(container.x > 0 && container.y > 0);
    RC_ASSERT(spacing >= 0);

    rc_array_box2i free = rc_array_box2i_make(16, arena);
    rc_array_box2i_push(&free, rc_box2i_make_pos_size(rc_vec2i_make_zero(), container), arena);

    return (rc_rect_pack) {
        .free      = free,
        .container = container,
        .spacing   = spacing
    };
}

rc_rect_pack_result rc_rect_pack_add(rc_rect_pack *p, rc_vec2i size, rc_arena *arena)
{
    RC_ASSERT(size.x > 0 && size.y > 0);

    int32_t  best_score = INT32_MAX;
    uint32_t best_idx   = RC_INDEX_NONE;

    for (uint32_t j = 0; j < p->free.num; j++) {
        rc_vec2i fs = rc_box2i_size(rc_array_box2i_get(&p->free, j));
        if (size.x <= fs.x && size.y <= fs.y) {
            int32_t lx    = fs.x - size.x;
            int32_t ly    = fs.y - size.y;
            int32_t score = lx < ly ? lx : ly;
            if (score < best_score) {
                best_score = score;
                best_idx   = j;
            }
        }
    }

    if (best_idx == RC_INDEX_NONE)
        return (rc_rect_pack_result) {.placed = false};

    // The placement sits at the chosen free rect's top-left corner.
    rc_vec2i pos = rc_box2i_min(rc_array_box2i_get(&p->free, best_idx));
    split_free_rects(&p->free, rc_box2i_make_pos_size(pos, size), p->spacing, arena);

    return (rc_rect_pack_result) {
        .pos    = pos,
        .placed = true
    };
}

rc_span_vec2i rc_rect_pack_all(rc_vec2i container, int32_t spacing,
                               rc_view_vec2i sizes,
                               rc_arena *arena, rc_arena scratch)
{
    // A by-value scratch shares its source's base; equal bases mean the same
    // arena was passed for output and scratch, which would alias the free list
    // with the result array.
    RC_ASSERT(arena->base != scratch.base);

    if (sizes.num == 0)
        return (rc_span_vec2i) {0};

    // Order indices by decreasing max-side.  Sized once and never grown again,
    // so the packer's free list (created after it) stays the latest scratch
    // allocation and grows in place.
    rc_array_u32 order = rc_array_u32_make(sizes.num, &scratch);
    rc_array_u32_resize(&order, sizes.num, &scratch);
    for (uint32_t i = 0; i < sizes.num; i++)
        rc_array_u32_set(&order, i, i);
    rc_sort_by_max_side(order.span, &sizes);

    rc_rect_pack p = rc_rect_pack_make(container, spacing, &scratch);

    // Result is the only arena allocation here, so on failure deinit reclaims it
    // (it is the latest allocation) and leaves arena untouched.
    rc_array_vec2i positions = rc_array_vec2i_make(sizes.num, arena);
    rc_array_vec2i_resize(&positions, sizes.num, arena);

    for (uint32_t i = 0; i < sizes.num; i++) {
        uint32_t idx = rc_array_u32_get(&order, i);
        rc_rect_pack_result r = rc_rect_pack_add(&p, rc_view_vec2i_get(sizes, idx), &scratch);
        if (!r.placed) {
            rc_array_vec2i_deinit(&positions, arena);
            return (rc_span_vec2i) {0};
        }
        rc_array_vec2i_set(&positions, idx, r.pos);
    }

    return positions.span;
}
