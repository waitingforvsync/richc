#include "richc/file.h"
#include "richc/image/image_png.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(image_png) {
    rc_arena a;         // output (loaded file bytes + decoded pixels)
    rc_arena scratch;   // transient decode buffers
};

RC_TEST_GROUP_INIT(image_png, fix)
{
    fix->a = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(image_png, fix)
{
    rc_arena_deinit(&fix->a);
    rc_arena_deinit(&fix->scratch);
}

static rc_view_bytes load(rc_arena *a, rc_str path)
{
    rc_file_load_binary_result r = rc_file_load_binary(path, 0, a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    return r.contents.view;
}

static rc_image_png_error decode_err(rc_arena *a, rc_arena scratch, const uint8_t *bytes, uint32_t n)
{
    rc_image_png_result r = rc_image_from_png(rc_view_bytes_make(bytes, n),
                                              RC_PIXEL_FORMAT_NONE, a, scratch);
    return r.error;
}

RC_TEST_STEP(image_png, gray8, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/gray8.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.size.x, ==, 2);
    RC_CHECK(r.image.size.y, ==, 2);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_R8);
    // grayscale reads back as opaque, replicated across R/G/B
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, 0xFF000000u);
    RC_CHECK(rc_image_get_pixel(r.image, 1, 0), ==, 0xFF404040u);
    RC_CHECK(rc_image_get_pixel(r.image, 0, 1), ==, 0xFF808080u);
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, 0xFFFFFFFFu);
}

RC_TEST_STEP(image_png, gray8_widen_to_rgba8, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/gray8.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_RGBA8, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
    RC_CHECK(rc_image_get_pixel(r.image, 1, 0), ==, 0xFF404040u);   // grayscale, opaque
}

RC_TEST_STEP(image_png, rgb8, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/rgb8.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGB8);
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (10u | (20u << 8) | (30u << 16) | 0xFF000000u));
    RC_CHECK(rc_image_get_pixel(r.image, 1, 0), ==, (40u | (50u << 8) | (60u << 16) | 0xFF000000u));
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, (100u | (110u << 8) | (120u << 16) | 0xFF000000u));
}

RC_TEST_STEP(image_png, rgb8_widen_to_rgba8, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/rgb8.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_RGBA8, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (10u | (20u << 8) | (30u << 16) | 0xFF000000u));
}

RC_TEST_STEP(image_png, rgba8, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/rgba8.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (11u | (22u << 8) | (33u << 16) | (44u << 24)));
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, (143u | (154u << 8) | (165u << 16) | (176u << 24)));
}

RC_TEST_STEP(image_png, rgba8_hint_does_not_narrow, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/rgba8.png"));
    // an R8 hint must not narrow an RGBA8 source
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_R8, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);
}

RC_TEST_STEP(image_png, graya8, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/graya8.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);   // no 2-channel format
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (0x10u | (0x10u << 8) | (0x10u << 16) | (0xFFu << 24)));
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, (0x40u | (0x40u << 8) | (0x40u << 16) | (0x00u << 24)));
}

RC_TEST_STEP(image_png, palette_with_trns, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/palette.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGBA8);   // tRNS -> alpha present
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (255u | (0u << 8) | (0u << 16) | (0x80u << 24)));   // idx0
    RC_CHECK(rc_image_get_pixel(r.image, 1, 0), ==, (0u | (255u << 8) | (0u << 16) | (0x40u << 24)));   // idx1
    RC_CHECK(rc_image_get_pixel(r.image, 0, 1), ==, (0u | (0u << 8) | (255u << 16) | (255u << 24)));    // idx2 opaque
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, (255u | (255u << 8) | (0u << 16) | (255u << 24)));  // idx3 opaque
}

RC_TEST_STEP(image_png, palette_opaque, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/palette_opaque.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGB8);   // no tRNS -> RGB
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (255u | (0u << 8) | (0u << 16) | 0xFF000000u));
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, (255u | (255u << 8) | (0u << 16) | 0xFF000000u));
}

RC_TEST_STEP(image_png, gray1, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/gray1.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_R8);
    // 1-bit samples scale 0 -> 0, 1 -> 255
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, 0xFFFFFFFFu);
    RC_CHECK(rc_image_get_pixel(r.image, 1, 0), ==, 0xFF000000u);
    RC_CHECK(rc_image_get_pixel(r.image, 0, 1), ==, 0xFF000000u);
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, 0xFFFFFFFFu);
}

RC_TEST_STEP(image_png, palette_4bit, fix)
{
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/pal4.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGB8);
    RC_CHECK(rc_image_get_pixel(r.image, 0, 0), ==, (255u | (0u << 8) | (0u << 16) | 0xFF000000u));   // idx0
    RC_CHECK(rc_image_get_pixel(r.image, 1, 0), ==, (0u | (255u << 8) | (0u << 16) | 0xFF000000u));   // idx1
    RC_CHECK(rc_image_get_pixel(r.image, 0, 1), ==, (0u | (0u << 8) | (255u << 16) | 0xFF000000u));   // idx2
    RC_CHECK(rc_image_get_pixel(r.image, 1, 1), ==, (255u | (255u << 8) | (0u << 16) | 0xFF000000u)); // idx3
}

