#include "richc/image/image.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(image) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(image, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(image, fix)
{
    rc_arena_deinit(&fix->a);
}

RC_TEST(image, bytes_per_pixel)
{
    RC_CHECK(rc_pixel_format_bytes_per_pixel(RC_PIXEL_FORMAT_NONE),  ==, 0u);
    RC_CHECK(rc_pixel_format_bytes_per_pixel(RC_PIXEL_FORMAT_R8),    ==, 1u);
    RC_CHECK(rc_pixel_format_bytes_per_pixel(RC_PIXEL_FORMAT_RGB8),  ==, 3u);
    RC_CHECK(rc_pixel_format_bytes_per_pixel(RC_PIXEL_FORMAT_RGBA8), ==, 4u);
}

RC_TEST_STEP(image, make_cleared, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(4, 3), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    RC_CHECK(img.size.x, ==, 4);
    RC_CHECK(img.size.y, ==, 3);
    RC_CHECK(img.stride, ==, 16u);          // 4 px * 4 bytes
    RC_CHECK(img.data.num, ==, 48u);        // 16 * 3 rows
    RC_CHECK(img.format, ==, RC_PIXEL_FORMAT_RGBA8);
    for (uint32_t i = 0; i < img.data.num; i++)
        RC_CHECK(rc_span_bytes_get(img.data, i), ==, (uint8_t)0);
}

RC_TEST_STEP(image, make_fill_rgba8, fix)
{
    // packed fill R=0x11 G=0x22 B=0x33 A=0x44 -> every pixel byte-for-byte
    rc_image img = rc_image_make_filled(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_RGBA8, 0x44332211u, &fix->a);
    RC_CHECK(img.stride, ==, 8u);
    RC_CHECK(img.data.num, ==, 16u);
    for (uint32_t off = 0; off < img.data.num; off += 4) {
        RC_CHECK(rc_span_bytes_get(img.data, off),     ==, (uint8_t)0x11);
        RC_CHECK(rc_span_bytes_get(img.data, off + 1), ==, (uint8_t)0x22);
        RC_CHECK(rc_span_bytes_get(img.data, off + 2), ==, (uint8_t)0x33);
        RC_CHECK(rc_span_bytes_get(img.data, off + 3), ==, (uint8_t)0x44);
    }
}

RC_TEST_STEP(image, make_fill_rgb8, fix)
{
    // alpha byte of the packed fill is ignored for an RGB8 image
    rc_image img = rc_image_make_filled(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_RGB8, 0xFF302010u, &fix->a);
    RC_CHECK(img.stride, ==, 6u);
    RC_CHECK(img.data.num, ==, 12u);
    for (uint32_t off = 0; off < img.data.num; off += 3) {
        RC_CHECK(rc_span_bytes_get(img.data, off),     ==, (uint8_t)0x10);
        RC_CHECK(rc_span_bytes_get(img.data, off + 1), ==, (uint8_t)0x20);
        RC_CHECK(rc_span_bytes_get(img.data, off + 2), ==, (uint8_t)0x30);
    }
}

RC_TEST_STEP(image, make_fill_r8, fix)
{
    // only the low (R) byte of the packed fill is stored
    rc_image img = rc_image_make_filled(rc_vec2i_make(3, 2), RC_PIXEL_FORMAT_R8, 0xAABBCC7Fu, &fix->a);
    RC_CHECK(img.stride, ==, 3u);
    RC_CHECK(img.data.num, ==, 6u);
    for (uint32_t i = 0; i < img.data.num; i++)
        RC_CHECK(rc_span_bytes_get(img.data, i), ==, (uint8_t)0x7F);
}

RC_TEST_STEP(image, make_zero_area, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(0, 0), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    RC_CHECK(img.data.num, ==, 0u);
    RC_CHECK(img.size.x, ==, 0);
    RC_CHECK(img.size.y, ==, 0);
    RC_CHECK(img.format, ==, RC_PIXEL_FORMAT_RGBA8);
}

