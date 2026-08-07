/*
 * gfx_gl33_convert.c - rc_gfx enum to GL constant conversion tables.
 *
 * Pure switches, each with an RC_UNREACHABLE default so the compiler treats
 * them as exhaustive over the validated enum ranges.
 */

#include "gfx_gl33_internal.h"

rc_gfx_gl_format rc_gfx_gl_format_get(rc_gfx_texture_format fmt)
{
    switch (fmt) {
    case RC_GFX_TEXTURE_FORMAT_R8_UNORM:      return (rc_gfx_gl_format) {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_RG8_UNORM:     return (rc_gfx_gl_format) {GL_RG8, GL_RG, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM:   return (rc_gfx_gl_format) {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB:    return (rc_gfx_gl_format) {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_BGRA8_UNORM:   return (rc_gfx_gl_format) {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_BGRA8_SRGB:    return (rc_gfx_gl_format) {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_R8_UINT:       return (rc_gfx_gl_format) {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_RGBA8_UINT:    return (rc_gfx_gl_format) {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE};
    case RC_GFX_TEXTURE_FORMAT_R16F:          return (rc_gfx_gl_format) {GL_R16F, GL_RED, GL_HALF_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_RG16F:         return (rc_gfx_gl_format) {GL_RG16F, GL_RG, GL_HALF_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_RGBA16F:       return (rc_gfx_gl_format) {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_R16_UINT:      return (rc_gfx_gl_format) {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT};
    case RC_GFX_TEXTURE_FORMAT_RG16_UINT:     return (rc_gfx_gl_format) {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT};
    case RC_GFX_TEXTURE_FORMAT_R32F:          return (rc_gfx_gl_format) {GL_R32F, GL_RED, GL_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_RG32F:         return (rc_gfx_gl_format) {GL_RG32F, GL_RG, GL_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_RGBA32F:       return (rc_gfx_gl_format) {GL_RGBA32F, GL_RGBA, GL_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_R32_UINT:      return (rc_gfx_gl_format) {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT};
    case RC_GFX_TEXTURE_FORMAT_RGBA32_UINT:   return (rc_gfx_gl_format) {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT};
    case RC_GFX_TEXTURE_FORMAT_RGB10A2_UNORM: return (rc_gfx_gl_format) {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV};
    case RC_GFX_TEXTURE_FORMAT_RG11B10F:      return (rc_gfx_gl_format) {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV};
    case RC_GFX_TEXTURE_FORMAT_DEPTH16_UNORM: return (rc_gfx_gl_format) {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT};
    case RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS:  return (rc_gfx_gl_format) {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT};
    case RC_GFX_TEXTURE_FORMAT_DEPTH32F:      return (rc_gfx_gl_format) {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
    case RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS_STENCIL8:
        return (rc_gfx_gl_format) {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
    case RC_GFX_TEXTURE_FORMAT_DEPTH32F_STENCIL8:
        return (rc_gfx_gl_format) {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV};
    case RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM: return (rc_gfx_gl_format) {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB:  return (rc_gfx_gl_format) {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC3_RGBA_UNORM: return (rc_gfx_gl_format) {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB:  return (rc_gfx_gl_format) {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC4_R_UNORM:    return (rc_gfx_gl_format) {GL_COMPRESSED_RED_RGTC1, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC5_RG_UNORM:   return (rc_gfx_gl_format) {GL_COMPRESSED_RG_RGTC2, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC6H_RGB_FLOAT: return (rc_gfx_gl_format) {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM: return (rc_gfx_gl_format) {GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, 0, 0};
    case RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB:  return (rc_gfx_gl_format) {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB, 0, 0};
    default:
        RC_UNREACHABLE();
    }
}

rc_gfx_gl_vertex_format rc_gfx_gl_vertex_format_get(rc_gfx_vertex_format fmt)
{
    switch (fmt) {
    case RC_GFX_VERTEX_FORMAT_U8X2:       return (rc_gfx_gl_vertex_format) {2, GL_UNSIGNED_BYTE, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U8X4:       return (rc_gfx_gl_vertex_format) {4, GL_UNSIGNED_BYTE, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I8X2:       return (rc_gfx_gl_vertex_format) {2, GL_BYTE, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I8X4:       return (rc_gfx_gl_vertex_format) {4, GL_BYTE, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U8X2_NORM:  return (rc_gfx_gl_vertex_format) {2, GL_UNSIGNED_BYTE, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_U8X4_NORM:  return (rc_gfx_gl_vertex_format) {4, GL_UNSIGNED_BYTE, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_I8X2_NORM:  return (rc_gfx_gl_vertex_format) {2, GL_BYTE, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_I8X4_NORM:  return (rc_gfx_gl_vertex_format) {4, GL_BYTE, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_U16X2:      return (rc_gfx_gl_vertex_format) {2, GL_UNSIGNED_SHORT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U16X4:      return (rc_gfx_gl_vertex_format) {4, GL_UNSIGNED_SHORT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I16X2:      return (rc_gfx_gl_vertex_format) {2, GL_SHORT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I16X4:      return (rc_gfx_gl_vertex_format) {4, GL_SHORT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U16X2_NORM: return (rc_gfx_gl_vertex_format) {2, GL_UNSIGNED_SHORT, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_U16X4_NORM: return (rc_gfx_gl_vertex_format) {4, GL_UNSIGNED_SHORT, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_I16X2_NORM: return (rc_gfx_gl_vertex_format) {2, GL_SHORT, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_I16X4_NORM: return (rc_gfx_gl_vertex_format) {4, GL_SHORT, GL_TRUE, false};
    case RC_GFX_VERTEX_FORMAT_F16X2:      return (rc_gfx_gl_vertex_format) {2, GL_HALF_FLOAT, GL_FALSE, false};
    case RC_GFX_VERTEX_FORMAT_F16X4:      return (rc_gfx_gl_vertex_format) {4, GL_HALF_FLOAT, GL_FALSE, false};
    case RC_GFX_VERTEX_FORMAT_F32:        return (rc_gfx_gl_vertex_format) {1, GL_FLOAT, GL_FALSE, false};
    case RC_GFX_VERTEX_FORMAT_F32X2:      return (rc_gfx_gl_vertex_format) {2, GL_FLOAT, GL_FALSE, false};
    case RC_GFX_VERTEX_FORMAT_F32X3:      return (rc_gfx_gl_vertex_format) {3, GL_FLOAT, GL_FALSE, false};
    case RC_GFX_VERTEX_FORMAT_F32X4:      return (rc_gfx_gl_vertex_format) {4, GL_FLOAT, GL_FALSE, false};
    case RC_GFX_VERTEX_FORMAT_U32:        return (rc_gfx_gl_vertex_format) {1, GL_UNSIGNED_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U32X2:      return (rc_gfx_gl_vertex_format) {2, GL_UNSIGNED_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U32X3:      return (rc_gfx_gl_vertex_format) {3, GL_UNSIGNED_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_U32X4:      return (rc_gfx_gl_vertex_format) {4, GL_UNSIGNED_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I32:        return (rc_gfx_gl_vertex_format) {1, GL_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I32X2:      return (rc_gfx_gl_vertex_format) {2, GL_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I32X3:      return (rc_gfx_gl_vertex_format) {3, GL_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_I32X4:      return (rc_gfx_gl_vertex_format) {4, GL_INT, GL_FALSE, true};
    case RC_GFX_VERTEX_FORMAT_RGB10A2_NORM:
        return (rc_gfx_gl_vertex_format) {4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, false};
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_compare(rc_gfx_compare compare)
{
    switch (compare) {
    case RC_GFX_COMPARE_ALWAYS:        return GL_ALWAYS;
    case RC_GFX_COMPARE_NEVER:         return GL_NEVER;
    case RC_GFX_COMPARE_LESS:          return GL_LESS;
    case RC_GFX_COMPARE_EQUAL:         return GL_EQUAL;
    case RC_GFX_COMPARE_LESS_EQUAL:    return GL_LEQUAL;
    case RC_GFX_COMPARE_GREATER:       return GL_GREATER;
    case RC_GFX_COMPARE_NOT_EQUAL:     return GL_NOTEQUAL;
    case RC_GFX_COMPARE_GREATER_EQUAL: return GL_GEQUAL;
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_stencil_op(rc_gfx_stencil_op op)
{
    switch (op) {
    case RC_GFX_STENCIL_OP_KEEP:       return GL_KEEP;
    case RC_GFX_STENCIL_OP_ZERO:       return GL_ZERO;
    case RC_GFX_STENCIL_OP_REPLACE:    return GL_REPLACE;
    case RC_GFX_STENCIL_OP_INVERT:     return GL_INVERT;
    case RC_GFX_STENCIL_OP_INCR_CLAMP: return GL_INCR;
    case RC_GFX_STENCIL_OP_DECR_CLAMP: return GL_DECR;
    case RC_GFX_STENCIL_OP_INCR_WRAP:  return GL_INCR_WRAP;
    case RC_GFX_STENCIL_OP_DECR_WRAP:  return GL_DECR_WRAP;
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_blend_factor(rc_gfx_blend_factor factor)
{
    switch (factor) {
    case RC_GFX_BLEND_FACTOR_ZERO:                 return GL_ZERO;
    case RC_GFX_BLEND_FACTOR_ONE:                  return GL_ONE;
    case RC_GFX_BLEND_FACTOR_SRC:                  return GL_SRC_COLOR;
    case RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC:        return GL_ONE_MINUS_SRC_COLOR;
    case RC_GFX_BLEND_FACTOR_SRC_ALPHA:            return GL_SRC_ALPHA;
    case RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
    case RC_GFX_BLEND_FACTOR_DST:                  return GL_DST_COLOR;
    case RC_GFX_BLEND_FACTOR_ONE_MINUS_DST:        return GL_ONE_MINUS_DST_COLOR;
    case RC_GFX_BLEND_FACTOR_DST_ALPHA:            return GL_DST_ALPHA;
    case RC_GFX_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:  return GL_ONE_MINUS_DST_ALPHA;
    case RC_GFX_BLEND_FACTOR_SRC_ALPHA_SATURATED:  return GL_SRC_ALPHA_SATURATE;
    case RC_GFX_BLEND_FACTOR_CONSTANT:             return GL_CONSTANT_COLOR;
    case RC_GFX_BLEND_FACTOR_ONE_MINUS_CONSTANT:   return GL_ONE_MINUS_CONSTANT_COLOR;
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_blend_op(rc_gfx_blend_op op)
{
    switch (op) {
    case RC_GFX_BLEND_OP_ADD:              return GL_FUNC_ADD;
    case RC_GFX_BLEND_OP_SUBTRACT:         return GL_FUNC_SUBTRACT;
    case RC_GFX_BLEND_OP_REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
    case RC_GFX_BLEND_OP_MIN:              return GL_MIN;
    case RC_GFX_BLEND_OP_MAX:              return GL_MAX;
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_primitive(rc_gfx_primitive primitive)
{
    switch (primitive) {
    case RC_GFX_PRIMITIVE_TRIANGLES:      return GL_TRIANGLES;
    case RC_GFX_PRIMITIVE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case RC_GFX_PRIMITIVE_LINES:          return GL_LINES;
    case RC_GFX_PRIMITIVE_LINE_STRIP:     return GL_LINE_STRIP;
    case RC_GFX_PRIMITIVE_POINTS:         return GL_POINTS;
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_address(rc_gfx_address address)
{
    switch (address) {
    case RC_GFX_ADDRESS_CLAMP_TO_EDGE:   return GL_CLAMP_TO_EDGE;
    case RC_GFX_ADDRESS_REPEAT:          return GL_REPEAT;
    case RC_GFX_ADDRESS_MIRROR_REPEAT:   return GL_MIRRORED_REPEAT;
    case RC_GFX_ADDRESS_CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;
    default:
        RC_UNREACHABLE();
    }
}

GLenum rc_gfx_gl_min_filter(rc_gfx_filter min_filter, rc_gfx_filter mip_filter)
{
    // always a mipmapped mode: a texture clamped to one level stays complete,
    // and the sampler need not know whether its texture carries mips
    if (min_filter == RC_GFX_FILTER_NEAREST) {
        return mip_filter == RC_GFX_FILTER_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_LINEAR;
    }
    return mip_filter == RC_GFX_FILTER_NEAREST ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
}
