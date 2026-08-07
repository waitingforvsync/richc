/*
 * gfx_internal.h - shared internal structures and the backend interface.
 *
 * The frontend (gfx.c) owns the handle pools, validation, the uniform ring
 * shadow, and the deferred-destruction queue.  The backend (one of gl/...)
 * owns every GL object and all device state.  The backend interface is a set
 * of rc_gfx_backend_* functions implemented once per backend and selected at
 * compile time - no vtable, no runtime switching.
 *
 * GL object names are stored as plain uint32_t so no GL header is needed
 * here; only the backend .c files include glad.
 */

#ifndef RC_GFX_INTERNAL_H_
#define RC_GFX_INTERNAL_H_

#include <string.h>

#include "richc/gfx/bindings.h"
#include "richc/gfx/buffer.h"
#include "richc/gfx/encoder.h"
#include "richc/gfx/gfx.h"
#include "richc/gfx/pass.h"
#include "richc/gfx/pipeline.h"
#include "richc/gfx/shader.h"
#include "richc/gfx/texture.h"
#include "richc/macros.h"

/* ---- command stream ---- */

/*
 * The encoder writes a packed opcode stream: a uint32_t opcode followed by a
 * POD payload struct, with variable-length payloads (dynamic offset arrays)
 * inline after.  Payloads are memcpy'd in and out, so alignment never
 * matters.  rc_str fields carry their pointer; the pointed-at text must
 * outlive the command buffer (it is only read at submit / dump time).
 */
typedef enum rc_gfx_op {
    RC_GFX_OP_PASS_BEGIN = 1,
    RC_GFX_OP_PASS_END,
    RC_GFX_OP_SET_PIPELINE,
    RC_GFX_OP_SET_BIND_GROUP,
    RC_GFX_OP_SET_VERTEX_BUFFER,
    RC_GFX_OP_SET_INDEX_BUFFER,
    RC_GFX_OP_SET_VIEWPORT,
    RC_GFX_OP_SET_SCISSOR,
    RC_GFX_OP_SET_BLEND_CONSTANT,
    RC_GFX_OP_SET_STENCIL_REFERENCE,
    RC_GFX_OP_DRAW,
    RC_GFX_OP_DRAW_INDEXED,
    RC_GFX_OP_PUSH_DEBUG_GROUP,
    RC_GFX_OP_POP_DEBUG_GROUP,
} rc_gfx_op;

typedef struct rc_gfx_cmd_set_pipeline      { rc_gfx_pipeline pipeline; } rc_gfx_cmd_set_pipeline;

typedef struct rc_gfx_cmd_set_bind_group {
    uint32_t          group_index;
    rc_gfx_bind_group group;
    uint32_t          offset_count;
} rc_gfx_cmd_set_bind_group;

typedef struct rc_gfx_cmd_set_vertex_buffer {
    uint32_t      slot;
    rc_gfx_buffer buffer;
    uint32_t      offset;
} rc_gfx_cmd_set_vertex_buffer;

typedef struct rc_gfx_cmd_set_index_buffer {
    rc_gfx_buffer buffer;
    uint32_t      format;
    uint32_t      offset;
} rc_gfx_cmd_set_index_buffer;

typedef struct rc_gfx_cmd_set_viewport {
    rc_box2i rect;
    float    min_depth;
    float    max_depth;
} rc_gfx_cmd_set_viewport;
typedef struct rc_gfx_cmd_set_scissor       { rc_box2i rect; } rc_gfx_cmd_set_scissor;
typedef struct rc_gfx_cmd_set_blend_constant { rc_vec4f color; } rc_gfx_cmd_set_blend_constant;
typedef struct rc_gfx_cmd_set_stencil_reference { uint32_t reference; } rc_gfx_cmd_set_stencil_reference;

/* Sequential reader over a finished stream, shared by playback and dump. */
typedef struct rc_gfx_cmd_reader {
    const uint8_t *p;
    const uint8_t *end;
} rc_gfx_cmd_reader;

