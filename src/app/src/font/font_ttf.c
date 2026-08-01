#include "font_ttf.h"

#include "richc/arena.h"
#include "richc/array/u8.h"
#include "richc/math/array/vec2f.h"

// sfnt table tags, as the big-endian uint32 of the four tag bytes.
enum {
    TAG_head = 0x68656164u,
    TAG_maxp = 0x6D617870u,
    TAG_hhea = 0x68686561u,
    TAG_hmtx = 0x686D7478u,
    TAG_cmap = 0x636D6170u,
    TAG_loca = 0x6C6F6361u,
    TAG_glyf = 0x676C7966u,
};

// glyf simple-glyph flag bits.
enum {
    FLAG_ON_CURVE  = 0x01,
    FLAG_X_SHORT   = 0x02,
    FLAG_Y_SHORT   = 0x04,
    FLAG_REPEAT    = 0x08,
    FLAG_X_SAME    = 0x10,   // if X_SHORT: x delta is positive; else x unchanged
    FLAG_Y_SAME    = 0x20,
};

// cmap platform IDs.
enum {
    CMAP_PLAT_UNICODE = 0,
    CMAP_PLAT_MAC     = 1,
    CMAP_PLAT_WINDOWS = 3,
};

// cmap encoding IDs (interpreted per platform).
enum {
    CMAP_ENC_WIN_SYMBOL = 0,    // Windows: symbol
    CMAP_ENC_WIN_BMP    = 1,    // Windows: Unicode BMP
    CMAP_ENC_WIN_UCS4   = 10,   // Windows: Unicode full repertoire
    CMAP_ENC_MAC_ROMAN  = 0,    // Macintosh: Roman
};

// cmap subtable formats we support.
enum {
    CMAP_FMT_BYTE      = 0,     // byte encoding table
    CMAP_FMT_SEGMENT   = 4,     // segment mapping to delta values (BMP)
    CMAP_FMT_TRIMMED   = 6,     // trimmed table mapping
    CMAP_FMT_SEGMENTED = 12,    // segmented coverage (UCS-4)
};

// ---- bounds-checked big-endian readers (return 0 past the end) ----

static uint32_t rd8(rc_view_bytes d, uint32_t o)
{
    return o < d.num ? rc_view_bytes_get(d, o) : 0;
}

static uint32_t rd16(rc_view_bytes d, uint32_t o)
{
    if ((uint64_t)o + 2 > d.num)
        return 0;
    return ((uint32_t)rc_view_bytes_get(d, o) << 8) | rc_view_bytes_get(d, o + 1);
}

static uint32_t rd32(rc_view_bytes d, uint32_t o)
{
    if ((uint64_t)o + 4 > d.num)
        return 0;
    return ((uint32_t)rc_view_bytes_get(d, o)     << 24)
         | ((uint32_t)rc_view_bytes_get(d, o + 1) << 16)
         | ((uint32_t)rc_view_bytes_get(d, o + 2) << 8)
         |  (uint32_t)rc_view_bytes_get(d, o + 3);
}

static int32_t rd16s(rc_view_bytes d, uint32_t o)
{
    return (int16_t)(uint16_t)rd16(d, o);
}

// ---- table directory ----

// Locate a table by tag; returns a view over it ({0} if absent or out of range).
static rc_view_bytes find_table(rc_view_bytes ttf, uint32_t num_tables, uint32_t tag)
{
    for (uint32_t i = 0; i < num_tables; ++i) {
        uint32_t rec = 12 + i * 16;
        if (rd32(ttf, rec) != tag)
            continue;
        uint32_t off = rd32(ttf, rec + 8);
        uint32_t len = rd32(ttf, rec + 12);
        if ((uint64_t)off + len > ttf.num)
            return (rc_view_bytes) {0};
        return rc_view_bytes_get_subview(ttf, off, off + len);
    }
    return (rc_view_bytes) {0};
}

