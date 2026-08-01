/*
 * font_ttf.h - internal: TrueType table parsing and glyph outline extraction.
 *
 * Not part of the public API; lives beside the font .c files.  Turns borrowed
 * .ttf bytes into table views + metrics (font_ttf_parse), maps codepoints to
 * glyph indices (font_ttf_glyph_index), reads advance widths
 * (font_ttf_advance_units), and extracts a glyph's outline as a flat list of
 * line / quadratic-Bezier edges (font_ttf_outline).  All untrusted input is
 * validated and reported via rc_font_error; nothing here traps on bad data.
 */

#ifndef RC_FONT_TTF_H_
#define RC_FONT_TTF_H_

#include "richc/font/font.h"
#include "richc/math/box2f.h"
#include "richc/math/vec2f.h"

/* One outline segment in pixel space, y up (font orientation).  A line runs
 * p0 -> p2 (p1 unused); a quadratic Bezier runs p0 -> p1 (control) -> p2. */
typedef struct font_edge {
    rc_vec2f p0;
    rc_vec2f p1;
    rc_vec2f p2;
    bool     is_quad;
} font_edge;

#define RC_ARRAY_TYPE font_edge
#define RC_ARRAY_NAME font_edge
#include "richc/template/array.h"

/* Parsed table set + raw (font-unit) metrics.  Filled by font_ttf_parse. */
typedef struct font_ttf {
    rc_view_bytes ttf;
    rc_view_bytes cmap;       /* selected cmap subtable, from its start */
    rc_view_bytes loca;
    rc_view_bytes glyf;
    rc_view_bytes hmtx;
    uint32_t cmap_format;     /* 0 / 4 / 6 / 12 */
    uint32_t num_glyphs;
    uint32_t num_hmetrics;
    int32_t  loca_long;       /* indexToLocFormat: 0 short, 1 long */
    uint32_t units_per_em;
    int32_t  ascent;          /* hhea, font units */
    int32_t  descent;         /* hhea, font units (negative) */
    int32_t  line_gap;        /* hhea, font units */
} font_ttf;

/* A glyph's extracted outline, scaled to pixel space (y up). */
typedef struct font_outline {
    rc_view_font_edge edges;  /* allocated from the caller's arena */
    rc_box2f          bbox;   /* glyf-header bbox, scaled to pixels */
    bool              composite;  /* true -> edges empty, render as blank */
} font_outline;

typedef struct font_outline_result {
    font_outline  outline;   /* zeroed on error */
    rc_font_error error;
} font_outline_result;

/* Parse the table directory and required tables.  Returns RC_FONT_OK and fills
 * *out, or an error (out is left zeroed). */
rc_font_error font_ttf_parse(rc_view_bytes ttf, font_ttf *out);

/* Map a codepoint to a glyph index via the selected cmap subtable; 0 (.notdef)
 * if absent or unsupported. */
uint32_t font_ttf_glyph_index(const font_ttf *f, uint32_t codepoint);

/* Advance width of a glyph index, in font units. */
uint32_t font_ttf_advance_units(const font_ttf *f, uint32_t gid);

/* Extract glyph `gid`'s outline, scaling points by `scale` into pixel space.
 * Edges are allocated from arena.  A composite glyph yields .composite with no
 * edges; on malformed data the result carries a non-OK error. */
font_outline_result font_ttf_outline(const font_ttf *f, uint32_t gid, float scale,
                                     rc_arena *arena);

#endif /* RC_FONT_TTF_H_ */
