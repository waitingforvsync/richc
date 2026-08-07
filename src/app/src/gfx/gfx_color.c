/*
 * gfx_color.c - sRGB conversion helpers and texture format property tables.
 *
 * Pure functions over static tables: no device, no GL.  The sRGB conversions
 * use the exact piecewise curve from the sRGB specification, not a power-2.2
 * approximation - the linear segment near black matters.
 */

#include "richc/gfx/color.h"

#include <math.h>

#include "richc/macros.h"
#include "richc/ops.h"

/* ---- sRGB transfer curve ---- */

float rc_gfx_srgb_to_linear(float s)
{
    if (s <= 0.04045f) {
        return s / 12.92f;
    }
    return powf((s + 0.055f) / 1.055f, 2.4f);
}

float rc_gfx_linear_to_srgb(float l)
{
    if (l <= 0.0031308f) {
        return l * 12.92f;
    }
    return 1.055f * powf(l, 1.0f / 2.4f) - 0.055f;
}

rc_vec4f rc_gfx_color_from_srgb_u32(uint32_t rgba)
{
    // R in the low byte (the little-endian reading of RGBA8 bytes in memory);
    // alpha is never encoded, so it converts by scale alone
    return (rc_vec4f) {
        rc_gfx_srgb_to_linear((float)((rgba >> 0) & 0xFFu) / 255.0f),
        rc_gfx_srgb_to_linear((float)((rgba >> 8) & 0xFFu) / 255.0f),
        rc_gfx_srgb_to_linear((float)((rgba >> 16) & 0xFFu) / 255.0f),
        (float)((rgba >> 24) & 0xFFu) / 255.0f
    };
}

static uint32_t pack_channel(float value)
{
    float clamped = rc_max_f32(0.0f, rc_min_f32(1.0f, value));
    return (uint32_t)(clamped * 255.0f + 0.5f);
}

uint32_t rc_gfx_color_to_srgb_u32(rc_vec4f linear)
{
    return pack_channel(rc_gfx_linear_to_srgb(linear.x))
         | pack_channel(rc_gfx_linear_to_srgb(linear.y)) << 8
         | pack_channel(rc_gfx_linear_to_srgb(linear.z)) << 16
         | pack_channel(linear.w) << 24;
}

/* ---- format property tables ---- */

typedef struct format_props {
    uint8_t block_size;    // bytes per block
    uint8_t block_dim;     // blocks are square: 1 or 4
    bool    srgb;
    bool    depth;
    bool    stencil;
    bool    compressed;
} format_props;

static const format_props format_table[RC_GFX_TEXTURE_FORMAT_COUNT] = {
    [RC_GFX_TEXTURE_FORMAT_NONE]                  = {0, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_R8_UNORM]              = {1, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RG8_UNORM]             = {2, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM]           = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB]            = {4, 1, true,  false, false, false},
    [RC_GFX_TEXTURE_FORMAT_BGRA8_UNORM]           = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_BGRA8_SRGB]            = {4, 1, true,  false, false, false},
    [RC_GFX_TEXTURE_FORMAT_R8_UINT]               = {1, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGBA8_UINT]            = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_R16F]                  = {2, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RG16F]                 = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGBA16F]               = {8, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_R16_UINT]              = {2, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RG16_UINT]             = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_R32F]                  = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RG32F]                 = {8, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGBA32F]               = {16, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_R32_UINT]              = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGBA32_UINT]           = {16, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RGB10A2_UNORM]         = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_RG11B10F]              = {4, 1, false, false, false, false},
    [RC_GFX_TEXTURE_FORMAT_DEPTH16_UNORM]         = {2, 1, false, true,  false, false},
    [RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS]          = {4, 1, false, true,  false, false},
    [RC_GFX_TEXTURE_FORMAT_DEPTH32F]              = {4, 1, false, true,  false, false},
    [RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS_STENCIL8] = {4, 1, false, true,  true,  false},
    [RC_GFX_TEXTURE_FORMAT_DEPTH32F_STENCIL8]     = {8, 1, false, true,  true,  false},
    [RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM]        = {8, 4, false, false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB]         = {8, 4, true,  false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC3_RGBA_UNORM]        = {16, 4, false, false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB]         = {16, 4, true,  false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC4_R_UNORM]           = {8, 4, false, false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC5_RG_UNORM]          = {16, 4, false, false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC6H_RGB_FLOAT]        = {16, 4, false, false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM]        = {16, 4, false, false, false, true},
    [RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB]         = {16, 4, true,  false, false, true},
};

static format_props format_get(rc_gfx_texture_format fmt)
{
    RC_ASSERT((uint32_t)fmt < RC_GFX_TEXTURE_FORMAT_COUNT);
    return format_table[fmt];
}

bool rc_gfx_texture_format_is_srgb(rc_gfx_texture_format fmt)
{
    return format_get(fmt).srgb;
}

bool rc_gfx_texture_format_is_depth(rc_gfx_texture_format fmt)
{
    return format_get(fmt).depth;
}

bool rc_gfx_texture_format_is_stencil(rc_gfx_texture_format fmt)
{
    return format_get(fmt).stencil;
}

bool rc_gfx_texture_format_is_compressed(rc_gfx_texture_format fmt)
{
    return format_get(fmt).compressed;
}

uint32_t rc_gfx_texture_format_block_size(rc_gfx_texture_format fmt)
{
    return format_get(fmt).block_size;
}

rc_vec2i rc_gfx_texture_format_block_dim(rc_gfx_texture_format fmt)
{
    int32_t d = format_get(fmt).block_dim;
    return rc_vec2i_make(d, d);
}

rc_gfx_texture_format rc_gfx_texture_format_to_linear(rc_gfx_texture_format fmt)
{
    switch (fmt) {
    case RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB:    return RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM;
    case RC_GFX_TEXTURE_FORMAT_BGRA8_SRGB:    return RC_GFX_TEXTURE_FORMAT_BGRA8_UNORM;
    case RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB: return RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM;
    case RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB: return RC_GFX_TEXTURE_FORMAT_BC3_RGBA_UNORM;
    case RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB: return RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM;
    default:                                  return fmt;
    }
}

rc_gfx_texture_format rc_gfx_texture_format_to_srgb(rc_gfx_texture_format fmt)
{
    switch (fmt) {
    case RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM:    return RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB;
    case RC_GFX_TEXTURE_FORMAT_BGRA8_UNORM:    return RC_GFX_TEXTURE_FORMAT_BGRA8_SRGB;
    case RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM: return RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB;
    case RC_GFX_TEXTURE_FORMAT_BC3_RGBA_UNORM: return RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB;
    case RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM: return RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB;
    default:                                   return fmt;
    }
}