// cmap subtable preference: higher score wins (Unicode full > BMP > generic).
static int cmap_score(uint32_t plat, uint32_t enc)
{
    if (plat == CMAP_PLAT_WINDOWS && enc == CMAP_ENC_WIN_UCS4)   return 5;
    if (plat == CMAP_PLAT_WINDOWS && enc == CMAP_ENC_WIN_BMP)    return 4;
    if (plat == CMAP_PLAT_UNICODE)                              return 3;
    if (plat == CMAP_PLAT_WINDOWS && enc == CMAP_ENC_WIN_SYMBOL) return 2;
    if (plat == CMAP_PLAT_MAC && enc == CMAP_ENC_MAC_ROMAN)      return 1;
    return 0;
}

static rc_font_error select_cmap(rc_view_bytes cmap, rc_view_bytes *out_sub, uint32_t *out_fmt)
{
    uint32_t n = rd16(cmap, 2);
    int best = -1;
    uint32_t best_off = 0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t rec  = 4 + i * 8;
        uint32_t plat = rd16(cmap, rec);
        uint32_t enc  = rd16(cmap, rec + 2);
        uint32_t off  = rd32(cmap, rec + 4);
        int score = cmap_score(plat, enc);
        if (score > best && off < cmap.num) {
            best = score;
            best_off = off;
        }
    }
    if (best < 0)
        return RC_FONT_ERROR_UNSUPPORTED;

    rc_view_bytes sub = rc_view_bytes_get_subview(cmap, best_off, cmap.num);
    uint32_t fmt = rd16(sub, 0);
    if (fmt != CMAP_FMT_BYTE && fmt != CMAP_FMT_SEGMENT &&
        fmt != CMAP_FMT_TRIMMED && fmt != CMAP_FMT_SEGMENTED)
        return RC_FONT_ERROR_UNSUPPORTED;
    *out_sub = sub;
    *out_fmt = fmt;
    return RC_FONT_OK;
}

rc_font_error font_ttf_parse(rc_view_bytes ttf, font_ttf *out)
{
    *out = (font_ttf) {0};

    if (ttf.num < 12)
        return RC_FONT_ERROR_TRUNCATED;

    uint32_t ver = rd32(ttf, 0);
    if (ver == 0x4F54544Fu)                       // 'OTTO' - CFF outlines
        return RC_FONT_ERROR_UNSUPPORTED;
    if (ver != 0x00010000u && ver != 0x74727565u) // 1.0 or 'true'
        return RC_FONT_ERROR_NOT_TTF;

    uint32_t num_tables = rd16(ttf, 4);
    if ((uint64_t)12 + (uint64_t)num_tables * 16 > ttf.num)
        return RC_FONT_ERROR_TRUNCATED;

    rc_view_bytes head = find_table(ttf, num_tables, TAG_head);
    rc_view_bytes maxp = find_table(ttf, num_tables, TAG_maxp);
    rc_view_bytes hhea = find_table(ttf, num_tables, TAG_hhea);
    rc_view_bytes hmtx = find_table(ttf, num_tables, TAG_hmtx);
    rc_view_bytes cmap = find_table(ttf, num_tables, TAG_cmap);
    rc_view_bytes loca = find_table(ttf, num_tables, TAG_loca);
    rc_view_bytes glyf = find_table(ttf, num_tables, TAG_glyf);

    if (head.num < 54 || maxp.num < 6 || hhea.num < 36 ||
        hmtx.data == NULL || cmap.data == NULL ||
        loca.data == NULL || glyf.data == NULL)
        return RC_FONT_ERROR_BAD_TABLE;

    uint32_t units = rd16(head, 18);
    if (units == 0)
        return RC_FONT_ERROR_BAD_TABLE;

    font_ttf f = {
        .ttf          = ttf,
        .loca         = loca,
        .glyf         = glyf,
        .hmtx         = hmtx,
        .num_glyphs   = rd16(maxp, 4),
        .num_hmetrics = rd16(hhea, 34),
        .loca_long    = rd16s(head, 50),
        .units_per_em = units,
        .ascent       = rd16s(hhea, 4),
        .descent      = rd16s(hhea, 6),
        .line_gap     = rd16s(hhea, 8),
    };
    if (f.num_glyphs == 0 || f.num_hmetrics == 0)
        return RC_FONT_ERROR_BAD_TABLE;

    rc_font_error e = select_cmap(cmap, &f.cmap, &f.cmap_format);
    if (e != RC_FONT_OK)
        return e;

    *out = f;
    return RC_FONT_OK;
}

