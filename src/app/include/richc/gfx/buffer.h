/*
 * gfx/buffer.h - GPU buffers.
 *
 * Types
 * -----
 *   rc_gfx_buffer_usage   - bit flags: VERTEX, INDEX, UNIFORM, STORAGE,
 *                           COPY_SRC, COPY_DST
 *   rc_gfx_buffer_update  - IMMUTABLE (default), DYNAMIC, STREAM
 *   rc_gfx_buffer_desc    - creation descriptor
 *
 * Functions
 * ---------
 *   rc_gfx_buffer_make(desc) / rc_gfx_buffer_destroy(buf)
 *   rc_gfx_buffer_update(buf, offset, data)
 */

#ifndef RC_GFX_BUFFER_H_
#define RC_GFX_BUFFER_H_

#include "richc/bytes.h"
#include "richc/gfx/gfx.h"

/* Bit flags describing how a buffer may be bound. */
enum rc_gfx_buffer_usage {
    RC_GFX_BUFFER_USAGE_VERTEX   = 1u << 0,
    RC_GFX_BUFFER_USAGE_INDEX    = 1u << 1,
    RC_GFX_BUFFER_USAGE_UNIFORM  = 1u << 2,
    RC_GFX_BUFFER_USAGE_STORAGE  = 1u << 3,
    RC_GFX_BUFFER_USAGE_COPY_SRC = 1u << 5,
    RC_GFX_BUFFER_USAGE_COPY_DST = 1u << 6,
};

typedef enum rc_gfx_buffer_update_mode {
    RC_GFX_BUFFER_UPDATE_IMMUTABLE = 0,  /* contents supplied at creation, never changed */
    RC_GFX_BUFFER_UPDATE_DYNAMIC,        /* updated occasionally (whole or partial) */
    RC_GFX_BUFFER_UPDATE_STREAM,         /* rewritten every frame */
} rc_gfx_buffer_update_mode;

typedef struct rc_gfx_buffer_desc {
    uint32_t             size;    /* bytes; required */
    uint32_t             usage;   /* rc_gfx_buffer_usage flags; required */
    rc_gfx_buffer_update_mode update;  /* default IMMUTABLE */
    rc_view_bytes        data;    /* initial contents; required if IMMUTABLE */
    rc_str               label;   /* debug label; optional */
} rc_gfx_buffer_desc;

rc_gfx_buffer rc_gfx_buffer_make(const rc_gfx_buffer_desc *desc);
void          rc_gfx_buffer_destroy(rc_gfx_buffer buf);

/* Whole-or-partial update of a DYNAMIC or STREAM buffer.  Takes effect at the
 * point of the call in submission order; must not be called between
 * rc_gfx_encoder_pass_begin and rc_gfx_encoder_pass_end. */
void rc_gfx_buffer_update(rc_gfx_buffer buf, uint32_t offset, rc_view_bytes data);

#endif /* RC_GFX_BUFFER_H_ */
