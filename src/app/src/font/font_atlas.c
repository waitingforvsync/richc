#include "richc/font/font_atlas.h"

#include "richc/arena.h"

// Binary search over the sorted tail, keyed by codepoint.
#define RC_LOWER_BOUND_TYPE      rc_glyph_kv
#define RC_LOWER_BOUND_VIEW      rc_view_glyph_kv
#define RC_LOWER_BOUND_NAME      lower_bound_glyph_kv
#define RC_LOWER_BOUND_CMP(a, b) ((a).codepoint < (b).codepoint)
#include "richc/template/algorithm/lower_bound.h"

// A glyph rendered during a batch, awaiting placement.
typedef struct atlas_item {
    uint32_t codepoint;
    rc_image image;     // SDF pixels (in build_arena); empty for whitespace
    rc_vec2f offset;
    float    advance;
} atlas_item;

#define RC_ARRAY_TYPE atlas_item
#define RC_ARRAY_NAME atlas_item
#include "richc/template/array.h"

static int32_t item_max_side(atlas_item it)
{
    return it.image.size.x > it.image.size.y ? it.image.size.x : it.image.size.y;
}

// Pack larger glyphs first (rc_rect_pack_all's order), so CMP a-before-b means a
// has the larger max side.
#define RC_SORT_TYPE      atlas_item
#define RC_SORT_NAME      sort_atlas_items
#define RC_SORT_CMP(a, b) (item_max_side(a) > item_max_side(b))
#include "richc/template/algorithm/sort.h"

// ---- ASCII presence bitset ----

static bool ascii_is_present(const rc_glyph_table *t, uint32_t i)
{
    return (t->ascii_present[i >> 5] >> (i & 31u)) & 1u;
}

static void ascii_set_present(rc_glyph_table *t, uint32_t i)
{
    t->ascii_present[i >> 5] |= (1u << (i & 31u));
}

static bool is_ascii(uint32_t cp)
{
    return cp >= RC_GLYPH_ASCII_FIRST && cp <= RC_GLYPH_ASCII_LAST;
}

// ---- table ----

rc_glyph_table rc_glyph_table_make(rc_arena *table_arena)
{
    return (rc_glyph_table) {
        .ascii = rc_arena_alloc_zero_type(table_arena, rc_glyph, RC_GLYPH_ASCII_COUNT),
        .tail  = rc_array_glyph_kv_make(0, table_arena),
    };
}

// Mutable lookup backing add's dedup; NULL if absent.
static rc_glyph *table_find_ptr(rc_glyph_table *t, uint32_t cp)
{
    if (is_ascii(cp)) {
        uint32_t i = cp - RC_GLYPH_ASCII_FIRST;
        return ascii_is_present(t, i) ? &t->ascii[i] : NULL;
    }
    uint32_t idx = lower_bound_glyph_kv(t->tail.view, (rc_glyph_kv) {.codepoint = cp});
    if (idx < t->tail.num && rc_array_glyph_kv_get(&t->tail, idx).codepoint == cp)
        return &rc_array_glyph_kv_at(&t->tail, idx)->glyph;
    return NULL;
}

static void table_set(rc_glyph_table *t, uint32_t cp, rc_glyph g, rc_arena *table_arena)
{
    if (is_ascii(cp)) {
        uint32_t i = cp - RC_GLYPH_ASCII_FIRST;
        t->ascii[i] = g;
        ascii_set_present(t, i);
        return;
    }
    uint32_t idx = lower_bound_glyph_kv(t->tail.view, (rc_glyph_kv) {.codepoint = cp});
    if (idx < t->tail.num && rc_array_glyph_kv_get(&t->tail, idx).codepoint == cp) {
        rc_array_glyph_kv_at(&t->tail, idx)->glyph = g;
        return;
    }
    rc_array_glyph_kv_insert(&t->tail, idx, (rc_glyph_kv) {.codepoint = cp, .glyph = g}, table_arena);
}

rc_glyph rc_glyph_table_find(const rc_glyph_table *t, uint32_t cp)
{
    if (is_ascii(cp)) {
        uint32_t i = cp - RC_GLYPH_ASCII_FIRST;
        return ascii_is_present(t, i) ? t->ascii[i] : (rc_glyph) {0};
    }
    uint32_t idx = lower_bound_glyph_kv(t->tail.view, (rc_glyph_kv) {.codepoint = cp});
    if (idx < t->tail.num) {
        rc_glyph_kv kv = rc_array_glyph_kv_get(&t->tail, idx);
        if (kv.codepoint == cp)
            return kv.glyph;
    }
    return (rc_glyph) {0};
}