static inline bool rc_gfx_cmd_reader_done(const rc_gfx_cmd_reader *r)
{
    return r->p >= r->end;
}

static inline void rc_gfx_cmd_read(rc_gfx_cmd_reader *r, void *out, uint32_t size)
{
    RC_ASSERT(r->p + size <= r->end);
    memcpy(out, r->p, size);
    r->p += size;
}

static inline uint32_t rc_gfx_cmd_read_u32(rc_gfx_cmd_reader *r)
{
    uint32_t v;
    rc_gfx_cmd_read(r, &v, (uint32_t)sizeof(v));
    return v;
}

/* ---- internal objects ---- */

/* Genpool payloads.  Each slot carries its own generation (the genpool slot
 * keeps it outside the free-list union), so the frontend needs no parallel
 * bookkeeping; the gl_* members are GL object names (uint32_t everywhere GL
 * is concerned). */

typedef struct rc_gfx_buffer_obj {
    bool                 is_ring;    /* the uniform ring; destroy/update guard */
    uint32_t             size;
    uint32_t             usage;
    rc_gfx_buffer_update_mode update;
    uint32_t             gl_buffer;
} rc_gfx_buffer_obj;

typedef struct rc_gfx_texture_obj {
    rc_gfx_texture_dim    dim;
    rc_gfx_texture_format format;
    rc_vec2i              size;
    uint32_t              depth;
    uint32_t              mip_count;
    uint32_t              sample_count;   /* >= 1 once resolved */
    uint32_t              usage;
    uint32_t              gl_texture;
    uint32_t              gl_target;
} rc_gfx_texture_obj;

typedef struct rc_gfx_sampler_obj {
    bool     comparison;
    uint32_t gl_sampler;
} rc_gfx_sampler_obj;

typedef struct rc_gfx_shader_block_info {
    uint32_t group;
    uint32_t binding;
    uint32_t size;
    uint32_t gl_block_index;
} rc_gfx_shader_block_info;

typedef struct rc_gfx_shader_pair_info {
    uint32_t texture_group;
    uint32_t texture_binding;
    uint32_t sampler_group;
    uint32_t sampler_binding;
    int32_t  gl_location;
} rc_gfx_shader_pair_info;

typedef struct rc_gfx_shader_obj {
    uint32_t                  gl_program;
    uint32_t                  block_count;
    uint32_t                  pair_count;
    rc_gfx_shader_block_info *blocks;    /* in the gfx arena */
    rc_gfx_shader_pair_info  *pairs;
    uint64_t                  resolved_hash;  /* flat mapping the program was resolved
                                                 against; 0 = not yet resolved */
} rc_gfx_shader_obj;

typedef struct rc_gfx_bind_group_layout_obj {
    uint32_t                       entry_count;
    rc_gfx_bind_group_layout_entry entries[RC_GFX_MAX_BINDINGS_PER_GROUP];
} rc_gfx_bind_group_layout_obj;

typedef struct rc_gfx_pipeline_layout_obj {
    uint32_t                 group_count;
    rc_gfx_bind_group_layout groups[RC_GFX_MAX_BIND_GROUPS];
    bool                     owns_group0;    /* created by rc_gfx_simple_layout_make */
    /* flat GL units per group entry, parallel to each group layout's entries
     * array: UBO binding points for UNIFORM_BUFFER entries, texture units for
     * TEXTURE / STORAGE_BUFFER_READ entries, unused for samplers */
    uint8_t  flat_unit[RC_GFX_MAX_BIND_GROUPS][RC_GFX_MAX_BINDINGS_PER_GROUP];
    uint32_t ubo_count;
    uint32_t tex_count;
    uint64_t mapping_hash;   /* hash of the flat assignment, for compatibility checks */
} rc_gfx_pipeline_layout_obj;

