/*
 * font/font_atlas.h - a persistent glyph table plus a transient SDF atlas builder.
 *
 * Two types with different lifetimes:
 *
 *   rc_glyph_table  - the principal, persistent data: codepoint -> rc_glyph (a
 *                     normalized atlas UV rect + layout metrics).  This is what
 *                     you keep to lay out text.
 *   rc_font_atlas   - the transient builder: a font, the R8 SDF atlas pixels, and
 *                     a retained rectangle packer.  You fill it (add / add_range),
 *                     upload its `image` to a GPU texture, then free build_arena;
 *                     the glyph table lives on in table_arena.
 *
 * Layout model
 * ------------
 * A pen sits on the baseline.  For each glyph: draw a quad at pen + offset (offset
 * is the pen-to-quad-top-left vector, in pixels, screen y-down) of pixel size
 * uv.size * atlas_dimensions, textured by uv; then advance the pen by advance.
 * The quad carries the SDF spread padding (the shader thresholds it away); advance
 * is the true font advance, so spacing is correct.  Line pitch uses the font-level
 * ascent / descent / line_gap on rc_font.
 *
 * Arenas
 * ------
 * Two arenas, captured by neither type (passed each call, as rc_image_atlas does):
 *   build_arena - the atlas pixels + packer; freeable once the texture is uploaded
 *                 (its packer free list is its only growable).
 *   table_arena - the glyph table; persists for as long as you render text (its
 *                 sorted tail is its only growable).
 * Renders are transient (done through by-value arena copies), so neither add nor
 * add_range needs a separate scratch arena, and rc_glyph_table_find needs none.
 */

#ifndef RC_FONT_FONT_ATLAS_H_
#define RC_FONT_FONT_ATLAS_H_

#include "richc/font/font.h"
#include "richc/math/box2f.h"
#include "richc/rect_pack.h"

/* ---- glyph entry ---- */

/* Self-contained: uv is a texture-coordinate rect, not a pointer into the pixels,
 * so an entry stays valid after the atlas pixels are freed. */
typedef struct rc_glyph {
    rc_box2f uv;        /* normalized atlas rect [0,1]; zero-size for whitespace   */
    rc_vec2f offset;    /* pen -> quad top-left, pixels, screen y-down             */
    float    advance;   /* horizontal pen advance, pixels                          */
    bool     placed;    /* false if it did not fit (advance still valid); false    */
                        /* for a codepoint absent from rc_glyph_table_find         */
} rc_glyph;

/* ---- glyph table (persistent) ---- */

/* Printable-ASCII fast block: codepoints [first, first + count) are direct-indexed. */
enum {
    RC_GLYPH_ASCII_FIRST = 0x20,
    RC_GLYPH_ASCII_LAST  = 0x7E,
    RC_GLYPH_ASCII_COUNT = RC_GLYPH_ASCII_LAST - RC_GLYPH_ASCII_FIRST + 1,   /* 95 */
};

typedef struct rc_glyph_kv {
    uint32_t codepoint;
    rc_glyph glyph;
} rc_glyph_kv;

#define RC_ARRAY_TYPE rc_glyph_kv
#define RC_ARRAY_NAME glyph_kv
#include "richc/template/array.h"

/*
 * Codepoint -> rc_glyph.  The printable-ASCII block is direct-indexed
 * (cp - RC_GLYPH_ASCII_FIRST); every other codepoint lives in `tail`, kept sorted
 * by codepoint for binary-search lookup.  All storage is in table_arena; `tail`
 * is that arena's only growable.
 */
typedef struct rc_glyph_table {
    rc_glyph          *ascii;            /* [RC_GLYPH_ASCII_COUNT] */
    uint32_t           ascii_present[(RC_GLYPH_ASCII_COUNT + 31) / 32];
    rc_array_glyph_kv  tail;
} rc_glyph_table;

/* Create an empty table.  Allocates the ASCII block from table_arena. */
rc_glyph_table rc_glyph_table_make(rc_arena *table_arena);

/*
 * Look up a codepoint.  Touches only the table (no font, no arenas, never
 * renders), so this is the call to use after build_arena has been freed.  Returns
 * the stored entry, or { .placed = false } if the codepoint was never added.
 */
rc_glyph rc_glyph_table_find(const rc_glyph_table *table, uint32_t codepoint);

/* ---- atlas builder (transient) ---- */

/*
 * The SDF atlas under construction.  Read `image` to upload the texture; its
 * `image.size` is a value field that survives freeing build_arena, for UV scaling.
 */
typedef struct rc_font_atlas {
    rc_font      font;     /* by value; borrows the ttf bytes (must outlive the atlas) */
    rc_image     image;    /* R8 atlas pixels, in build_arena                          */
    rc_rect_pack packer;   /* retained packer, in build_arena                          */
    int32_t      spacing;  /* gap, in pixels, kept between glyphs                       */
} rc_font_atlas;

/* Create a cleared size-by-size R8 atlas with `spacing` px between glyphs. */
rc_font_atlas rc_font_atlas_make(rc_font font, rc_vec2i size, int32_t spacing,
                                 rc_arena *build_arena);

/*
 * Rasterise `codepoint` as an SDF, pack it into the atlas, blit it in, and record
 * its entry in `table`.  Idempotent: an already-present codepoint is returned from
 * the table without re-rendering.  Whitespace yields a zero-size uv with a valid
 * advance; an overflow yields placed = false (advance still valid).  Renders
 * transiently (no lasting build_arena growth beyond the packer).
 */
rc_glyph rc_font_atlas_add(rc_font_atlas *atlas, rc_glyph_table *table, uint32_t codepoint,
                           rc_arena *build_arena, rc_arena *table_arena);

/*
 * Pre-load the contiguous codepoint range [first, last] (e.g. 0x20..0x7E ASCII,
 * 0x20..0xFF Latin-1), packed densest-first (sorted by glyph size, matching
 * rc_rect_pack_all).  Records every entry in `table`.  Returns the count that did
 * not fit.
 */
uint32_t rc_font_atlas_add_range(rc_font_atlas *atlas, rc_glyph_table *table,
                                 uint32_t first, uint32_t last,
                                 rc_arena *build_arena, rc_arena *table_arena);

#endif /* RC_FONT_FONT_ATLAS_H_ */
