#include "richc/file.h"
#include "richc/font/font_atlas.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(font_atlas) {
    rc_arena font_arena;    // ttf bytes + font (persist for the font's lifetime)
    rc_arena build_arena;   // atlas pixels + packer (freeable after upload)
    rc_arena table_arena;   // the glyph table (persistent)
};

RC_TEST_GROUP_INIT(font_atlas, fix)
{
    fix->font_arena = rc_arena_make_default();
    fix->build_arena = rc_arena_make_default();
    fix->table_arena = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(font_atlas, fix)
{
    rc_arena_deinit(&fix->font_arena);
    rc_arena_deinit(&fix->build_arena);
    rc_arena_deinit(&fix->table_arena);
}

static rc_font load_roboto(rc_arena *a, float pixel_size)
{
    rc_file_load_binary_result r = rc_file_load_binary(RC_STR("data/fonts/Roboto-Regular.ttf"), 0, a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    rc_font_result f = rc_font_make(r.contents.view, pixel_size, a);
    RC_CHECK_TRUE(f.error == RC_FONT_OK);
    return f.font;
}

static bool uv_nonempty(rc_box2f uv)
{
    rc_vec2f s = rc_box2f_size(uv);
    return s.x > 0.0f && s.y > 0.0f;
}

static bool uv_in_unit(rc_box2f uv)
{
    rc_vec2f mn = rc_box2f_min(uv);
    rc_vec2f mx = rc_box2f_max(uv);
    return mn.x >= 0.0f && mn.y >= 0.0f && mx.x <= 1.0001f && mx.y <= 1.0001f;
}

RC_TEST_STEP(font_atlas, add_range_ascii, fix)
{
    rc_font font = load_roboto(&fix->font_arena, 24.0f);
    rc_font_atlas atlas = rc_font_atlas_make(font, rc_vec2i_make(256, 256), 1, &fix->build_arena);
    rc_glyph_table table = rc_glyph_table_make(&fix->table_arena);

    uint32_t missed = rc_font_atlas_add_range(&atlas, &table, 0x20, 0x7E,
                                              &fix->build_arena, &fix->table_arena);
    RC_CHECK(missed, ==, 0u);
    RC_CHECK(atlas.image.format, ==, RC_PIXEL_FORMAT_R8);
    RC_CHECK(atlas.image.size.x, ==, 256);

    rc_glyph a = rc_glyph_table_find(&table, 'A');
    RC_CHECK_TRUE(a.placed);
    RC_CHECK_TRUE(a.advance > 0.0f);
    RC_CHECK_TRUE(uv_nonempty(a.uv));
    RC_CHECK_TRUE(uv_in_unit(a.uv));
}

RC_TEST_STEP(font_atlas, sorted_tail_and_idempotent, fix)
{
    rc_font font = load_roboto(&fix->font_arena, 24.0f);
    rc_font_atlas atlas = rc_font_atlas_make(font, rc_vec2i_make(256, 256), 1, &fix->build_arena);
    rc_glyph_table table = rc_glyph_table_make(&fix->table_arena);

    // a non-ASCII codepoint exercises the sorted tail (Latin-1 pound sign, a
    // simple glyph - accented letters like 'e-acute' are composites and not yet
    // rendered)
    rc_glyph first = rc_font_atlas_add(&atlas, &table, 0x00A3, &fix->build_arena, &fix->table_arena);
    RC_CHECK_TRUE(first.placed);
    RC_CHECK_TRUE(uv_nonempty(first.uv));

    rc_glyph found = rc_glyph_table_find(&table, 0x00A3);
    RC_CHECK_TRUE(rc_box2f_is_equal(found.uv, first.uv));

    // adding again is idempotent: same placement, no second slot
    rc_glyph again = rc_font_atlas_add(&atlas, &table, 0x00A3, &fix->build_arena, &fix->table_arena);
    RC_CHECK_TRUE(rc_box2f_is_equal(again.uv, first.uv));
    RC_CHECK(table.tail.num, ==, 1u);
}

RC_TEST_STEP(font_atlas, whitespace_blank, fix)
{
    rc_font font = load_roboto(&fix->font_arena, 24.0f);
    rc_font_atlas atlas = rc_font_atlas_make(font, rc_vec2i_make(128, 128), 1, &fix->build_arena);
    rc_glyph_table table = rc_glyph_table_make(&fix->table_arena);

    rc_glyph sp = rc_font_atlas_add(&atlas, &table, ' ', &fix->build_arena, &fix->table_arena);
    RC_CHECK_TRUE(sp.placed);
    RC_CHECK_TRUE(sp.advance > 0.0f);
    RC_CHECK_FALSE(uv_nonempty(sp.uv));     // no pixels
}

RC_TEST_STEP(font_atlas, overflow_flags_unplaced, fix)
{
    rc_font font = load_roboto(&fix->font_arena, 24.0f);
    rc_font_atlas atlas = rc_font_atlas_make(font, rc_vec2i_make(16, 16), 1, &fix->build_arena);
    rc_glyph_table table = rc_glyph_table_make(&fix->table_arena);

    uint32_t missed = rc_font_atlas_add_range(&atlas, &table, 0x20, 0x7E,
                                              &fix->build_arena, &fix->table_arena);
    RC_CHECK_TRUE(missed > 0u);

    // 'W' cannot fit a 16x16 atlas at 24px, but its advance is still valid
    rc_glyph w = rc_glyph_table_find(&table, 'W');
    RC_CHECK_FALSE(w.placed);
    RC_CHECK_TRUE(w.advance > 0.0f);
}

RC_TEST_STEP(font_atlas, metrics_survive_freeing_pixels, fix)
{
    rc_font font = load_roboto(&fix->font_arena, 24.0f);
    rc_font_atlas atlas = rc_font_atlas_make(font, rc_vec2i_make(256, 256), 1, &fix->build_arena);
    rc_glyph_table table = rc_glyph_table_make(&fix->table_arena);

    rc_font_atlas_add_range(&atlas, &table, 0x20, 0x7E, &fix->build_arena, &fix->table_arena);
    rc_glyph before = rc_glyph_table_find(&table, 'A');
    rc_vec2i atlas_size = atlas.image.size;

    // upload would happen here; then drop the CPU pixels + packer
    rc_arena_reset(&fix->build_arena);

    rc_glyph after = rc_glyph_table_find(&table, 'A');
    RC_CHECK_TRUE(rc_box2f_is_equal(after.uv, before.uv));
    RC_CHECK_TRUE(rc_vec2f_is_equal(after.offset, before.offset));
    RC_CHECK(after.advance, ==, before.advance);
    RC_CHECK(atlas_size.x, ==, 256);        // size value survives the reset

    // a codepoint never added (CJK, absent from Roboto) reports not-placed
    rc_glyph missing = rc_glyph_table_find(&table, 0x4E2D);
    RC_CHECK_FALSE(missing.placed);
}
