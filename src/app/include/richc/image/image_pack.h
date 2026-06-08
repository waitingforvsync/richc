/*
 * image_pack.h - pack multiple CPU images into a single atlas image.
 *
 * rc_image_pack places every input image into one atlas (for later upload as a
 * single GPU texture) and returns each image's top-left position within it.  It
 * is a thin wrapper over the core rectangle packer (richc/rect_pack.h).
 *
 * Atlas format
 * ------------
 * The atlas takes the widest format among the inputs (R8 < RGB8 < RGBA8); each
 * source is widened into it by rc_image_blit.  The atlas is cleared, so the gaps
 * between packed images read as zero.
 *
 * Positions, not rectangles
 * -------------------------
 * The result reports positions[i] = the top-left of images[i] in the atlas.  The
 * caller already holds the sizes, so it derives a rectangle when needed with
 * rc_box2i_make_pos_size(positions[i], images[i].size).
 *
 * Sizing
 * ------
 * Pass a positive size to pack into a fixed container; the pack fails as a whole
 * (returns {0}) if everything does not fit.  Pass size == {0, 0} to auto-size:
 * the wrapper estimates the area, starts from a power-of-two square, and grows
 * (alternately doubling width then height) until it fits.  The chosen container
 * is reported back as result.image.size.
 *
 * Failure
 * -------
 * Returns a zero-initialised result (image.data.data == NULL, positions empty)
 * if images is empty, a fixed-size pack does not fit, or an auto-size pack would
 * exceed the addressable atlas byte size.  Nothing is written to arena on
 * failure.
 */

#ifndef RC_IMAGE_IMAGE_PACK_H_
#define RC_IMAGE_IMAGE_PACK_H_

#include "richc/arena.h"
#include "richc/image/array/image.h"
#include "richc/math/array/vec2i.h"

/*
 * On success: image is the packed atlas (allocated from arena); positions[i] is
 * the top-left of images[i] in the atlas (also from arena), indexed by input.
 * On failure: zero-initialised (image.data.data == NULL, positions.num == 0).
 */
typedef struct rc_image_pack_result {
    rc_image      image;
    rc_span_vec2i positions;
} rc_image_pack_result;

/*
 * Pack all images into an atlas.
 *
 * images   - input images; each must be valid (non-NULL data, size > 0).
 * size     - atlas dimensions in pixels, or {0, 0} to choose a size automatically.
 * spacing  - minimum pixel gap maintained between packed images.
 * arena    - receives the atlas pixels and the positions array on success.
 * scratch  - temporary working space (discarded); a different arena from arena.
 *
 * Returns zero-initialised on failure; nothing written to arena in that case.
 */
rc_image_pack_result rc_image_pack(rc_view_image images, rc_vec2i size,
                                   int32_t spacing, rc_arena *arena, rc_arena scratch);

#endif /* RC_IMAGE_IMAGE_PACK_H_ */
