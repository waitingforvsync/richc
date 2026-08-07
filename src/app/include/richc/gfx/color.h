/*
 * gfx/color.h - sRGB conversion helpers.
 *
 * Everything a shader sees is linear; encoding is a property of texture
 * format, applied by fixed-function hardware.  These helpers exist for the
 * one place a conversion is legitimately done on the CPU: colour constants
 * authored in sRGB (hex values from a design tool) that must be converted to
 * linear before they reach a shader, a clear value, or a blend constant.
 *
 * All functions use the exact piecewise sRGB curve, not a 2.2 power
 * approximation - the linear segment near black matters for dark UI colours.
 * Alpha is never encoded: sRGB applies to RGB only.
 *
 * Functions
 * ---------
 *   rc_gfx_srgb_to_linear(s)       - decode one sRGB-encoded channel
 *   rc_gfx_linear_to_srgb(l)       - encode one linear channel
 *   rc_gfx_color_from_srgb_u32(c)  - packed 0xAABBGGRR sRGB -> linear vec4
 *   rc_gfx_color_to_srgb_u32(c)    - linear vec4 -> packed 0xAABBGGRR sRGB
 */

#ifndef RC_GFX_COLOR_H_
#define RC_GFX_COLOR_H_

#include "richc/gfx/gfx.h"
#include "richc/math/vec4f.h"

/* Decode a single sRGB-encoded channel value in [0, 1] to linear. */
float rc_gfx_srgb_to_linear(float s);

/* Encode a single linear channel value in [0, 1] to sRGB. */
float rc_gfx_linear_to_srgb(float l);

/* Unpack 0xAABBGGRR (R in the low byte, the memory order of RGBA8 bytes) and
 * decode RGB to linear; alpha passes through untouched. */
rc_vec4f rc_gfx_color_from_srgb_u32(uint32_t rgba);

/* Encode linear RGB to sRGB and pack as 0xAABBGGRR; alpha passes through
 * untouched.  Channels are clamped to [0, 1] before packing. */
uint32_t rc_gfx_color_to_srgb_u32(rc_vec4f linear);

#endif /* RC_GFX_COLOR_H_ */
