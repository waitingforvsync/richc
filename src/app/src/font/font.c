#include "richc/font/font.h"

#include <math.h>

#include "richc/arena.h"

#include "font_sdf.h"
#include "font_ttf.h"

// SDF spread (and bitmap padding), in pixels, derived from the rasterisation
// size: ~1/8 em, floored so tiny sizes keep a usable field and capped so the
// byte resolution near the edge (128/spread levels per texel) stays good.
static float derive_spread(float pixel_size)
{
    float s = roundf(pixel_size * 0.125f);
    if (s < 2.0f) s = 2.0f;
    if (s > 8.0f) s = 8.0f;
    return s;
}

// Rebuild the internal parse view from the public font (font_ttf is private to
// the .c set, so it is not embedded in rc_font).
static font_ttf as_ttf(const rc_font *f)
{
    return (font_ttf) {
        .ttf          = f->ttf_,
        .cmap         = f->cmap_,
        .loca         = f->loca_,
        .glyf         = f->glyf_,
        .hmtx         = f->hmtx_,
        .cmap_format  = f->cmap_format_,
        .num_glyphs   = f->num_glyphs_,
        .num_hmetrics = f->num_hmetrics_,
        .loca_long    = f->loca_long_,
        .units_per_em = f->units_per_em_,
    };
}

rc_font_result rc_font_make(rc_view_bytes ttf, float pixel_size, rc_arena *arena)
{
    (void)arena;   // make borrows ttf and stores only views; nothing is allocated
    RC_ASSERT(pixel_size > 0.0f);

    font_ttf t;
    rc_font_error e = font_ttf_parse(ttf, &t);
    if (e != RC_FONT_OK)
        return (rc_font_result) {.error = e};

    float scale = pixel_size / (float)t.units_per_em;
    rc_font f = {
        .ascent       = (float)t.ascent   * scale,
        .descent      = (float)t.descent  * scale,
        .line_gap     = (float)t.line_gap * scale,
        .ttf_         = ttf,
        .cmap_        = t.cmap,
        .loca_        = t.loca,
        .glyf_        = t.glyf,
        .hmtx_        = t.hmtx,
        .cmap_format_ = t.cmap_format,
        .num_glyphs_  = t.num_glyphs,
        .num_hmetrics_= t.num_hmetrics,
        .loca_long_   = t.loca_long,
        .units_per_em_= t.units_per_em,
        .scale_       = scale,
        .spread       = derive_spread(pixel_size),
    };
    return (rc_font_result) {.font = f};
}

rc_font_glyph_result rc_font_get_glyph(rc_font *font, uint32_t codepoint,
                                       rc_arena *arena, rc_arena scratch)
{
    RC_ASSERT(font && arena && arena->base != scratch.base);

    font_ttf t   = as_ttf(font);
    uint32_t gid = font_ttf_glyph_index(&t, codepoint);

    rc_font_glyph glyph = {
        .codepoint = codepoint,
        .index     = gid,
        .advance   = (float)font_ttf_advance_units(&t, gid) * font->scale_,
    };

    font_outline_result olr = font_ttf_outline(&t, gid, font->scale_, &scratch);
    if (olr.error != RC_FONT_OK)
        return (rc_font_glyph_result) {.error = olr.error};
    font_outline ol = olr.outline;

    // whitespace / composite / degenerate bbox -> empty image, advance only
    if (ol.composite || ol.edges.num == 0)
        return (rc_font_glyph_result) {.glyph = glyph};

    // bbox corners are sorted (min <= max) by rc_box2f_make
    rc_vec2f bmin = rc_box2f_min(ol.bbox);
    rc_vec2f bmax = rc_box2f_max(ol.bbox);

    float spread = font->spread;
    float pad    = ceilf(spread);
    float ox     = floorf(bmin.x) - pad;    // image left, pixel space (x right)
    float oy_top = ceilf(bmax.y) + pad;     // image top, font-up pixel space (y up)
    int32_t w = (int32_t)(ceilf(bmax.x) - floorf(bmin.x)) + 2 * (int32_t)pad;
    int32_t h = (int32_t)(ceilf(bmax.y) - floorf(bmin.y)) + 2 * (int32_t)pad;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    // origin is the centre of texel (0, 0); rows run downward over a y-up outline
    rc_vec2f origin = {ox + 0.5f, oy_top - 0.5f};
    glyph.image  = font_sdf_render(ol.edges, rc_vec2i_make(w, h), origin, spread, arena, scratch);
    glyph.offset = (rc_vec2f) {ox, -oy_top};

    return (rc_font_glyph_result) {.glyph = glyph};
}
