/*
 * gfx/pipeline.h - render pipelines: vertex layout, fixed-function state, and
 * the pipeline object that bundles them with a shader and a layout.
 *
 * Colour target formats, the depth format and the sample count are baked into
 * the pipeline because D3D12 and Vulkan PSOs require them; they are validated
 * against the render target when the pipeline is bound inside a pass.
 *
 * Types
 * -----
 *   rc_gfx_vertex_format, rc_gfx_primitive, rc_gfx_cull, rc_gfx_front_face
 *   rc_gfx_stencil_op, rc_gfx_blend_factor, rc_gfx_blend_op
 *   rc_gfx_vertex_buffer_layout / rc_gfx_vertex_attribute / rc_gfx_vertex_layout
 *   rc_gfx_stencil_face / rc_gfx_depth_stencil_state
 *   rc_gfx_blend_state / rc_gfx_color_target_state
 *   rc_gfx_pipeline_desc
 *
 * Functions
 * ---------
 *   rc_gfx_pipeline_make(desc) / rc_gfx_pipeline_destroy(pip)
 *   rc_gfx_blend_state_make_alpha() / rc_gfx_blend_state_make_premultiplied() / rc_gfx_blend_state_make_additive()
 *
 * Prefer premultiplied alpha for anything composited more than once; it is
 * the only form that composes associatively, and it interacts correctly with
 * linear blending and with mipmapped sprite atlases.
 */

#ifndef RC_GFX_PIPELINE_H_
#define RC_GFX_PIPELINE_H_

#include "richc/gfx/gfx.h"

/* ---- enums ---- */

typedef enum rc_gfx_vertex_format {
    RC_GFX_VERTEX_FORMAT_NONE = 0,
    RC_GFX_VERTEX_FORMAT_U8X2,  RC_GFX_VERTEX_FORMAT_U8X4,
    RC_GFX_VERTEX_FORMAT_I8X2,  RC_GFX_VERTEX_FORMAT_I8X4,
    RC_GFX_VERTEX_FORMAT_U8X2_NORM, RC_GFX_VERTEX_FORMAT_U8X4_NORM,
    RC_GFX_VERTEX_FORMAT_I8X2_NORM, RC_GFX_VERTEX_FORMAT_I8X4_NORM,
    RC_GFX_VERTEX_FORMAT_U16X2, RC_GFX_VERTEX_FORMAT_U16X4,
    RC_GFX_VERTEX_FORMAT_I16X2, RC_GFX_VERTEX_FORMAT_I16X4,
    RC_GFX_VERTEX_FORMAT_U16X2_NORM, RC_GFX_VERTEX_FORMAT_U16X4_NORM,
    RC_GFX_VERTEX_FORMAT_I16X2_NORM, RC_GFX_VERTEX_FORMAT_I16X4_NORM,
    RC_GFX_VERTEX_FORMAT_F16X2, RC_GFX_VERTEX_FORMAT_F16X4,
    RC_GFX_VERTEX_FORMAT_F32,   RC_GFX_VERTEX_FORMAT_F32X2,
    RC_GFX_VERTEX_FORMAT_F32X3, RC_GFX_VERTEX_FORMAT_F32X4,
    RC_GFX_VERTEX_FORMAT_U32,   RC_GFX_VERTEX_FORMAT_U32X2,
    RC_GFX_VERTEX_FORMAT_U32X3, RC_GFX_VERTEX_FORMAT_U32X4,
    RC_GFX_VERTEX_FORMAT_I32,   RC_GFX_VERTEX_FORMAT_I32X2,
    RC_GFX_VERTEX_FORMAT_I32X3, RC_GFX_VERTEX_FORMAT_I32X4,
    RC_GFX_VERTEX_FORMAT_RGB10A2_NORM,
    RC_GFX_VERTEX_FORMAT_COUNT
} rc_gfx_vertex_format;

typedef enum rc_gfx_primitive {
    RC_GFX_PRIMITIVE_TRIANGLES = 0,   /* default */
    RC_GFX_PRIMITIVE_TRIANGLE_STRIP,
    RC_GFX_PRIMITIVE_LINES, RC_GFX_PRIMITIVE_LINE_STRIP,
    RC_GFX_PRIMITIVE_POINTS,
} rc_gfx_primitive;

typedef enum rc_gfx_stencil_op {
    RC_GFX_STENCIL_OP_KEEP = 0, RC_GFX_STENCIL_OP_ZERO, RC_GFX_STENCIL_OP_REPLACE,
    RC_GFX_STENCIL_OP_INVERT, RC_GFX_STENCIL_OP_INCR_CLAMP, RC_GFX_STENCIL_OP_DECR_CLAMP,
    RC_GFX_STENCIL_OP_INCR_WRAP, RC_GFX_STENCIL_OP_DECR_WRAP,
} rc_gfx_stencil_op;

typedef enum rc_gfx_blend_factor {
    RC_GFX_BLEND_FACTOR_ZERO = 0, RC_GFX_BLEND_FACTOR_ONE,
    RC_GFX_BLEND_FACTOR_SRC, RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC,
    RC_GFX_BLEND_FACTOR_SRC_ALPHA, RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    RC_GFX_BLEND_FACTOR_DST, RC_GFX_BLEND_FACTOR_ONE_MINUS_DST,
    RC_GFX_BLEND_FACTOR_DST_ALPHA, RC_GFX_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    RC_GFX_BLEND_FACTOR_SRC_ALPHA_SATURATED,
    RC_GFX_BLEND_FACTOR_CONSTANT, RC_GFX_BLEND_FACTOR_ONE_MINUS_CONSTANT,
} rc_gfx_blend_factor;