// ---- cmap lookup ----

static uint32_t cmap_lookup_byte(rc_view_bytes t, uint32_t c)
{
    if (c >= 256)
        return 0;
    return rd8(t, 6 + c);
}

static uint32_t cmap_lookup_trimmed(rc_view_bytes t, uint32_t c)
{
    uint32_t first = rd16(t, 6);
    uint32_t count = rd16(t, 8);
    if (c < first || c >= first + count)
        return 0;
    return rd16(t, 10 + (c - first) * 2);
}

static uint32_t cmap_lookup_segmented(rc_view_bytes t, uint32_t c)
{
    uint32_t groups = rd32(t, 12);
    for (uint32_t i = 0; i < groups; ++i) {
        uint32_t g     = 16 + i * 12;
        uint32_t start = rd32(t, g);
        uint32_t end   = rd32(t, g + 4);
        if (c >= start && c <= end)
            return rd32(t, g + 8) + (c - start);
    }
    return 0;
}

static uint32_t cmap_lookup_segment(rc_view_bytes t, uint32_t c)
{
    if (c > 0xFFFF)
        return 0;
    uint32_t segx2  = rd16(t, 6);
    uint32_t seg    = segx2 / 2;
    uint32_t end0   = 14;
    uint32_t start0 = 16 + segx2;
    uint32_t delta0 = 16 + segx2 * 2;
    uint32_t range0 = 16 + segx2 * 3;
    for (uint32_t i = 0; i < seg; ++i) {
        if (c > rd16(t, end0 + i * 2))
            continue;
        uint32_t start = rd16(t, start0 + i * 2);
        if (c < start)
            return 0;                       // in a gap before this segment
        int32_t  delta = rd16s(t, delta0 + i * 2);
        uint32_t roff  = rd16(t, range0 + i * 2);
        if (roff == 0)
            return (uint32_t)(c + delta) & 0xFFFFu;
        // glyph id read from glyphIdArray, addressed relative to the roff field
        uint32_t addr = (range0 + i * 2) + roff + (c - start) * 2;
        uint32_t gid  = rd16(t, addr);
        if (gid == 0)
            return 0;
        return (uint32_t)(gid + delta) & 0xFFFFu;
    }
    return 0;
}

uint32_t font_ttf_glyph_index(const font_ttf *f, uint32_t codepoint)
{
    uint32_t gid;
    switch (f->cmap_format) {
        case CMAP_FMT_BYTE:      gid = cmap_lookup_byte(f->cmap, codepoint);  break;
        case CMAP_FMT_SEGMENT:   gid = cmap_lookup_segment(f->cmap, codepoint);  break;
        case CMAP_FMT_TRIMMED:   gid = cmap_lookup_trimmed(f->cmap, codepoint);  break;
        case CMAP_FMT_SEGMENTED: gid = cmap_lookup_segmented(f->cmap, codepoint); break;
        default: gid = 0; break;
    }
    return gid < f->num_glyphs ? gid : 0;
}

uint32_t font_ttf_advance_units(const font_ttf *f, uint32_t gid)
{
    uint32_t k = gid < f->num_hmetrics ? gid : f->num_hmetrics - 1;
    return rd16(f->hmtx, k * 4);
}

// ---- outline extraction ----

static rc_vec2f mid(rc_vec2f a, rc_vec2f b)
{
    return rc_vec2f_scalar_mul(rc_vec2f_add(a, b), 0.5f);
}

// Append a point to the expanded contour list, inserting an implied on-curve
// midpoint when two off-curve points would otherwise be adjacent.
static void contour_append(rc_array_vec2f *epos, rc_array_u8 *eon, rc_vec2f p, bool on,
                           rc_arena *arena)
{
    uint32_t n = epos->num;
    if (!on && n > 0 && !rc_array_u8_get(eon, n - 1)) {
        rc_array_vec2f_push(epos, mid(rc_array_vec2f_get(epos, n - 1), p), arena);
        rc_array_u8_push(eon, 1, arena);
    }
    rc_array_vec2f_push(epos, p, arena);
    rc_array_u8_push(eon, (uint8_t)on, arena);
}