RC_TEST_STEP(image, get_set_pixel_rgba8, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(3, 3), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image_set_pixel(img, 1, 2, 0x44332211u);     // R=0x11 G=0x22 B=0x33 A=0x44
    RC_CHECK(rc_image_get_pixel(img, 1, 2), ==, 0x44332211u);
    // packing: R is the low byte, A the high byte, in memory order R,G,B,A
    uint32_t idx = 2u * img.stride + 1u * 4u;
    RC_CHECK(rc_span_bytes_get(img.data, idx),     ==, (uint8_t)0x11);
    RC_CHECK(rc_span_bytes_get(img.data, idx + 1), ==, (uint8_t)0x22);
    RC_CHECK(rc_span_bytes_get(img.data, idx + 2), ==, (uint8_t)0x33);
    RC_CHECK(rc_span_bytes_get(img.data, idx + 3), ==, (uint8_t)0x44);
    RC_CHECK(rc_image_get_pixel(img, 0, 0), ==, 0x00000000u);
    // the _rgba8 variant matches the generic dispatch
    RC_CHECK(rc_image_get_pixel_rgba8(img, 1, 2), ==, 0x44332211u);
    rc_image_set_pixel_rgba8(img, 0, 0, 0x88776655u);
    RC_CHECK(rc_image_get_pixel(img, 0, 0), ==, 0x88776655u);
}

RC_TEST_STEP(image, get_set_pixel_rgb8, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_RGB8, &fix->a);
    // alpha is dropped on write and reported as 255 on read
    rc_image_set_pixel(img, 0, 0, 0x99332211u);
    RC_CHECK(rc_image_get_pixel(img, 0, 0), ==, 0xFF332211u);
    // per-format variants agree
    rc_image_set_pixel_rgb8(img, 1, 1, 0x00AABBCCu);
    RC_CHECK(rc_image_get_pixel_rgb8(img, 1, 1), ==, 0xFFAABBCCu);
}

RC_TEST_STEP(image, get_set_pixel_r8, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_R8, &fix->a);
    // only the low byte is stored; read replicates it as grayscale with A=255
    rc_image_set_pixel(img, 1, 1, 0xAABBCC7Fu);
    RC_CHECK(rc_image_get_pixel(img, 1, 1), ==, 0xFF7F7F7Fu);
    RC_CHECK(rc_span_bytes_get(img.data, 1u * img.stride + 1u), ==, (uint8_t)0x7F);
    // per-format variants agree
    rc_image_set_pixel_r8(img, 0, 0, 0x000000A0u);
    RC_CHECK(rc_image_get_pixel_r8(img, 0, 0), ==, 0xFFA0A0A0u);
}

RC_TEST_STEP(image, subimage_aliases_parent, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(4, 4), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image sub = rc_image_make_subimage(img, rc_box2i_make_pos_size(rc_vec2i_make(1, 1),
                                                                      rc_vec2i_make(2, 2)));
    RC_CHECK(sub.size.x, ==, 2);
    RC_CHECK(sub.size.y, ==, 2);
    RC_CHECK(sub.stride, ==, img.stride);   // shares the parent's stride

    // writing through the subimage shows up in the parent at the offset origin
    rc_image_set_pixel(sub, 0, 0, 0x44332211u);
    RC_CHECK(rc_image_get_pixel(img, 1, 1), ==, 0x44332211u);
    // and vice versa
    rc_image_set_pixel(img, 2, 2, 0x88776655u);
    RC_CHECK(rc_image_get_pixel(sub, 1, 1), ==, 0x88776655u);
}

RC_TEST_STEP(image, subimage_clamped, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(4, 4), RC_PIXEL_FORMAT_R8, &fix->a);
    // a region overhanging the right/bottom edges is clamped to the image
    rc_image sub = rc_image_make_subimage(img, rc_box2i_make_pos_size(rc_vec2i_make(2, 2),
                                                                      rc_vec2i_make(10, 10)));
    RC_CHECK(sub.size.x, ==, 2);
    RC_CHECK(sub.size.y, ==, 2);
    RC_CHECK_TRUE(rc_span_bytes_is_valid(sub.data));
}

