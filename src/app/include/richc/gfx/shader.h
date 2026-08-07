/*
 * gfx/shader.h - shader programs.
 *
 * Phase 1 takes GLSL 330 source directly; there is no shading-language
 * abstraction.  The backend prepends a preamble to both stages, so sources
 * must not contain a #version line.  The vertex shader must write gl_Position
 * through rc_clip(), the injected hook that absorbs every clip-space
 * convention difference between backends:
 *
 *   gl_Position = rc_clip(u_mvp * vec4(a_pos, 0.0, 1.0));
 *
 * Vertex attributes are matched by location only: declare them with
 * layout(location = N).  All shader constants live in std140 uniform blocks
 * declared without an instance name; there are no loose uniforms.  The
 * preamble also defines RC_GFX_BACKEND_GL33 and RC_STORAGE_LOAD(name, i),
 * the storage-buffer access macro (texelFetch on a samplerBuffer under GL 3.3).
 *
 * Types
 * -----
 *   rc_gfx_uniform_member              - name/offset/size row for std140
 *                                        validation (debug builds)
 *   rc_gfx_shader_uniform_block       - maps a GLSL block to (group, binding)
 *   rc_gfx_shader_texture_sampler_pair - maps a GLSL combined sampler uniform
 *                                        back onto the separate texture and
 *                                        sampler bindings the API exposes
 *   rc_gfx_shader_desc
 *
 * Functions
 * ---------
 *   rc_gfx_shader_make(desc) / rc_gfx_shader_destroy(shd)
 */

#ifndef RC_GFX_SHADER_H_
#define RC_GFX_SHADER_H_

#include "richc/gfx/gfx.h"

/* One member row for debug std140 validation (see rc_gfx_shader_uniform_block).
 * Catches vec3/mat3/array padding mismatches at load time instead of as silent
 * corruption. */
typedef struct rc_gfx_uniform_member {
    rc_str   name;      /* GLSL member name, e.g. "u_mvp" */
    uint32_t offset;    /* offsetof() in the matching C struct */
    uint32_t size;      /* sizeof() the member */
} rc_gfx_uniform_member;

typedef struct rc_gfx_shader_uniform_block {
    rc_str   glsl_name;        /* the block name in GLSL, e.g. "DrawUniforms" */
    uint32_t group;
    uint32_t binding;
    uint32_t size;             /* sizeof() the matching C struct */
    const rc_gfx_uniform_member *members;   /* optional, debug validation */
    uint32_t member_count;
} rc_gfx_shader_uniform_block;

/*
 * GLSL 330 has combined samplers; this maps a GLSL sampler uniform back onto
 * the separate texture and sampler bindings.  For a samplerBuffer backing a
 * STORAGE_BUFFER_READ binding, point the texture fields at that binding and
 * leave the sampler fields unused (no sampler binding exists for it).
 * Dropped entirely when we move to SPIR-V / WGSL backends.
 */
typedef struct rc_gfx_shader_texture_sampler_pair {
    rc_str   glsl_name;                        /* e.g. "u_albedo" */
    uint32_t texture_group;
    uint32_t texture_binding;
    uint32_t sampler_group;
    uint32_t sampler_binding;
} rc_gfx_shader_texture_sampler_pair;

typedef struct rc_gfx_shader_desc {
    rc_str vs_source;                          /* GLSL 330 body; no #version line */
    rc_str fs_source;
    const rc_gfx_shader_uniform_block        *uniform_blocks;
    uint32_t                                  uniform_block_count;
    const rc_gfx_shader_texture_sampler_pair *texture_samplers;
    uint32_t                                  texture_sampler_count;
    rc_str label;
} rc_gfx_shader_desc;

rc_gfx_shader rc_gfx_shader_make(const rc_gfx_shader_desc *desc);
void          rc_gfx_shader_destroy(rc_gfx_shader shd);

#endif /* RC_GFX_SHADER_H_ */