RC_TEST_STEP(image_png, filtered_all_filter_types, fix)
{
    // 4x5 RGB image; the generator applied filters None/Sub/Up/Average/Paeth to
    // rows 0..4, so decoding must reverse each to recover the formula's pixels.
    rc_view_bytes png = load(&fix->a, RC_STR("data/png/filtered.png"));
    rc_image_png_result r = rc_image_from_png(png, RC_PIXEL_FORMAT_NONE, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_IMAGE_PNG_OK);
    RC_CHECK(r.image.format, ==, RC_PIXEL_FORMAT_RGB8);
    RC_CHECK(r.image.size.x, ==, 4);
    RC_CHECK(r.image.size.y, ==, 5);
    for (int32_t y = 0; y < 5; y++) {
        for (int32_t x = 0; x < 4; x++) {
            uint32_t rr = (uint32_t)(x * 37 + y * 11) & 0xFFu;
            uint32_t gg = (uint32_t)(x * 17 + y * 29) & 0xFFu;
            uint32_t bb = (uint32_t)(x * 53 + y * 7) & 0xFFu;
            uint32_t expect = rr | (gg << 8) | (bb << 16) | 0xFF000000u;
            RC_CHECK(rc_image_get_pixel(r.image, x, y), ==, expect);
        }
    }
}

RC_TEST_STEP(image_png, error_not_png, fix)
{
    uint8_t junk[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, junk, 8) == RC_IMAGE_PNG_ERROR_NOT_PNG);
    uint8_t tiny[3] = {0x89, 'P', 'N'};
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, tiny, 3) == RC_IMAGE_PNG_ERROR_NOT_PNG);
}

RC_TEST_STEP(image_png, error_truncated, fix)
{
    // valid signature, then only 4 bytes - not even a full chunk length+type
    uint8_t t[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13};
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, t, (uint32_t)sizeof(t)) == RC_IMAGE_PNG_ERROR_TRUNCATED);
}

RC_TEST_STEP(image_png, error_unsupported_16bit, fix)
{
    uint8_t p[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 0, 1,  0, 0, 0, 1,    // 1x1
        16, 2, 0, 0, 0,             // 16-bit, truecolour
        0, 0, 0, 0                  // dummy CRC (not validated)
    };
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, p, (uint32_t)sizeof(p)) == RC_IMAGE_PNG_ERROR_UNSUPPORTED);
}

RC_TEST_STEP(image_png, error_unsupported_interlace, fix)
{
    uint8_t p[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 0, 1,  0, 0, 0, 1,
        8, 2, 0, 0, 1,              // 8-bit RGB, Adam7 interlace
        0, 0, 0, 0
    };
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, p, (uint32_t)sizeof(p)) == RC_IMAGE_PNG_ERROR_UNSUPPORTED);
}

RC_TEST_STEP(image_png, error_bad_header_color_type, fix)
{
    uint8_t p[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 0, 1,  0, 0, 0, 1,
        8, 5, 0, 0, 0,              // colour type 5 is invalid
        0, 0, 0, 0
    };
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, p, (uint32_t)sizeof(p)) == RC_IMAGE_PNG_ERROR_BAD_HEADER);
}

RC_TEST_STEP(image_png, error_trns_before_plte, fix)
{
    // palette image with tRNS appearing before PLTE - illegal chunk ordering
    uint8_t p[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 0, 1,  0, 0, 0, 1,
        8, 3, 0, 0, 0,             // 1x1 palette 8-bit
        0, 0, 0, 0,
        0, 0, 0, 1, 't', 'R', 'N', 'S',
        0x80,                      // alpha for entry 0 - but no PLTE yet
        0, 0, 0, 0
    };
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, p, (uint32_t)sizeof(p)) == RC_IMAGE_PNG_ERROR_BAD_DATA);
}

RC_TEST_STEP(image_png, error_bad_idat, fix)
{
    uint8_t p[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 0, 1,  0, 0, 0, 1,
        8, 0, 0, 0, 0,             // 1x1 grayscale 8-bit
        0, 0, 0, 0,
        0, 0, 0, 4, 'I', 'D', 'A', 'T',
        0xDE, 0xAD, 0xBE, 0xEF,    // not a valid zlib stream
        0, 0, 0, 0,
        0, 0, 0, 0, 'I', 'E', 'N', 'D',
        0, 0, 0, 0
    };
    RC_CHECK_TRUE(decode_err(&fix->a, fix->scratch, p, (uint32_t)sizeof(p)) == RC_IMAGE_PNG_ERROR_BAD_DATA);
}