static void emit_contour(rc_array_font_edge *arr, rc_view_vec2f P, rc_view_u8 ON,
                         uint32_t base, uint32_t m, rc_array_vec2f *epos,
                         rc_array_u8 *eon, rc_arena *arena)
{
    if (m == 0)
        return;

    rc_array_vec2f_reset(epos);   // reuse the buffers' capacity across contours
    rc_array_u8_reset(eon);

    // pick a starting on-curve point, or synthesise one between the ends
    uint32_t first_on = m;
    for (uint32_t k = 0; k < m; ++k)
        if (rc_view_u8_get(ON, base + k)) { first_on = k; break; }

    rc_vec2f start;
    if (first_on == m) {
        start = mid(rc_view_vec2f_get(P, base), rc_view_vec2f_get(P, base + m - 1));
        contour_append(epos, eon, start, true, arena);
        for (uint32_t k = 0; k < m; ++k)
            contour_append(epos, eon, rc_view_vec2f_get(P, base + k), false, arena);
    } else {
        start = rc_view_vec2f_get(P, base + first_on);
        contour_append(epos, eon, start, true, arena);
        for (uint32_t k = 1; k < m; ++k) {
            uint32_t i = base + (first_on + k) % m;
            contour_append(epos, eon, rc_view_vec2f_get(P, i), rc_view_u8_get(ON, i) != 0, arena);
        }
    }
    contour_append(epos, eon, start, true, arena);   // close back to the start

    rc_vec2f cur = rc_array_vec2f_get(epos, 0);
    uint32_t i = 1;
    while (i < epos->num) {
        if (rc_array_u8_get(eon, i)) {
            rc_vec2f pi = rc_array_vec2f_get(epos, i);
            if (!rc_vec2f_is_equal(cur, pi))
                rc_array_font_edge_push(arr, (font_edge) {.p0 = cur, .p2 = pi}, arena);
            cur = pi;
            i += 1;
        } else {
            rc_vec2f ctrl = rc_array_vec2f_get(epos, i);
            rc_vec2f nxt  = rc_array_vec2f_get(epos, i + 1);
            rc_array_font_edge_push(arr,
                (font_edge) {.p0 = cur, .p1 = ctrl, .p2 = nxt, .is_quad = true}, arena);
            cur = nxt;
            i += 2;
        }
    }
}

static font_outline_result outline_fail(rc_font_error error)
{
    return (font_outline_result) {.error = error};
}