typedef struct rc_gfx_bind_entry_resolved {
    uint32_t            binding;
    rc_gfx_binding_type type;
    bool                has_dynamic_offset;
    uint32_t            gl_object;   /* buffer / texture / sampler name; the TBO
                                        texture for STORAGE_BUFFER_READ */
    uint32_t            gl_target;   /* texture target for TEXTURE / storage entries */
    uint32_t            offset;      /* uniform buffer range */
    uint32_t            size;
} rc_gfx_bind_entry_resolved;

typedef struct rc_gfx_bind_group_obj {
    rc_gfx_bind_group_layout   layout;
    uint32_t                   entry_count;
    uint32_t                   dynamic_count;
    rc_gfx_bind_entry_resolved entries[RC_GFX_MAX_BINDINGS_PER_GROUP];
} rc_gfx_bind_group_obj;

typedef struct rc_gfx_pipeline_obj {
    rc_gfx_shader              shader;
    rc_gfx_pipeline_layout     layout;
    uint32_t                   gl_program;
    rc_gfx_primitive           primitive;
    rc_gfx_index_format        index_format;
    rc_gfx_cull                cull;
    rc_gfx_front_face          front_face;
    uint32_t                   color_count;
    rc_gfx_color_target_state  colors[RC_GFX_MAX_COLOR_ATTACHMENTS];
    rc_gfx_depth_stencil_state depth_stencil;
    uint32_t                   sample_count;   /* resolved; >= 1 */
    bool                       alpha_to_coverage;
    /* resolved vertex layout: attribute offsets and buffer strides computed */
    uint32_t                   attr_count;
    rc_gfx_vertex_attribute    attrs[RC_GFX_MAX_VERTEX_ATTRIBUTES];
    uint32_t                   strides[RC_GFX_MAX_VERTEX_BUFFERS];
    uint32_t                   divisors[RC_GFX_MAX_VERTEX_BUFFERS];  /* 0 = per-vertex */
    uint32_t                   vbuf_mask;      /* referenced vertex buffer slots */
} rc_gfx_pipeline_obj;

typedef struct rc_gfx_render_target_obj {
    rc_vec2i              size;
    uint32_t              color_count;
    rc_gfx_texture_format color_formats[RC_GFX_MAX_COLOR_ATTACHMENTS];
    rc_gfx_texture_format depth_format;    /* NONE if no depth attachment */
    uint32_t              sample_count;
    uint32_t              resolve_mask;    /* colour attachments with a resolve destination */
    uint32_t              gl_fbo;
    uint32_t              gl_resolve_fbo;  /* 0 when resolve_mask == 0 */
} rc_gfx_render_target_obj;

/* ---- frontend services used by the backend ---- */

/* Resolve a handle to its live object; asserts validity and generation. */
rc_gfx_buffer_obj            *rc_gfx_resolve_buffer(rc_gfx_buffer handle);
rc_gfx_texture_obj           *rc_gfx_resolve_texture(rc_gfx_texture handle);
rc_gfx_sampler_obj           *rc_gfx_resolve_sampler(rc_gfx_sampler handle);
rc_gfx_shader_obj            *rc_gfx_resolve_shader(rc_gfx_shader handle);
rc_gfx_bind_group_layout_obj *rc_gfx_resolve_bind_group_layout(rc_gfx_bind_group_layout handle);
rc_gfx_bind_group_obj        *rc_gfx_resolve_bind_group(rc_gfx_bind_group handle);
rc_gfx_pipeline_layout_obj   *rc_gfx_resolve_pipeline_layout(rc_gfx_pipeline_layout handle);
rc_gfx_pipeline_obj          *rc_gfx_resolve_pipeline(rc_gfx_pipeline handle);
rc_gfx_render_target_obj     *rc_gfx_resolve_render_target(rc_gfx_render_target handle);

/* The gfx persistent arena (backend caches allocate from it too). */
rc_arena *rc_gfx_internal_arena(void);

/* Whether the extra validation layer is active. */
bool rc_gfx_internal_validation(void);

