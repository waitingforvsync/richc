/*
 * gfx/bindings.h - the binding model: bind group layouts, pipeline layouts,
 * and bind groups.
 *
 * Resources reach shaders through bind groups: immutable bundles of buffers,
 * textures and samplers created once against a layout and rebound cheaply.
 * A pipeline layout is the ordered set of bind group layouts a pipeline uses.
 * Group indices are by update frequency; every renderer built on the layer
 * should follow this convention:
 *
 *   group 0 - camera, time, global lighting     (rebound once per frame)
 *   group 1 - per-pass state                    (once per pass)
 *   group 2 - material textures and constants   (per material)
 *   group 3 - per-draw data via dynamic offsets (per draw)
 *
 * Types
 * -----
 *   rc_gfx_binding_type              - UNIFORM_BUFFER, TEXTURE, SAMPLER,
 *                                      COMPARISON_SAMPLER, STORAGE_BUFFER_READ
 *   rc_gfx_bind_group_layout_entry / rc_gfx_bind_group_layout_desc
 *   rc_gfx_pipeline_layout_desc
 *   rc_gfx_simple_layout             - one-group convenience bundle
 *   rc_gfx_bind_group_entry / rc_gfx_bind_group_desc
 *
 * Functions
 * ---------
 *   rc_gfx_bind_group_layout_make(desc) / rc_gfx_bind_group_layout_destroy(l)
 *   rc_gfx_pipeline_layout_make(desc)   / rc_gfx_pipeline_layout_destroy(l)
 *   rc_gfx_simple_layout_make(entries, entry_count, label)
 *   rc_gfx_bind_group_make(desc)        / rc_gfx_bind_group_destroy(g)
 */

#ifndef RC_GFX_BINDINGS_H_
#define RC_GFX_BINDINGS_H_

#include "richc/gfx/gfx.h"
#include "richc/gfx/texture.h"

typedef enum rc_gfx_binding_type {
    RC_GFX_BINDING_UNIFORM_BUFFER = 0,
    RC_GFX_BINDING_TEXTURE,
    RC_GFX_BINDING_SAMPLER,
    RC_GFX_BINDING_COMPARISON_SAMPLER,
    RC_GFX_BINDING_STORAGE_BUFFER_READ,   /* TBO on GL 3.3, SSBO elsewhere */
} rc_gfx_binding_type;

/* ---- bind group layouts ---- */

typedef struct rc_gfx_bind_group_layout_entry {
    uint32_t             binding;        /* index within the group */
    uint32_t             visibility;     /* rc_gfx_stage flags */
    rc_gfx_binding_type  type;

    /* type == UNIFORM_BUFFER */
    bool                 has_dynamic_offset;
    uint32_t             min_binding_size;   /* 0 = unvalidated; set it, it catches std140 bugs */

    /* type == TEXTURE */
    rc_gfx_texture_dim         texture_dim;
    rc_gfx_texture_sample_type sample_type;
    bool                       multisampled;

    /* type == STORAGE_BUFFER_READ (GL 3.3 backs this with a TBO) */
    rc_gfx_texture_format      texel_format;
} rc_gfx_bind_group_layout_entry;

typedef struct rc_gfx_bind_group_layout_desc {
    rc_gfx_bind_group_layout_entry entries[RC_GFX_MAX_BINDINGS_PER_GROUP];
    uint32_t                       entry_count;
    rc_str                         label;
} rc_gfx_bind_group_layout_desc;

rc_gfx_bind_group_layout rc_gfx_bind_group_layout_make(const rc_gfx_bind_group_layout_desc *desc);
void                     rc_gfx_bind_group_layout_destroy(rc_gfx_bind_group_layout layout);

/* ---- pipeline layouts ---- */

typedef struct rc_gfx_pipeline_layout_desc {
    rc_gfx_bind_group_layout groups[RC_GFX_MAX_BIND_GROUPS];
    uint32_t                 group_count;
    rc_str                   label;
} rc_gfx_pipeline_layout_desc;

rc_gfx_pipeline_layout rc_gfx_pipeline_layout_make(const rc_gfx_pipeline_layout_desc *desc);
void                   rc_gfx_pipeline_layout_destroy(rc_gfx_pipeline_layout layout);

/*
 * Helper for the common case: build a layout from a single group's entries in
 * one call.  Creates and owns the bind group layout internally; destroying
 * the pipeline layout destroys it.  Returns both so bind groups can be
 * created against the layout.
 */
typedef struct rc_gfx_simple_layout {
    rc_gfx_pipeline_layout   layout;
    rc_gfx_bind_group_layout group0;
} rc_gfx_simple_layout;

rc_gfx_simple_layout rc_gfx_simple_layout_make(
    const rc_gfx_bind_group_layout_entry *entries, uint32_t entry_count, rc_str label);

/* ---- bind groups ---- */

typedef struct rc_gfx_bind_group_entry {
    uint32_t binding;
    /* exactly one of these is used, per the layout's type for this binding */
    rc_gfx_buffer  buffer;
    uint32_t       buffer_offset;    /* base offset; dynamic offsets add to this */
    uint32_t       buffer_size;      /* 0 => rest of buffer */
    rc_gfx_texture texture;
    rc_gfx_sampler sampler;
} rc_gfx_bind_group_entry;

typedef struct rc_gfx_bind_group_desc {
    rc_gfx_bind_group_layout layout;                            /* required */
    rc_gfx_bind_group_entry  entries[RC_GFX_MAX_BINDINGS_PER_GROUP];
    uint32_t                 entry_count;
    rc_str                   label;
} rc_gfx_bind_group_desc;

/* Bind groups are immutable; a material creates one once at load time. */
rc_gfx_bind_group rc_gfx_bind_group_make(const rc_gfx_bind_group_desc *desc);
void              rc_gfx_bind_group_destroy(rc_gfx_bind_group group);

#endif /* RC_GFX_BINDINGS_H_ */
