/*
 * gfx_gl33_internal.h - shared declarations for the OpenGL 3.3 backend.
 *
 * Only the gl/ backend files include this (and through it, glad).  The
 * conversion tables in gfx_gl33_convert.c translate rc_gfx enums to GL
 * constants; gfx_gl33.c holds the device state and command playback.
 */

#ifndef RC_GFX_GL33_INTERNAL_H_
#define RC_GFX_GL33_INTERNAL_H_

#include <glad/gl.h>

#include "../gfx_internal.h"

/*
 * Extension constants glad (generated core-only) does not carry.  The
 * functions that consume them are core; only the enum values come from
 * extensions, so defining them here is enough.
 */
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT        0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT        0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT  0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT  0x8C4F
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
#define GL_COMPRESSED_RGBA_BPTC_UNORM_ARB       0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB 0x8E8D
#endif
#ifndef GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB 0x8E8F
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT           0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT       0x84FF
#endif

/* ---- enum conversion (gfx_gl33_convert.c) ---- */

/* GL upload/storage description of a texture format. */
typedef struct rc_gfx_gl_format {
    GLenum internal_format;
    GLenum format;    /* pixel transfer format; 0 for compressed formats */
    GLenum type;      /* pixel transfer type; 0 for compressed formats */
} rc_gfx_gl_format;

/* GL attribute pointer description of a vertex format. */
typedef struct rc_gfx_gl_vertex_format {
    GLint     size;         /* component count (or GL_BGRA-style special) */
    GLenum    type;
    GLboolean normalized;
    bool      integer;      /* use glVertexAttribIPointer */
} rc_gfx_gl_vertex_format;

rc_gfx_gl_format        rc_gfx_gl_format_get(rc_gfx_texture_format fmt);
rc_gfx_gl_vertex_format rc_gfx_gl_vertex_format_get(rc_gfx_vertex_format fmt);
GLenum rc_gfx_gl_compare(rc_gfx_compare compare);
GLenum rc_gfx_gl_stencil_op(rc_gfx_stencil_op op);
GLenum rc_gfx_gl_blend_factor(rc_gfx_blend_factor factor);
GLenum rc_gfx_gl_blend_op(rc_gfx_blend_op op);
GLenum rc_gfx_gl_primitive(rc_gfx_primitive primitive);
GLenum rc_gfx_gl_address(rc_gfx_address address);

/* Combined min/mip filter for sampler objects (mag maps directly). */
GLenum rc_gfx_gl_min_filter(rc_gfx_filter min_filter, rc_gfx_filter mip_filter);

#endif /* RC_GFX_GL33_INTERNAL_H_ */
