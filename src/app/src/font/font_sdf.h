/*
 * font_sdf.h - internal: analytic signed-distance-field rasteriser.
 *
 * Renders a glyph outline (edges in pixel space, y up) into an R8 image whose
 * texel (i, j) is sampled at glyph-pixel (origin.x + i, origin.y - j) - i.e.
 * origin is the centre of the top-left texel and rows run downward (image y-down
 * over a y-up outline).  Each byte is clamp(round(128 + d_px * 128/spread)) with
 * d_px the signed distance (positive inside), from exact distance to every edge
 * and a nonzero-winding inside test.
 */

#ifndef RC_FONT_SDF_H_
#define RC_FONT_SDF_H_

#include "richc/font/font.h"
#include "richc/math/vec2f.h"

#include "font_ttf.h"   /* rc_view_font_edge */

/* size must be positive and origin the centre of texel (0,0).  The image is
 * allocated from arena; scratch (a distinct arena, by value) holds per-edge
 * precompute. */
rc_image font_sdf_render(rc_view_font_edge edges, rc_vec2i size, rc_vec2f origin,
                         float spread, rc_arena *arena, rc_arena scratch);

#endif /* RC_FONT_SDF_H_ */
