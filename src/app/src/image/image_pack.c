/*
 * image_pack.c - pack CPU images into a single atlas via the core rectangle
 * packer.  The atlas takes the widest input format; sources are widened into it
 * by rc_image_blit.  With size == {0, 0} the container is chosen automatically,
 * starting from a power-of-two square/2:1 rectangle and growing until everything
 * fits.
 */

#include "richc/image/image_pack.h"

#include "richc/rect_pack.h"

/* ---- input scans ---- */

// The widest pixel format across all inputs (R8 < RGB8 < RGBA8).
static rc_pixel_format widest_format(rc_view_image images)
{
    rc_pixel_format fmt = RC_PIXEL_FORMAT_R8;
    for (uint32_t i = 0; i < images.num; i++) {
        rc_pixel_format f = rc_view_image_get(images, i).format;
        if (f > fmt)
            fmt = f;
    }
    return fmt;
}

// Total pixel area (sum of w*h) across all images, in 64 bits to avoid overflow.
static uint64_t total_area(rc_view_image images)
{
    uint64_t area = 0;
    for (uint32_t i = 0; i < images.num; i++) {
        rc_vec2i s = rc_view_image_get(images, i).size;
        area += (uint64_t)(uint32_t)s.x * (uint32_t)s.y;
    }
    return area;
}

// Gather the input image sizes into a scratch array (asserts each image valid),
// returning a view over them for the packer.
static rc_view_vec2i collect_sizes(rc_view_image images, rc_arena *scratch)
{
    rc_array_vec2i sizes = rc_array_vec2i_make(images.num, scratch);
    rc_array_vec2i_resize(&sizes, images.num, scratch);
    for (uint32_t i = 0; i < images.num; i++) {
        rc_image img = rc_view_image_get(images, i);
        RC_ASSERT(img.data.data != NULL);
        RC_ASSERT(img.size.x > 0 && img.size.y > 0);
        rc_array_vec2i_set(&sizes, i, img.size);
    }
    return sizes.view;
}

/* ---- auto-size ladder ---- */

// Step to the next size on the square / 2:1 (width == 2*height) ladder: a square
// widens into a 2:1 rectangle, a 2:1 rectangle heightens back into a square.
static rc_vec2i ladder_next(rc_vec2i size)
{
    return size.x <= size.y
        ? rc_vec2i_make(size.x * 2, size.y)
        : rc_vec2i_make(size.x, size.y * 2);
}

// Smallest ladder size (square or width == 2*height) whose area covers `area`.
static rc_vec2i start_size(uint64_t area)
{
    rc_vec2i s = rc_vec2i_make(1, 1);
    while (s.x < (1 << 20) && (uint64_t)(uint32_t)s.x * (uint32_t)s.y < area)
        s = ladder_next(s);
    return s;
}

/* ---- public API ---- */

rc_image_pack_result rc_image_pack(rc_view_image images, rc_vec2i size,
                                   int32_t spacing, rc_arena *arena, rc_arena scratch)
{
    if (images.num == 0)
        return (rc_image_pack_result) {0};

    rc_pixel_format fmt = widest_format(images);
    uint32_t bpp = rc_pixel_format_bytes_per_pixel(fmt);
    rc_view_vec2i sizes = collect_sizes(images, &scratch);

    // size == {0,0} requests auto-sizing from a ladder size covering the total
    // area; otherwise the container is fixed.
    bool auto_size = (size.x == 0 && size.y == 0);
    rc_vec2i container = auto_size ? start_size(total_area(images)) : size;

    // Grow-and-retry does not leak: a failed rc_rect_pack_all reclaims its own
    // result and leaves arena untouched, and scratch is taken by value so each
    // attempt's scratch allocations are discarded.  Only the final, successful
    // positions survives in arena.
    rc_span_vec2i positions = {0};
    for (;;) {
        // arena offsets are uint32, so the atlas byte size must stay in range.
        if ((uint64_t)(uint32_t)container.x * (uint32_t)container.y * bpp > UINT32_MAX)
            return (rc_image_pack_result) {0};
        positions = rc_rect_pack_all(container, spacing, sizes, arena, scratch);
        if (rc_span_vec2i_is_valid(positions))
            break;
        if (!auto_size)
            return (rc_image_pack_result) {0};
        container = ladder_next(container);
    }

    rc_image atlas = rc_image_make(container, fmt, arena);
    for (uint32_t i = 0; i < images.num; i++)
        rc_image_blit(atlas, rc_span_vec2i_get(positions, i), rc_view_image_get(images, i));

    return (rc_image_pack_result) {
        .image     = atlas,
        .positions = positions
    };
}
