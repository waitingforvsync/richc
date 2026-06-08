/*
 * image_atlas.h - an image atlas you fill incrementally, one image at a time.
 *
 * Pairs a fixed-size atlas image with a retained rectangle packer (core
 * rect_pack), so images can be added over time - e.g. font glyphs the first time
 * each is encountered - without disturbing earlier placements. This is the
 * streaming counterpart to rc_image_pack (image_pack.h), which packs a whole set
 * at once for a denser result. Use rc_image_pack when the set is known up front;
 * use rc_image_atlas when images arrive incrementally and existing placements
 * must stay put (callers hold positions into them).
 *
 * Arenas
 * ------
 * One arena, no scratch. The atlas pixels are allocated once at make and never
 * grow; only the packer's free-rect list grows (on add). Pass the same arena to
 * make and every add, and let the free list be that arena's only growable so it
 * grows in place. The atlas captures no arena (it is a parameter each call).
 *
 * Format and clearing
 * -------------------
 * Choose the atlas format up front; each added image is widened into it by
 * rc_image_blit (so a source must be no wider than the atlas format). The atlas
 * is cleared, so gaps between images read as zero.
 */

#ifndef RC_IMAGE_IMAGE_ATLAS_H_
#define RC_IMAGE_IMAGE_ATLAS_H_

#include "richc/arena.h"
#include "richc/image/image.h"
#include "richc/rect_pack.h"

/*
 * image is the atlas pixels (fixed size, allocated once); packer holds the
 * free-rect state and is the only growable. Read image to upload or sample the
 * atlas; the packer is internal.
 */
typedef struct rc_image_atlas {
    rc_image     image;
    rc_rect_pack packer;
} rc_image_atlas;

/*
 * Create a cleared size-by-size atlas of the given format, with a spacing-pixel
 * gap maintained between added images. format must not be NONE; size must be
 * positive. Allocates the atlas pixels and the packer's initial free list from
 * arena.
 */
rc_image_atlas rc_image_atlas_make(rc_vec2i size, rc_pixel_format format,
                                   int32_t spacing, rc_arena *arena);

/*
 * Place src into the atlas at the packer's chosen position and blit its pixels
 * in. Returns {.pos, .placed = true} on success, or {.placed = false} (pos zero)
 * when src no longer fits. Pass the same arena as make. Earlier placements are
 * never moved. Precondition: src is valid and no wider than the atlas format.
 */
rc_rect_pack_result rc_image_atlas_add(rc_image_atlas *atlas, rc_image src,
                                       rc_arena *arena);

#endif /* RC_IMAGE_IMAGE_ATLAS_H_ */