// ---- atlas ----

rc_font_atlas rc_font_atlas_make(rc_font font, rc_vec2i size, int32_t spacing,
                                 rc_arena *build_arena)
{
    return (rc_font_atlas) {
        .font    = font,
        .image   = rc_image_make(size, RC_PIXEL_FORMAT_R8, build_arena),
        .packer  = rc_rect_pack_make(size, spacing, build_arena),
        .spacing = spacing,
    };
}

static rc_box2f normalize_uv(rc_vec2i pos, rc_vec2i sz, rc_vec2i atlas)
{
    float ax = (float)atlas.x;
    float ay = (float)atlas.y;
    rc_vec2f mn = {(float)pos.x / ax, (float)pos.y / ay};
    rc_vec2f mx = {(float)(pos.x + sz.x) / ax, (float)(pos.y + sz.y) / ay};
    return rc_box2f_make(mn, mx);
}

// Pack `image` into the atlas and blit it; build the glyph entry.  An empty image
// is whitespace (no slot); a failed pack is placed = false.  Does not touch the table.
static rc_glyph place(rc_font_atlas *atlas, rc_image image, rc_vec2f offset, float advance,
                      rc_arena *build_arena)
{
    rc_glyph g = {.offset = offset, .advance = advance};
    if (image.size.x == 0 || image.size.y == 0) {
        g.placed = true;                            // whitespace: valid, no pixels
        return g;
    }
    rc_rect_pack_result p = rc_rect_pack_add(&atlas->packer, image.size, build_arena);
    if (!p.placed)
        return g;                                   // overflow: placed stays false
    rc_image_blit(atlas->image, p.pos, image);
    g.uv     = normalize_uv(p.pos, image.size, atlas->image.size);
    g.placed = true;
    return g;
}

rc_glyph rc_font_atlas_add(rc_font_atlas *atlas, rc_glyph_table *table, uint32_t codepoint,
                           rc_arena *build_arena, rc_arena *table_arena)
{
    rc_glyph *existing = table_find_ptr(table, codepoint);
    if (existing)
        return *existing;

    // Render transiently: the SDF image into a by-value copy of table_arena, the
    // outline into a by-value copy of build_arena.  The image is consumed by
    // place() (blit) before table_set() grows the real table_arena over it.
    rc_arena image_arena = *table_arena;
    rc_font_glyph_result r = rc_font_get_glyph(&atlas->font, codepoint, &image_arena, *build_arena);

    rc_glyph g = {0};
    if (r.error == RC_FONT_OK)
        g = place(atlas, r.glyph.image, r.glyph.offset, r.glyph.advance, build_arena);

    table_set(table, codepoint, g, table_arena);
    return g;
}

uint32_t rc_font_atlas_add_range(rc_font_atlas *atlas, rc_glyph_table *table,
                                 uint32_t first, uint32_t last,
                                 rc_arena *build_arena, rc_arena *table_arena)
{
    if (last < first)
        return 0;

    // Render the whole set into build_arena (reclaimed when build_arena is freed),
    // then pack densest-first so the result matches rc_image_pack's density.
    rc_array_atlas_item items = rc_array_atlas_item_make(last - first + 1, build_arena);
    for (uint32_t cp = first; cp <= last; ++cp) {
        if (table_find_ptr(table, cp))
            continue;                               // already present
        rc_font_glyph_result r = rc_font_get_glyph(&atlas->font, cp, build_arena, *table_arena);
        if (r.error != RC_FONT_OK)
            continue;                               // skip malformed
        rc_array_atlas_item_push(&items, (atlas_item) {
            .codepoint = cp,
            .image     = r.glyph.image,
            .offset    = r.glyph.offset,
            .advance   = r.glyph.advance,
        }, build_arena);
    }

    sort_atlas_items(items.span);

    uint32_t missed = 0;
    for (uint32_t i = 0; i < items.num; ++i) {
        atlas_item *it = rc_array_atlas_item_at(&items, i);
        rc_glyph g = place(atlas, it->image, it->offset, it->advance, build_arena);
        if (!g.placed)
            ++missed;
        table_set(table, it->codepoint, g, table_arena);
    }
    return missed;
}
