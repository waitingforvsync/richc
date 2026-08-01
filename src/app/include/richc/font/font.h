/*
 * font/font.h - TrueType (.ttf) loading and signed-distance-field glyph
 * rasterisation.
 *
 * An rc_font is built from an in-memory .ttf (rc_font_make) at a chosen pixel
 * size; rc_font_get_glyph then produces one glyph at a time as an R8 signed
 * distance field (SDF) plus the metrics needed to lay it out in a run of text.
 * The resulting glyph images are sized to drop straight into rc_image_atlas.
 *
 * Borrowing
 * ---------
 * rc_font_make borrows the .ttf bytes: the font keeps a view into them and reads
 * glyph outlines from them lazily in rc_font_get_glyph, so the bytes must outlive
 * the font (exactly as rc_image borrows the pixel bytes its arena owns).
 *
 * Signed distance field
 * ---------------------
 * Each glyph image is RC_PIXEL_FORMAT_R8 with the convention that 128 is exactly
 * on the outline, values above 128 are inside, below are outside; the byte is
 *   clamp(round(128 + signed_distance_px * 128 / spread), 0, 255),
 * with signed_distance_px positive inside.  spread (the distance, in pixels, that
 * maps onto the full byte range) is an internal detail derived from pixel_size
 * and is also the margin of padding around the glyph, so the field saturates to
 * 0 / 255 by the image edge.  An empty (whitespace) glyph has a zero-size image.
 *
 * Subpixel positioning
 * --------------------
 * One rasterisation per glyph reproduces every subpixel position: the metrics
 * (offset, advance) are full-precision floats taken from the font's true units,
 * and the fractional placement is encoded in the field itself (texel (i, j)
 * covers glyph-pixel [origin + i, origin + i + 1) and is sampled at its centre
 * origin + i + 0.5, matching GL texel-centre sampling).  To render at subpixel
 * precision, keep the pen in floats (pen += advance, never snapped) and draw the
 * glyph image with its top-left at pen + offset; bilinear filtering plus an
 * SDF threshold then reproduces the edge at its true position.
 *
 * Scope
 * -----
 * ASCII-oriented first pass: cmap formats 0/4/6/12, simple glyph outlines.
 * Composite glyphs currently yield an empty image with a correct advance.
 */

#ifndef RC_FONT_FONT_H_
#define RC_FONT_FONT_H_

#include "richc/arena.h"
#include "richc/image/image.h"
#include "richc/math/vec2f.h"

/* ---- errors ---- */

typedef enum rc_font_error {
    RC_FONT_OK = 0,
    RC_FONT_ERROR_NOT_TTF,       /* not an sfnt with TrueType outlines        */
    RC_FONT_ERROR_TRUNCATED,     /* a table or the table directory runs past the data */
    RC_FONT_ERROR_BAD_TABLE,     /* a required table is missing or malformed   */
    RC_FONT_ERROR_UNSUPPORTED,   /* e.g. no usable cmap subtable format         */
} rc_font_error;

/* ---- font ---- */

/*
 * A parsed font configured for one pixel size.  ascent / descent / line_gap are
 * public, in pixels (descent is negative, below the baseline); the trailing-_
 * fields are internal parse state.
 */
typedef struct rc_font {
    float ascent;            /* baseline to top, pixels (>= 0)              */
    float descent;           /* baseline to bottom, pixels (<= 0)          */
    float line_gap;          /* extra leading between lines, pixels        */

    rc_view_bytes ttf_;      /* borrowed font data                          */
    rc_view_bytes cmap_;     /* selected cmap subtable (from its start)      */
    rc_view_bytes loca_;     /* loca table                                  */
    rc_view_bytes glyf_;     /* glyf table                                  */
    rc_view_bytes hmtx_;     /* hmtx table                                  */
    uint32_t cmap_format_;   /* 0 / 4 / 6 / 12                              */
    uint32_t num_glyphs_;
    uint32_t num_hmetrics_;
    int32_t  loca_long_;     /* indexToLocFormat: 0 = short, 1 = long        */
    uint32_t units_per_em_;
    float    scale_;         /* pixel_size / units_per_em                    */
    float    spread_;        /* SDF half-range and padding, pixels           */
} rc_font;

typedef struct rc_font_result {
    rc_font       font;      /* zeroed on error                             */
    rc_font_error error;
} rc_font_result;

/*
 * Parse an in-memory .ttf and configure it to rasterise glyphs at pixel_size
 * (the em square's height in pixels: scale = pixel_size / unitsPerEm).  Borrows
 * ttf (see "Borrowing" above).  Untrusted input: validates and returns an error,
 * never traps.
 */
rc_font_result rc_font_make(rc_view_bytes ttf, float pixel_size, rc_arena *arena);

/* ---- glyph ---- */

/*
 * One glyph as an SDF plus layout metrics.  image is empty (size 0) for
 * whitespace and outline-less glyphs, which still carry a valid advance.  offset
 * is the image top-left relative to the pen origin on the baseline, in a screen
 * y-down frame: draw the image at (pen_x + offset.x, baseline_y + offset.y).
 */
typedef struct rc_font_glyph {
    rc_image image;      /* R8 SDF, 128 = on-edge; empty for whitespace      */
    rc_vec2f offset;     /* image top-left relative to pen origin, screen y-down */
    float    advance;    /* horizontal pen advance, pixels                   */
    uint32_t codepoint;  /* the requested codepoint                          */
    uint32_t index;      /* resolved glyph index (0 = .notdef)               */
} rc_font_glyph;

typedef struct rc_font_glyph_result {
    rc_font_glyph glyph;     /* zeroed image on error                        */
    rc_font_error error;
} rc_font_glyph_result;

/*
 * Rasterise the glyph for codepoint.  A codepoint absent from the cmap resolves
 * to glyph 0 (.notdef), not an error.  The glyph image is allocated from arena;
 * scratch (a distinct arena, passed by value and discarded) holds the transient
 * outline.  error is set only on genuinely malformed glyph data in an otherwise
 * valid font.
 */
rc_font_glyph_result rc_font_get_glyph(rc_font *font, uint32_t codepoint,
                                       rc_arena *arena, rc_arena scratch);

#endif /* RC_FONT_FONT_H_ */
