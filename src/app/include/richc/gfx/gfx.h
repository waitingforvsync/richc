/*
 * gfx/gfx.h - graphics device: handles, shared vocabulary, and frame lifecycle.
 *
 * The gfx module is a backend-agnostic GPU abstraction whose object model
 * follows WebGPU (bind groups, immutable pipelines, command encoders), so that
 * the OpenGL 3.3 backend is the only one that ever has to translate.  This
 * header holds everything shared across the category headers: the handle
 * types, the texture-format vocabulary and its helper functions, the enums
 * used by more than one category, the limit macros, and the device lifecycle.
 *
 * Coordinate conventions (identical for every backend)
 * ----------------------------------------------------
 *   NDC          x right, y up, both [-1, 1]; depth [0, 1]
 *   Depth        reverse-Z: near -> 1, far -> 0; clear to 0.0, GREATER_EQUAL
 *   Viewport     origin top-left, +y down, pixel units (scissor likewise)
 *   Texture UVs  origin top-left, v down; texel (0,0) first in memory
 *   Winding      CCW front-facing by default
 *   Colour       linear throughout the shader; encoding is a format property
 *
 * Handles
 * -------
 *   rc_gfx_buffer, rc_gfx_texture, rc_gfx_sampler, rc_gfx_shader,
 *   rc_gfx_bind_group_layout, rc_gfx_bind_group, rc_gfx_pipeline_layout,
 *   rc_gfx_pipeline, rc_gfx_render_target
 * Each is a distinct struct wrapping core's rc_genpool_handle so the type
 * system prevents mixing them.  {0} is the invalid "none" handle.  Inspect a
 * handle through the rc_genpool_handle_* functions on the h member.
 *
 * Shared enums
 * ------------
 *   rc_gfx_texture_format (+ helpers), rc_gfx_compare, rc_gfx_index_format,
 *   rc_gfx_stage (bit flags), rc_gfx_color_space
 *
 * Device and frame lifecycle
 * --------------------------
 *   rc_gfx_init(desc) / rc_gfx_shutdown()
 *   rc_gfx_begin_frame(size) / rc_gfx_end_frame()
 *   rc_gfx_swapchain_size() / rc_gfx_swapchain_format()
 *
 * Capability queries
 * ------------------
 *   rc_gfx_features_query(), rc_gfx_limits_query(),
 *   rc_gfx_format_caps_query(fmt), rc_gfx_backend_name()
 *
 * A GL context must be current on the calling thread before rc_gfx_init
 * (rc_app_init establishes one); gfx never creates a context itself.
 * Resource creation and submission must happen on that thread.
 */

#ifndef RC_GFX_GFX_H_
#define RC_GFX_GFX_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/genpool_handle.h"
#include "richc/math/vec2i.h"
#include "richc/str.h"

typedef struct rc_arena rc_arena;

/* ---- handles ---- */

/*
 * Each handle wraps core's rc_genpool_handle ({slot index, generation}) in a
 * distinct one-member struct, so handles of different resource types cannot be
 * mixed.  {0} is the invalid "none" handle.  Generations are bumped on destroy,
 * so a stale handle traps on use rather than aliasing a recycled slot.
 */
typedef struct rc_gfx_buffer            { rc_genpool_handle h; } rc_gfx_buffer;
typedef struct rc_gfx_texture           { rc_genpool_handle h; } rc_gfx_texture;
typedef struct rc_gfx_sampler           { rc_genpool_handle h; } rc_gfx_sampler;
typedef struct rc_gfx_shader            { rc_genpool_handle h; } rc_gfx_shader;
typedef struct rc_gfx_bind_group_layout { rc_genpool_handle h; } rc_gfx_bind_group_layout;
typedef struct rc_gfx_bind_group        { rc_genpool_handle h; } rc_gfx_bind_group;
typedef struct rc_gfx_pipeline_layout   { rc_genpool_handle h; } rc_gfx_pipeline_layout;
typedef struct rc_gfx_pipeline          { rc_genpool_handle h; } rc_gfx_pipeline;
typedef struct rc_gfx_render_target     { rc_genpool_handle h; } rc_gfx_render_target;

/* ---- limits ---- */

#define RC_GFX_MAX_BIND_GROUPS              4
#define RC_GFX_MAX_BINDINGS_PER_GROUP      16
#define RC_GFX_MAX_VERTEX_BUFFERS           8
#define RC_GFX_MAX_VERTEX_ATTRIBUTES       16
#define RC_GFX_MAX_COLOR_ATTACHMENTS        4
#define RC_GFX_MAX_MIP_LEVELS              16
#define RC_GFX_FRAMES_IN_FLIGHT             2
#define RC_GFX_UNIFORM_ALIGN              256

