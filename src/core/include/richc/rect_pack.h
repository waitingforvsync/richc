/*
 * rect_pack.h - pack axis-aligned rectangles into a container with spacing.
 *
 * Algorithm
 * ---------
 * Maximal Rectangles with Best Short Side Fit (BSSF).  The free-rect list is
 * carved around each placed rectangle (inflated by the spacing gap), and any
 * free rect fully contained within another is pruned.  No rotation is attempted.
 *
 * Spacing
 * -------
 * Each placed rectangle is inflated by spacing on all sides before the free list
 * is carved, maintaining a gap of at least spacing pixels between any two placed
 * rectangles.  No border gap is enforced at the container edges.
 *
 * Two ways to pack
 * ----------------
 * rc_rect_pack_all is the from-scratch path: hand it every size at once and it
 * sorts them (largest first) for a denser packing, returning all positions or
 * failing as a whole.  Its free list is transient, living in a scratch arena.
 *
 * rc_rect_pack_make / rc_rect_pack_add is the incremental path: the packer
 * retains its free list as state in the caller's arena, so rectangles can be
 * added over time (e.g. glyphs appended to a font atlas) without disturbing
 * earlier placements.  Each batch added is only as dense as its own arrival
 * order allows.
 *
 * Arenas
 * ------
 * Nothing here captures an arena; an allocating call takes one as a parameter.
 * For the retained packer, pass the same arena to make and every add, and let
 * the free list be that arena's only growable so it grows in place.
 */

#ifndef RC_RECT_PACK_H_
#define RC_RECT_PACK_H_

#include "richc/arena.h"
#include "richc/math/array/box2i.h"
#include "richc/math/array/vec2i.h"

/*
 * A retained packer.  free is the free-rectangle list (internal state) and is
 * the only growable the packer owns.  container and spacing are the fixed
 * configuration set at make time.
 */
typedef struct rc_rect_pack {
    rc_array_box2i free;
    rc_vec2i       container;
    int32_t        spacing;
} rc_rect_pack;

/* Result of placing one rectangle: its top-left position, and whether it fit. */
typedef struct rc_rect_pack_result {
    rc_vec2i pos;
    bool     placed;
} rc_rect_pack_result;

/*
 * Create a packer for a container-sized bin with a spacing-pixel gap between
 * placed rectangles.  Seeds the free list with the whole container (one
 * allocation from arena).  container.x and container.y must be > 0; spacing
 * must be >= 0.
 */
rc_rect_pack rc_rect_pack_make(rc_vec2i container, int32_t spacing, rc_arena *arena);

/*
 * Place one size rectangle, returning its position with placed == true on
 * success, or {.placed = false} (position zero) when it does not fit.  arena is
 * touched only when the free list must grow; pass the same arena as make.
 * size.x and size.y must be > 0.
 */
rc_rect_pack_result rc_rect_pack_add(rc_rect_pack *p, rc_vec2i size, rc_arena *arena);

/*
 * From-scratch pack of a whole batch.  Sorts the sizes by decreasing max(side)
 * for a denser result, then places them all.  Returns an array of positions
 * indexed by sizes (positions.num == sizes.num), allocated from arena.  Returns
 * the invalid {0} array if the whole set does not fit, or if sizes is empty;
 * arena is left untouched in that case.  scratch (by value, discarded) holds the
 * free list and the sort permutation, and must be a different arena from arena.
 */
rc_array_vec2i rc_rect_pack_all(rc_vec2i container, int32_t spacing,
                                rc_view_vec2i sizes,
                                rc_arena *arena, rc_arena scratch);

#endif /* RC_RECT_PACK_H_ */