typedef enum rc_gfx_blend_op {
    RC_GFX_BLEND_OP_ADD = 0, RC_GFX_BLEND_OP_SUBTRACT,
    RC_GFX_BLEND_OP_REVERSE_SUBTRACT, RC_GFX_BLEND_OP_MIN, RC_GFX_BLEND_OP_MAX,
} rc_gfx_blend_op;

typedef enum rc_gfx_cull {
    RC_GFX_CULL_NONE = 0, RC_GFX_CULL_FRONT, RC_GFX_CULL_BACK
} rc_gfx_cull;

typedef enum rc_gfx_front_face {
    RC_GFX_FRONT_FACE_CCW = 0, RC_GFX_FRONT_FACE_CW
} rc_gfx_front_face;

/* Colour write mask: bits DISABLE a channel, so 0 means "write RGBA" and the
 * {0}-is-valid-default rule holds. */
enum rc_gfx_color_mask {
    RC_GFX_COLOR_MASK_DISABLE_R = 1u << 0,
    RC_GFX_COLOR_MASK_DISABLE_G = 1u << 1,
    RC_GFX_COLOR_MASK_DISABLE_B = 1u << 2,
    RC_GFX_COLOR_MASK_DISABLE_A = 1u << 3,
    RC_GFX_COLOR_MASK_DISABLE_ALL = 0xF,
};

/* ---- vertex layout ---- */

typedef struct rc_gfx_vertex_buffer_layout {
    uint32_t stride;         /* 0 => computed from the attributes for this buffer */
    bool     per_instance;   /* false => per-vertex */
    uint32_t step_rate;      /* instance divisor; 0 => 1 */
} rc_gfx_vertex_buffer_layout;

typedef struct rc_gfx_vertex_attribute {
    uint32_t             location;      /* matches layout(location=) in GLSL */
    uint32_t             buffer_index;
    uint32_t             offset;        /* 0 => packed after the previous attribute */
    rc_gfx_vertex_format format;
} rc_gfx_vertex_attribute;

typedef struct rc_gfx_vertex_layout {
    rc_gfx_vertex_buffer_layout buffers[RC_GFX_MAX_VERTEX_BUFFERS];
    rc_gfx_vertex_attribute     attributes[RC_GFX_MAX_VERTEX_ATTRIBUTES];
} rc_gfx_vertex_layout;

/* ---- depth / stencil state ---- */

typedef struct rc_gfx_stencil_face {
    rc_gfx_compare    compare;        /* default ALWAYS */
    rc_gfx_stencil_op fail_op;
    rc_gfx_stencil_op depth_fail_op;
    rc_gfx_stencil_op pass_op;
} rc_gfx_stencil_face;

typedef struct rc_gfx_depth_stencil_state {
    rc_gfx_texture_format format;            /* NONE => no depth/stencil attachment */
    bool                  depth_write;
    rc_gfx_compare        depth_compare;     /* default ALWAYS; GREATER_EQUAL for reverse-Z */
    bool                  stencil_enabled;
    rc_gfx_stencil_face   stencil_front;
    rc_gfx_stencil_face   stencil_back;
    uint8_t               stencil_read_mask;    /* 0 => 0xFF */
    uint8_t               stencil_write_mask;   /* 0 => 0xFF */
    float                 depth_bias;
    float                 depth_bias_slope_scale;
    float                 depth_bias_clamp;
} rc_gfx_depth_stencil_state;

/* ---- blend / colour target state ---- */

typedef struct rc_gfx_blend_state {
    bool                enabled;
    rc_gfx_blend_factor src_factor;
    rc_gfx_blend_factor dst_factor;
    rc_gfx_blend_op     op;
    rc_gfx_blend_factor src_alpha_factor;
    rc_gfx_blend_factor dst_alpha_factor;
    rc_gfx_blend_op     alpha_op;
} rc_gfx_blend_state;

typedef struct rc_gfx_color_target_state {
    rc_gfx_texture_format format;       /* required for each active target */
    rc_gfx_blend_state    blend;
    uint8_t               write_mask;   /* rc_gfx_color_mask DISABLE bits; 0 = write RGBA */
} rc_gfx_color_target_state;

/* ---- pipeline ---- */

typedef struct rc_gfx_pipeline_desc {
    rc_gfx_shader              shader;         /* required */
    rc_gfx_pipeline_layout     layout;         /* required */
    rc_gfx_vertex_layout       vertex_layout;
    rc_gfx_primitive           primitive;
    rc_gfx_index_format        index_format;   /* NONE => non-indexed only */
    rc_gfx_cull                cull;
    rc_gfx_front_face          front_face;
    rc_gfx_color_target_state  colors[RC_GFX_MAX_COLOR_ATTACHMENTS];
    uint32_t                   color_count;
    rc_gfx_depth_stencil_state depth_stencil;
    uint32_t                   sample_count;   /* 0 or 1 => no MSAA */
    bool                       alpha_to_coverage;
    rc_str                     label;
} rc_gfx_pipeline_desc;

rc_gfx_pipeline rc_gfx_pipeline_make(const rc_gfx_pipeline_desc *desc);
void            rc_gfx_pipeline_destroy(rc_gfx_pipeline pip);

/* Blend-state helpers; all operate in linear space, like every blend. */
rc_gfx_blend_state rc_gfx_blend_state_make_alpha(void);          /* non-premultiplied */
rc_gfx_blend_state rc_gfx_blend_state_make_premultiplied(void);
rc_gfx_blend_state rc_gfx_blend_state_make_additive(void);

#endif /* RC_GFX_PIPELINE_H_ */