/* ---- texture formats ---- */

typedef enum rc_gfx_texture_format {
    RC_GFX_TEXTURE_FORMAT_NONE = 0,
    /* 8-bit */
    RC_GFX_TEXTURE_FORMAT_R8_UNORM, RC_GFX_TEXTURE_FORMAT_RG8_UNORM,
    RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM, RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
    RC_GFX_TEXTURE_FORMAT_BGRA8_UNORM, RC_GFX_TEXTURE_FORMAT_BGRA8_SRGB,
    RC_GFX_TEXTURE_FORMAT_R8_UINT, RC_GFX_TEXTURE_FORMAT_RGBA8_UINT,
    /* 16-bit */
    RC_GFX_TEXTURE_FORMAT_R16F, RC_GFX_TEXTURE_FORMAT_RG16F, RC_GFX_TEXTURE_FORMAT_RGBA16F,
    RC_GFX_TEXTURE_FORMAT_R16_UINT, RC_GFX_TEXTURE_FORMAT_RG16_UINT,
    /* 32-bit */
    RC_GFX_TEXTURE_FORMAT_R32F, RC_GFX_TEXTURE_FORMAT_RG32F, RC_GFX_TEXTURE_FORMAT_RGBA32F,
    RC_GFX_TEXTURE_FORMAT_R32_UINT, RC_GFX_TEXTURE_FORMAT_RGBA32_UINT,
    /* packed */
    RC_GFX_TEXTURE_FORMAT_RGB10A2_UNORM, RC_GFX_TEXTURE_FORMAT_RG11B10F,
    /* depth / stencil */
    RC_GFX_TEXTURE_FORMAT_DEPTH16_UNORM, RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS,
    RC_GFX_TEXTURE_FORMAT_DEPTH32F,
    RC_GFX_TEXTURE_FORMAT_DEPTH24_PLUS_STENCIL8, RC_GFX_TEXTURE_FORMAT_DEPTH32F_STENCIL8,
    /* compressed */
    RC_GFX_TEXTURE_FORMAT_BC1_RGBA_UNORM, RC_GFX_TEXTURE_FORMAT_BC1_RGBA_SRGB,
    RC_GFX_TEXTURE_FORMAT_BC3_RGBA_UNORM, RC_GFX_TEXTURE_FORMAT_BC3_RGBA_SRGB,
    RC_GFX_TEXTURE_FORMAT_BC4_R_UNORM,    RC_GFX_TEXTURE_FORMAT_BC5_RG_UNORM,
    RC_GFX_TEXTURE_FORMAT_BC6H_RGB_FLOAT, RC_GFX_TEXTURE_FORMAT_BC7_RGBA_UNORM,
    RC_GFX_TEXTURE_FORMAT_BC7_RGBA_SRGB,
    RC_GFX_TEXTURE_FORMAT_COUNT
} rc_gfx_texture_format;

/* Format property helpers (pure table lookups; no device needed). */
bool     rc_gfx_texture_format_is_srgb(rc_gfx_texture_format fmt);
bool     rc_gfx_texture_format_is_depth(rc_gfx_texture_format fmt);
bool     rc_gfx_texture_format_is_stencil(rc_gfx_texture_format fmt);
bool     rc_gfx_texture_format_is_compressed(rc_gfx_texture_format fmt);

/* Bytes per block (per texel for uncompressed formats). */
uint32_t rc_gfx_texture_format_block_size(rc_gfx_texture_format fmt);

/* Block dimensions: 1x1 for uncompressed formats, 4x4 for BC formats. */
rc_vec2i rc_gfx_texture_format_block_dim(rc_gfx_texture_format fmt);

/* The linear counterpart of an sRGB format, and vice versa; identity when the
 * format has no counterpart. */
rc_gfx_texture_format rc_gfx_texture_format_to_linear(rc_gfx_texture_format fmt);
rc_gfx_texture_format rc_gfx_texture_format_to_srgb(rc_gfx_texture_format fmt);

/* ---- shared enums ---- */

typedef enum rc_gfx_compare {
    RC_GFX_COMPARE_ALWAYS = 0,        /* default: no rejection */
    RC_GFX_COMPARE_NEVER, RC_GFX_COMPARE_LESS, RC_GFX_COMPARE_EQUAL,
    RC_GFX_COMPARE_LESS_EQUAL, RC_GFX_COMPARE_GREATER, RC_GFX_COMPARE_NOT_EQUAL,
    RC_GFX_COMPARE_GREATER_EQUAL,
} rc_gfx_compare;