RC_TEST_STEP(image, subimage_empty, fix)
{
    rc_image img = rc_image_make(rc_vec2i_make(4, 4), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    // fully out of bounds -> invalid image
    rc_image sub = rc_image_make_subimage(img, rc_box2i_make_pos_size(rc_vec2i_make(10, 10),
                                                                      rc_vec2i_make(2, 2)));
    RC_CHECK(sub.size.x, ==, 0);
    RC_CHECK(sub.size.y, ==, 0);
    RC_CHECK(sub.data.num, ==, 0u);
    RC_CHECK_FALSE(rc_span_bytes_is_valid(sub.data));
}

RC_TEST_STEP(image, blit_same_format, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(4, 4), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_RGBA8, 0x44332211u, &fix->a);

    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(1, 1), src));
    RC_CHECK(rc_image_get_pixel(dst, 1, 1), ==, 0x44332211u);
    RC_CHECK(rc_image_get_pixel(dst, 2, 2), ==, 0x44332211u);
    // outside the blit stays zero
    RC_CHECK(rc_image_get_pixel(dst, 0, 0), ==, 0x00000000u);
    RC_CHECK(rc_image_get_pixel(dst, 3, 3), ==, 0x00000000u);
}

RC_TEST_STEP(image, blit_clipped_negative, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(3, 3), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_RGBA8, 0x40302010u, &fix->a);

    // place top-left of src at (-1, -1): only its bottom-right pixel lands at (0,0)
    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(-1, -1), src));
    RC_CHECK(rc_image_get_pixel(dst, 0, 0), ==, 0x40302010u);
    RC_CHECK(rc_image_get_pixel(dst, 1, 1), ==, 0x00000000u);
}

RC_TEST_STEP(image, blit_fully_clipped, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(3, 3), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(2, 2), RC_PIXEL_FORMAT_RGBA8, 0x40302010u, &fix->a);

    // entirely off the right edge: a no-op that still reports success
    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(5, 0), src));
    for (int32_t y = 0; y < 3; y++)
        for (int32_t x = 0; x < 3; x++)
            RC_CHECK(rc_image_get_pixel(dst, x, y), ==, 0x00000000u);
}

RC_TEST_STEP(image, blit_expand_r8_to_rgb8, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(2, 1), RC_PIXEL_FORMAT_RGB8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(1, 1), RC_PIXEL_FORMAT_R8, 0x7Fu, &fix->a);

    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(0, 0), src));
    RC_CHECK(rc_image_get_pixel(dst, 0, 0), ==, 0xFF7F7F7Fu);   // grayscale, opaque
}

RC_TEST_STEP(image, blit_expand_r8_to_rgba8, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(2, 1), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(1, 1), RC_PIXEL_FORMAT_R8, 0x40u, &fix->a);

    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(0, 0), src));
    RC_CHECK(rc_image_get_pixel(dst, 0, 0), ==, 0xFF404040u);
}

RC_TEST_STEP(image, blit_expand_rgb8_to_rgba8, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(2, 1), RC_PIXEL_FORMAT_RGBA8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(1, 1), RC_PIXEL_FORMAT_RGB8, 0x00332211u, &fix->a);

    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(0, 0), src));
    RC_CHECK(rc_image_get_pixel(dst, 0, 0), ==, 0xFF332211u);   // alpha filled to 255
}

RC_TEST_STEP(image, blit_same_format_r8, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(3, 1), RC_PIXEL_FORMAT_R8, &fix->a);
    rc_image src = rc_image_make_filled(rc_vec2i_make(2, 1), RC_PIXEL_FORMAT_R8, 0x5Au, &fix->a);

    RC_CHECK_TRUE(rc_image_blit(dst, rc_vec2i_make(1, 0), src));
    RC_CHECK(rc_image_get_pixel(dst, 0, 0), ==, 0xFF000000u);   // untouched grayscale 0
    RC_CHECK(rc_image_get_pixel(dst, 1, 0), ==, 0xFF5A5A5Au);
    RC_CHECK(rc_image_get_pixel(dst, 2, 0), ==, 0xFF5A5A5Au);
}

RC_TEST_STEP(image, blit_narrowing_fails, fix)
{
    rc_image dst = rc_image_make(rc_vec2i_make(2, 1), RC_PIXEL_FORMAT_RGB8, &fix->a);
    rc_image src = rc_image_make(rc_vec2i_make(1, 1), RC_PIXEL_FORMAT_RGBA8, &fix->a);

    // RGBA8 -> RGB8 would drop alpha; not supported, returns false
    RC_CHECK_FALSE(rc_image_blit(dst, rc_vec2i_make(0, 0), src));
}
