/*
 * gfx/pass.h - render targets, attachments, and render pass descriptors.
 *
 * A render target bakes an attachment set (textures at specific mips and
 * slices); load/store actions are not baked - they are given per pass.  User
 * passes never render to the default framebuffer: a pass desc target of {0}
 * selects the internal swapchain target, and presentation samples that into
 * the window at end of frame.  The swapchain target is colour-only.
 *
 * Explicit load/store is worth stating even though it is nearly free on
 * desktop GL: it is essential on tilers, it is how MSAA resolve is expressed,
 * and DISCARD maps to glInvalidateFramebuffer.
 *
 * Types
 * -----
 *   rc_gfx_load_op   - CLEAR (default), LOAD, DISCARD
 *   rc_gfx_store_op  - STORE (default), DISCARD, RESOLVE
 *   rc_gfx_attachment / rc_gfx_render_target_desc
 *   rc_gfx_color_attachment_action / rc_gfx_depth_stencil_action
 *   rc_gfx_pass_desc
 *
 * Functions
 * ---------
 *   rc_gfx_render_target_make(desc) / rc_gfx_render_target_destroy(rt)
 *   rc_gfx_render_target_size(rt)
 */

#ifndef RC_GFX_PASS_H_
#define RC_GFX_PASS_H_

#include "richc/gfx/gfx.h"
#include "richc/math/vec4f.h"

typedef enum rc_gfx_load_op {
    RC_GFX_LOAD_OP_CLEAR = 0, RC_GFX_LOAD_OP_LOAD, RC_GFX_LOAD_OP_DISCARD
} rc_gfx_load_op;

typedef enum rc_gfx_store_op {
    RC_GFX_STORE_OP_STORE = 0, RC_GFX_STORE_OP_DISCARD, RC_GFX_STORE_OP_RESOLVE
} rc_gfx_store_op;

/* ---- render targets ---- */

typedef struct rc_gfx_attachment {
    rc_gfx_texture texture;
    uint32_t       mip;
    uint32_t       slice;        /* array layer or cube face */
} rc_gfx_attachment;

typedef struct rc_gfx_render_target_desc {
    rc_gfx_attachment colors[RC_GFX_MAX_COLOR_ATTACHMENTS];
    uint32_t          color_count;
    rc_gfx_attachment depth_stencil;
    rc_gfx_attachment resolves[RC_GFX_MAX_COLOR_ATTACHMENTS];  /* MSAA resolve destinations */
    rc_str            label;
} rc_gfx_render_target_desc;

rc_gfx_render_target rc_gfx_render_target_make(const rc_gfx_render_target_desc *desc);
void                 rc_gfx_render_target_destroy(rc_gfx_render_target rt);
rc_vec2i             rc_gfx_render_target_size(rc_gfx_render_target rt);

/* ---- passes ---- */

typedef struct rc_gfx_color_attachment_action {
    rc_gfx_load_op  load_op;
    rc_gfx_store_op store_op;
    rc_vec4f        clear_value;    /* LINEAR, even for sRGB attachments: mid grey
                                       is 0.216, not 0.5 - the hardware encodes
                                       on write */
} rc_gfx_color_attachment_action;

typedef struct rc_gfx_depth_stencil_action {
    rc_gfx_load_op  depth_load_op;
    rc_gfx_store_op depth_store_op;
    float           depth_clear_value;      /* reverse-Z: clear to 0.0 */
    rc_gfx_load_op  stencil_load_op;
    rc_gfx_store_op stencil_store_op;
    uint8_t         stencil_clear_value;
} rc_gfx_depth_stencil_action;

typedef struct rc_gfx_pass_desc {
    rc_gfx_render_target           target;   /* {0} => the swapchain target */
    rc_gfx_color_attachment_action colors[RC_GFX_MAX_COLOR_ATTACHMENTS];
    rc_gfx_depth_stencil_action    depth_stencil;
    rc_str                         label;
} rc_gfx_pass_desc;

#endif /* RC_GFX_PASS_H_ */