typedef enum rc_gfx_index_format {
    RC_GFX_INDEX_FORMAT_NONE = 0,
    RC_GFX_INDEX_FORMAT_U16, RC_GFX_INDEX_FORMAT_U32
} rc_gfx_index_format;

/* Shader stage bit flags (bind group entry visibility). */
enum rc_gfx_stage {
    RC_GFX_STAGE_VERTEX   = 1u << 0,
    RC_GFX_STAGE_FRAGMENT = 1u << 1,
};

/*
 * Swapchain colour space.  SRGB (the default) presents through an 8-bit
 * sRGB-encoded surface; the encode is applied by fixed-function hardware on
 * write, never in a shader.  LINEAR presents raw 8-bit values with no encode -
 * only for bit-exact output (an emulator's framebuffer, pixel-exact test
 * harnesses) or a pipeline applying its own display transform.
 */
typedef enum rc_gfx_color_space {
    RC_GFX_COLOR_SPACE_SRGB = 0,
    RC_GFX_COLOR_SPACE_LINEAR,
} rc_gfx_color_space;

/* ---- device ---- */

typedef struct rc_gfx_desc {
    rc_arena          *arena;         /* arena for gfx's persistent allocations; required */
    rc_gfx_color_space color_space;   /* default SRGB */
    uint32_t           swapchain_sample_count;  /* 0 or 1 => no MSAA */
    uint32_t           uniform_ring_size;       /* bytes per in-flight frame; 0 => 1 MB */
    bool               validation;    /* extra load-time checks; forced on in debug builds */
} rc_gfx_desc;

/* Initialise the device.  A GL context must be current on this thread. */
void rc_gfx_init(const rc_gfx_desc *desc);

/* Destroy any objects still alive and release the device. */
void rc_gfx_shutdown(void);

/* Call once per frame before recording.  size is the current framebuffer
 * size in pixels; the swapchain target is recreated internally on change. */
void rc_gfx_begin_frame(rc_vec2i size);

/* Run the present pass into the default framebuffer and retire deferred
 * destructions.  Does not swap buffers - that stays with the app layer. */
void rc_gfx_end_frame(void);

rc_vec2i              rc_gfx_swapchain_size(void);
rc_gfx_texture_format rc_gfx_swapchain_format(void);

/* ---- features and limits ---- */

typedef struct rc_gfx_features {
    bool compute;                  /* GL 4.3; unsupported on the GL 3.3 backend */
    bool storage_buffers;          /* GL 4.3 SSBO */
    bool storage_buffers_via_tbo;  /* GL 3.3 path for STORAGE_BUFFER_READ */
    bool base_instance;            /* GL 4.2; first_instance must be 0 without it */
    bool indirect_draw;            /* GL 4.0 */
    bool native_depth_zero_to_one; /* ARB_clip_control present */
    bool cube_map_array;           /* GL 4.0 */
    bool texture_view;             /* GL 4.3 */
    bool anisotropic_filtering;
    bool srgb_default_framebuffer; /* the default FB reports sRGB encoding */
    bool debug_markers;            /* GL_KHR_debug */
    bool timer_queries;
} rc_gfx_features;

typedef struct rc_gfx_limits {
    uint32_t max_texture_size_2d;
    uint32_t max_texture_size_3d;
    uint32_t max_texture_size_cube;
    uint32_t max_texture_array_layers;
    uint32_t max_color_attachments;
    uint32_t max_vertex_attributes;
    uint32_t max_uniform_buffer_range;
    uint32_t uniform_buffer_offset_alignment;
    uint32_t max_msaa_samples;
    float    max_anisotropy;
} rc_gfx_limits;

typedef enum rc_gfx_format_caps {
    RC_GFX_FORMAT_CAP_SAMPLE  = 1u << 0,
    RC_GFX_FORMAT_CAP_FILTER  = 1u << 1,
    RC_GFX_FORMAT_CAP_RENDER  = 1u << 2,
    RC_GFX_FORMAT_CAP_BLEND   = 1u << 3,
    RC_GFX_FORMAT_CAP_MSAA    = 1u << 4,
    RC_GFX_FORMAT_CAP_RESOLVE = 1u << 5,
} rc_gfx_format_caps;

rc_gfx_features rc_gfx_features_query(void);
rc_gfx_limits   rc_gfx_limits_query(void);
uint32_t        rc_gfx_format_caps_query(rc_gfx_texture_format fmt);
rc_str          rc_gfx_backend_name(void);

#endif /* RC_GFX_GFX_H_ */
