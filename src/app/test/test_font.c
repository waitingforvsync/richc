#include "richc/file.h"
#include "richc/font/font.h"
#include "richc/image/image_atlas.h"
#include "richc/test.h"

#include <math.h>

RC_TEST_GROUP_DATA(font) {
    rc_arena a;         // output (loaded file bytes + glyph images)
    rc_arena scratch;   // transient outline / sdf buffers
};

RC_TEST_GROUP_INIT(font, fix)
{
    fix->a = rc_arena_make_default();
    fix->scratch = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(font, fix)
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

static rc_font make_roboto(rc_arena *a, float pixel_size)
{
    rc_view_bytes ttf = load(a, RC_STR("data/fonts/Roboto-Regular.ttf"));
    rc_font_result r = rc_font_make(ttf, pixel_size, a);
    RC_CHECK_TRUE(r.error == RC_FONT_OK);
    return r.font;
}

// Raw R8 byte at (x, y).
static uint32_t r8(rc_image img, int32_t x, int32_t y)
{
    return rc_image_get_pixel(img, x, y) & 0xFFu;
}

RC_TEST_STEP(font, parse_ok_and_metrics, fix)
{
    rc_font f = make_roboto(&fix->a, 48.0f);
    RC_CHECK_TRUE(f.ascent > 0.0f);
    RC_CHECK_TRUE(f.descent < 0.0f);
    RC_CHECK_TRUE(f.ascent - f.descent > 0.0f);
}

RC_TEST_STEP(font, reject_non_ttf, fix)
{
    uint8_t junk[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    rc_font_result r = rc_font_make(rc_view_bytes_make(junk, sizeof junk), 48.0f, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FONT_ERROR_NOT_TTF);

    rc_font_result t = rc_font_make(rc_view_bytes_make(junk, 4), 48.0f, &fix->a);
    RC_CHECK_TRUE(t.error == RC_FONT_ERROR_TRUNCATED);
}

RC_TEST_STEP(font, glyph_H_is_sdf, fix)
{
    rc_font f = make_roboto(&fix->a, 48.0f);
    rc_font_glyph_result r = rc_font_get_glyph(&f, 'H', &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_FONT_OK);

    rc_font_glyph g = r.glyph;
    RC_CHECK_TRUE(g.index != 0);            // resolved through cmap
    RC_CHECK_TRUE(g.advance > 0.0f);
    RC_CHECK(g.image.format, ==, RC_PIXEL_FORMAT_R8);
    RC_CHECK_TRUE(g.image.size.x > 12);     // glyph + 2*pad (spread 6 at size 48)
    RC_CHECK_TRUE(g.image.size.y > 12);

    // the top-left corner sits in the padding margin -> far outside -> ~0
    RC_CHECK_TRUE(r8(g.image, 0, 0) < 128u);

    // the field must span the edge: some inside (>128), some outside (<128),
    // and saturate to 0 somewhere in the margin.
    uint32_t lo = 255, hi = 0;
    for (int32_t y = 0; y < g.image.size.y; ++y) {
        for (int32_t x = 0; x < g.image.size.x; ++x) {
            uint32_t v = r8(g.image, x, y);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    RC_CHECK_TRUE(hi > 128u);                // interior present
    RC_CHECK_TRUE(lo == 0u);                 // saturated-outside present
}

RC_TEST_STEP(font, advance_is_unrounded, fix)
{
    // advance must be integer-font-units * scale, never snapped to whole pixels:
    // advance / scale recovers the integer font-unit advance width.
    rc_font f = make_roboto(&fix->a, 37.0f);
    rc_font_glyph_result r = rc_font_get_glyph(&f, 'H', &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_FONT_OK);
    float units = r.glyph.advance / f.scale_;
    RC_CHECK_TRUE(fabsf(units - roundf(units)) < 0.05f);
}

RC_TEST_STEP(font, space_is_blank_with_advance, fix)
{
    rc_font f = make_roboto(&fix->a, 48.0f);
    rc_font_glyph_result r = rc_font_get_glyph(&f, ' ', &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_FONT_OK);
    RC_CHECK(r.glyph.image.size.x, ==, 0);   // empty image
    RC_CHECK(r.glyph.image.size.y, ==, 0);
    RC_CHECK_TRUE(r.glyph.advance > 0.0f);    // but a real advance
    RC_CHECK_TRUE(r.glyph.index != 0);
}

RC_TEST_STEP(font, missing_codepoint_is_notdef, fix)
{
    rc_font f = make_roboto(&fix->a, 48.0f);
    // a codepoint Roboto does not cover (private use area) -> .notdef
    rc_font_glyph_result r = rc_font_get_glyph(&f, 0xE000u, &fix->a, fix->scratch);
    RC_CHECK_TRUE(r.error == RC_FONT_OK);
    RC_CHECK(r.glyph.index, ==, 0u);
}

RC_TEST_STEP(font, glyphs_pack_into_atlas, fix)
{
    rc_font f = make_roboto(&fix->a, 32.0f);
    rc_image_atlas atlas = rc_image_atlas_make(rc_vec2i_make(256, 256),
                                               RC_PIXEL_FORMAT_R8, 1, &fix->a);
    for (uint32_t cp = 'A'; cp <= 'Z'; ++cp) {
        rc_font_glyph_result r = rc_font_get_glyph(&f, cp, &fix->a, fix->scratch);
        RC_CHECK_TRUE(r.error == RC_FONT_OK);
        if (r.glyph.image.size.x == 0)
            continue;
        rc_rect_pack_result p = rc_image_atlas_add(&atlas, r.glyph.image, &fix->a);
        RC_CHECK_TRUE(p.placed);
    }
}

/*
 * Regression: winding when a quad endpoint or an on-curve extremum lies
 * exactly on a sample row.  The old root-interval crossing convention could
 * double-count there, painting a one-texel-tall "inside" streak across the
 * row (Roboto's 'r' at pixel_size 48 put its arch apex exactly on a sample
 * row and streaked 14 texels from the bitmap's left edge).  Detect the streak
 * shape: a horizontal RUN of inside texels with no inside vertical neighbour.
 * Genuine sharp corners (diagonal stroke tips of V, X, w, backslash) produce
 * single such texels - at this size the longest legitimate run is 1 - so only
 * runs of 3 or more fail.
 */
RC_TEST_STEP(font, no_single_row_winding_streaks, fix)
{
    rc_font f = make_roboto(&fix->a, 48.0f);
    for (uint32_t cp = 0x20; cp <= 0x7E; cp += 1) {
        rc_font_glyph_result r = rc_font_get_glyph(&f, cp, &fix->a, fix->scratch);
        RC_CHECK_TRUE(r.error == RC_FONT_OK);
        rc_image img = r.glyph.image;
        uint32_t max_run = 0;
        for (int32_t y = 0; y < img.size.y; y += 1) {
            uint32_t run = 0;
            for (int32_t x = 0; x < img.size.x; x += 1) {
                bool inside = r8(img, x, y) >= 128;
                bool above = y > 0 && r8(img, x, y - 1) >= 128;
                bool below = y + 1 < img.size.y && r8(img, x, y + 1) >= 128;
                run = (inside && !above && !below) ? run + 1 : 0;
                max_run = run > max_run ? run : max_run;
            }
        }
        RC_CHECK(max_run, <, 3u);
    }
}
