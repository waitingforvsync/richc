/*
 * gfx/encoder.h - command recording, submission, and per-draw uniforms.
 *
 * Commands are recorded into an arena-backed encoder as a packed opcode
 * stream, then submitted for playback.  Recording touches no shared device
 * state, so encoders may be used from any thread (each thread with its own
 * arena and encoder); resource creation and rc_gfx_submit must happen on the
 * thread that owns the GL context.  A command buffer stays valid until its
 * arena is reset - typical use is a per-frame arena reset at begin frame.
 *
 * Uniforms
 * --------
 * There are no loose uniforms; per-draw constants come from a per-frame
 * uniform ring bound with dynamic offsets.  The ring is a single buffer with
 * RC_GFX_FRAMES_IN_FLIGHT rotating regions, so a bind group referencing
 * rc_gfx_uniform_buffer() is created once and reused forever; the offset
 * returned by rc_gfx_encoder_alloc_uniforms selects the live allocation.
 *
 * Types
 * -----
 *   rc_gfx_encoder (opaque), rc_gfx_cmd_buffer
 *   rc_gfx_uniform_alloc
 *   rc_gfx_draw_desc, rc_gfx_draw_indexed_desc
 *
 * Functions
 * ---------
 *   rc_gfx_encoder_begin(arena) / rc_gfx_encoder_finish(enc) / rc_gfx_submit()
 *   rc_gfx_encoder_pass_begin(enc, desc) / rc_gfx_encoder_pass_end(enc)
 *   rc_gfx_encoder_set_pipeline / set_bind_group / set_vertex_buffer / set_index_buffer
 *   rc_gfx_encoder_set_viewport / set_scissor / set_blend_constant / set_stencil_reference
 *   rc_gfx_encoder_draw / rc_gfx_encoder_draw_indexed
 *   rc_gfx_uniform_buffer() / rc_gfx_encoder_alloc_uniforms(enc, size)
 *   rc_gfx_encoder_push_debug_group / rc_gfx_encoder_pop_debug_group
 *   rc_gfx_cmd_buffer_dump(cb, arena)
 */

#ifndef RC_GFX_ENCODER_H_
#define RC_GFX_ENCODER_H_

#include "richc/gfx/gfx.h"
#include "richc/gfx/pass.h"
#include "richc/math/box2i.h"
#include "richc/mstr.h"

typedef struct rc_gfx_encoder rc_gfx_encoder;   /* opaque, arena-allocated */

typedef struct rc_gfx_cmd_buffer {
    const uint8_t *data;
    uint32_t       size;
} rc_gfx_cmd_buffer;

/* Begin recording.  The encoder and its command stream live in `arena`; give
 * each recording thread its own arena. */
rc_gfx_encoder   *rc_gfx_encoder_begin(rc_arena *arena);

/* End recording and return the finished command buffer (a view of the
 * encoder's stream; valid until the arena is reset). */
rc_gfx_cmd_buffer rc_gfx_encoder_finish(rc_gfx_encoder *enc);

/* Play back command buffers in order.  Context thread only. */
void rc_gfx_submit(const rc_gfx_cmd_buffer *buffers, uint32_t count);

/* ---- passes ---- */

void rc_gfx_encoder_pass_begin(rc_gfx_encoder *enc, const rc_gfx_pass_desc *desc);
void rc_gfx_encoder_pass_end  (rc_gfx_encoder *enc);

/* ---- state ---- */

void rc_gfx_encoder_set_pipeline    (rc_gfx_encoder *enc, rc_gfx_pipeline pip);
void rc_gfx_encoder_set_bind_group  (rc_gfx_encoder *enc, uint32_t group_index,
                                     rc_gfx_bind_group group,
                                     const uint32_t *dynamic_offsets, uint32_t dynamic_offset_count);
void rc_gfx_encoder_set_vertex_buffer(rc_gfx_encoder *enc, uint32_t slot,
                                      rc_gfx_buffer buf, uint32_t offset);
void rc_gfx_encoder_set_index_buffer (rc_gfx_encoder *enc, rc_gfx_buffer buf,
                                      rc_gfx_index_format fmt, uint32_t offset);

/* Viewport and scissor default to the full render target at pass_begin.
 * Rects are in pixels, origin top-left, +y down. */
void rc_gfx_encoder_set_viewport    (rc_gfx_encoder *enc, rc_box2i rect, float min_depth, float max_depth);
void rc_gfx_encoder_set_scissor     (rc_gfx_encoder *enc, rc_box2i rect);
void rc_gfx_encoder_set_blend_constant(rc_gfx_encoder *enc, rc_vec4f color);   /* linear */
void rc_gfx_encoder_set_stencil_reference(rc_gfx_encoder *enc, uint32_t reference);

/* ---- draws ---- */

typedef struct rc_gfx_draw_desc {
    uint32_t vertex_count;
    uint32_t instance_count;    /* 0 => 1 */
    uint32_t first_vertex;
    uint32_t first_instance;    /* requires features.base_instance */
} rc_gfx_draw_desc;

typedef struct rc_gfx_draw_indexed_desc {
    uint32_t index_count;
    uint32_t instance_count;    /* 0 => 1 */
    uint32_t first_index;
    int32_t  base_vertex;
    uint32_t first_instance;    /* requires features.base_instance */
} rc_gfx_draw_indexed_desc;

void rc_gfx_encoder_draw        (rc_gfx_encoder *enc, const rc_gfx_draw_desc *desc);
void rc_gfx_encoder_draw_indexed(rc_gfx_encoder *enc, const rc_gfx_draw_indexed_desc *desc);

/* ---- per-draw uniforms ---- */

/* The uniform ring's buffer handle - constant for the lifetime of the device.
 * Reference it from bind groups; select allocations with dynamic offsets. */
rc_gfx_buffer rc_gfx_uniform_buffer(void);

typedef struct rc_gfx_uniform_alloc {
    void          *ptr;      /* write your struct here; valid until end_frame */
    rc_gfx_buffer  buffer;   /* == rc_gfx_uniform_buffer(), for convenience */
    uint32_t       offset;   /* pass as a dynamic offset */
} rc_gfx_uniform_alloc;

/* Allocate size bytes from the current frame's ring region, aligned to
 * RC_GFX_UNIFORM_ALIGN.  Callable from any recording thread. */
rc_gfx_uniform_alloc rc_gfx_encoder_alloc_uniforms(rc_gfx_encoder *enc, uint32_t size);

/* ---- debug ---- */

/* Debug markers - GL_KHR_debug where available, no-ops otherwise. */
void rc_gfx_encoder_push_debug_group(rc_gfx_encoder *enc, rc_str label);
void rc_gfx_encoder_pop_debug_group (rc_gfx_encoder *enc);

/* Decode a command buffer into a human-readable listing (one command per
 * line), allocated from `arena`.  Needs no device. */
rc_mstr rc_gfx_cmd_buffer_dump(rc_gfx_cmd_buffer cb, rc_arena *arena);

#endif /* RC_GFX_ENCODER_H_ */
