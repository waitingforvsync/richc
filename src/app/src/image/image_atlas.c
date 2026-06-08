/*
 * image_atlas.c - incremental image atlas over the core rectangle packer.
 *
 * The atlas pixels are allocated once and never grow; only the packer's free
 * list grows on add. Allocating the pixels first, then seeding the packer, keeps
 * the free list as the latest arena allocation (so it grows in place) while the
 * pixel buffer stays put for the atlas's lifetime.
 */

#include "richc/image/image_atlas.h"

rc_image_atlas rc_image_atlas_make(rc_vec2i size, rc_pixel_format format,
                                   int32_t spacing, rc_arena *arena)
{
    RC_ASSERT(format != RC_PIXEL_FORMAT_NONE);

    // Pixels first (fixed size, never relocated), then the packer's free list
    // (the sole growable, now the latest allocation).
    rc_image image = rc_image_make(size, format, arena);
    rc_rect_pack packer = rc_rect_pack_make(size, spacing, arena);

    return (rc_image_atlas) {
        .image  = image,
        .packer = packer
    };
}

rc_rect_pack_result rc_image_atlas_add(rc_image_atlas *atlas, rc_image src,
                                       rc_arena *arena)
{
    RC_ASSERT(src.data.data != NULL);
    RC_ASSERT(src.size.x > 0 && src.size.y > 0);
    // The atlas format must be at least as wide as src; blit never narrows.
    RC_ASSERT(src.format <= atlas->image.format);

    rc_rect_pack_result r = rc_rect_pack_add(&atlas->packer, src.size, arena);
    if (r.placed)
        // In bounds by construction (the packer placed it within the atlas), so
        // the blit clips nothing and always succeeds.
        rc_image_blit(atlas->image, r.pos, src);

    return r;
}