font_outline_result font_ttf_outline(const font_ttf *f, uint32_t gid, float scale,
                                     rc_arena *arena)
{
    font_outline out = {0};

    // glyph byte range from loca
    uint32_t goff, gend;
    if (f->loca_long) {
        goff = rd32(f->loca, gid * 4);
        gend = rd32(f->loca, gid * 4 + 4);
    } else {
        goff = rd16(f->loca, gid * 2) * 2;
        gend = rd16(f->loca, gid * 2 + 2) * 2;
    }
    if (goff > gend || gend > f->glyf.num)
        return outline_fail(RC_FONT_ERROR_BAD_TABLE);
    if (goff == gend)
        return (font_outline_result) {.outline = out};   // blank glyph (e.g. space)

    rc_view_bytes g = rc_view_bytes_get_subview(f->glyf, goff, gend);
    if (g.num < 10)
        return outline_fail(RC_FONT_ERROR_BAD_TABLE);

    int32_t ncont = rd16s(g, 0);
    rc_vec2f bmin = {(float)rd16s(g, 2) * scale, (float)rd16s(g, 4) * scale};
    rc_vec2f bmax = {(float)rd16s(g, 6) * scale, (float)rd16s(g, 8) * scale};
    out.bbox = rc_box2f_make(bmin, bmax);

    if (ncont < 0) {
        out.composite = true;                            // composite: blank for now
        return (font_outline_result) {.outline = out};
    }
    if (ncont == 0)
        return (font_outline_result) {.outline = out};

    uint32_t nc = (uint32_t)ncont;
    if ((uint64_t)10 + (uint64_t)nc * 2 + 2 > g.num)
        return outline_fail(RC_FONT_ERROR_BAD_TABLE);

    uint32_t num_points = rd16(g, 10 + (nc - 1) * 2) + 1;

    uint32_t o = 10 + nc * 2;
    uint32_t instr_len = rd16(g, o);
    o += 2 + instr_len;
    if (o > g.num)
        return outline_fail(RC_FONT_ERROR_BAD_TABLE);

    // scratch arrays, each reserved to its known maximum
    rc_array_u8    flags = rc_array_u8_make(num_points, arena);
    rc_array_vec2f P     = rc_array_vec2f_make(num_points, arena);
    rc_array_u8    ON    = rc_array_u8_make(num_points, arena);
    rc_array_vec2f epos  = rc_array_vec2f_make(2 * num_points + 4, arena);
    rc_array_u8    eon   = rc_array_u8_make(2 * num_points + 4, arena);

    // flags (with run-length repeat)
    while (flags.num < num_points) {
        if (o >= g.num)
            return outline_fail(RC_FONT_ERROR_BAD_TABLE);
        uint8_t fl = (uint8_t)rd8(g, o++);
        rc_array_u8_push(&flags, fl, arena);
        if (fl & FLAG_REPEAT) {
            if (o >= g.num)
                return outline_fail(RC_FONT_ERROR_BAD_TABLE);
            uint32_t rep = rd8(g, o++);
            while (rep-- && flags.num < num_points)
                rc_array_u8_push(&flags, fl, arena);
        }
    }

    // verify the coordinate bytes fit before reading them
    uint64_t xbytes = 0, ybytes = 0;
    for (uint32_t i = 0; i < num_points; ++i) {
        uint8_t fl = rc_array_u8_get(&flags, i);
        if (fl & FLAG_X_SHORT)      xbytes += 1;
        else if (!(fl & FLAG_X_SAME)) xbytes += 2;
        if (fl & FLAG_Y_SHORT)      ybytes += 1;
        else if (!(fl & FLAG_Y_SAME)) ybytes += 2;
    }
    if ((uint64_t)o + xbytes + ybytes > g.num)
        return outline_fail(RC_FONT_ERROR_BAD_TABLE);

    // x then y, both delta-encoded; the x pass pushes each point (y zero), the
    // y pass fills the y in place and records the on-curve flag
    int32_t acc = 0;
    for (uint32_t i = 0; i < num_points; ++i) {
        uint8_t fl = rc_array_u8_get(&flags, i);
        if (fl & FLAG_X_SHORT) {
            uint32_t d = rd8(g, o++);
            acc += (fl & FLAG_X_SAME) ? (int32_t)d : -(int32_t)d;
        } else if (!(fl & FLAG_X_SAME)) {
            acc += rd16s(g, o);
            o += 2;
        }
        rc_array_vec2f_push(&P, (rc_vec2f) {(float)acc * scale, 0.0f}, arena);
    }
    acc = 0;
    for (uint32_t i = 0; i < num_points; ++i) {
        uint8_t fl = rc_array_u8_get(&flags, i);
        if (fl & FLAG_Y_SHORT) {
            uint32_t d = rd8(g, o++);
            acc += (fl & FLAG_Y_SAME) ? (int32_t)d : -(int32_t)d;
        } else if (!(fl & FLAG_Y_SAME)) {
            acc += rd16s(g, o);
            o += 2;
        }
        rc_array_vec2f_at(&P, i)->y = (float)acc * scale;
        rc_array_u8_push(&ON, (uint8_t)((fl & FLAG_ON_CURVE) != 0), arena);
    }

    rc_array_font_edge arr = rc_array_font_edge_make(num_points * 2, arena);

    uint32_t base = 0;
    for (uint32_t c = 0; c < nc; ++c) {
        uint32_t end = rd16(g, 10 + c * 2);
        if (end >= num_points || end < base)
            return outline_fail(RC_FONT_ERROR_BAD_TABLE);
        emit_contour(&arr, P.view, ON.view, base, end - base + 1, &epos, &eon, arena);
        base = end + 1;
    }

    out.edges = arr.view;
    return (font_outline_result) {.outline = out};
}