/* ---- backend interface (implemented once per backend) ---- */

/* Kinds of raw GL object the deferred-destruction queue can release. */
typedef enum rc_gfx_release_kind {
    RC_GFX_RELEASE_BUFFER,
    RC_GFX_RELEASE_TEXTURE,
    RC_GFX_RELEASE_SAMPLER,
    RC_GFX_RELEASE_PROGRAM,
    RC_GFX_RELEASE_FBO,
} rc_gfx_release_kind;

/* Device lifecycle.  init probes features/limits, creates the present
 * program and the uniform ring GL buffer, and applies the global GL state. */
rc_gfx_features rc_gfx_backend_init(uint32_t ring_size_total, uint32_t *out_ring_buffer);
void            rc_gfx_backend_shutdown(void);
rc_gfx_limits   rc_gfx_backend_limits(void);
uint32_t        rc_gfx_backend_format_caps(rc_gfx_texture_format fmt);
rc_str          rc_gfx_backend_name(void);

/* Frame lifecycle.  begin_frame (re)creates the swapchain target on size
 * change; end_frame resolves a multisampled swapchain and runs the present
 * draw into the default framebuffer. */
void rc_gfx_backend_begin_frame(rc_vec2i size, rc_gfx_texture_format format, uint32_t sample_count);
void rc_gfx_backend_end_frame(rc_gfx_color_space color_space);

/* Resources.  Each fills in the gl_* members of the given object. */
void rc_gfx_backend_make_buffer(rc_gfx_buffer_obj *obj, rc_view_bytes data);
void rc_gfx_backend_update_buffer(rc_gfx_buffer_obj *obj, uint32_t offset, rc_view_bytes data);
void rc_gfx_backend_make_texture(rc_gfx_texture_obj *obj, const rc_gfx_texture_desc *desc);
void rc_gfx_backend_update_texture(rc_gfx_texture_obj *obj, uint32_t mip, uint32_t slice,
                                   rc_box2i region, rc_view_bytes data);
void rc_gfx_backend_generate_mipmaps(rc_gfx_texture_obj *obj);
void rc_gfx_backend_make_sampler(rc_gfx_sampler_obj *obj, const rc_gfx_sampler_desc *desc);
void rc_gfx_backend_make_shader(rc_gfx_shader_obj *obj, const rc_gfx_shader_desc *desc);
void rc_gfx_backend_make_render_target(rc_gfx_render_target_obj *obj,
                                       const rc_gfx_render_target_desc *desc);

/* Create the texture buffer object backing a STORAGE_BUFFER_READ binding;
 * returns the GL texture name (released as RC_GFX_RELEASE_TEXTURE). */
uint32_t rc_gfx_backend_make_tbo(uint32_t gl_buffer, rc_gfx_texture_format texel_format);

/* Bind the shader's uniform blocks and sampler uniforms to the flat GL units
 * of the given pipeline layout.  Called at pipeline creation. */
void rc_gfx_backend_resolve_shader(rc_gfx_shader_obj *shd,
                                   const rc_gfx_pipeline_layout_obj *layout);

/* Immediate eviction hooks, called at destroy time (before the deferred
 * release) so caches drop entries that reference the object. */
void rc_gfx_backend_evict_buffer(rc_gfx_buffer buffer);
void rc_gfx_backend_evict_pipeline(rc_gfx_pipeline pipeline);

/* Deferred release of a raw GL object, called RC_GFX_FRAMES_IN_FLIGHT frames
 * after the destroy. */
void rc_gfx_backend_release(rc_gfx_release_kind kind, uint32_t name);

/* Upload freshly written uniform-ring bytes (offset is ring-absolute). */
void rc_gfx_backend_flush_uniforms(uint32_t offset, rc_view_bytes data);

/* Play back one finished command stream. */
void rc_gfx_backend_submit(rc_view_bytes stream);

#endif /* RC_GFX_INTERNAL_H_ */
