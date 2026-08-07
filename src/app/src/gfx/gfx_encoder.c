/*
 * gfx_encoder.c - command encoding and the command buffer dump.
 *
 * The encoder appends a packed opcode stream to an rc_array_bytes in the
 * caller's arena: a uint32_t opcode, a memcpy'd POD payload, then any
 * variable-length tail (dynamic offsets).  No device state is touched, so
 * encoding is safe from any thread.  Playback lives in the backend;
 * rc_gfx_cmd_buffer_dump decodes the same stream into text with no device.
 */

#include "gfx_internal.h"

#include "richc/arena.h"

struct rc_gfx_encoder {
    rc_arena      *arena;
    rc_array_bytes stream;
    bool           in_pass;
    bool           finished;
    uint32_t       debug_group_depth;
};

/* ---- encoding helpers ---- */

static void enc_push_bytes(rc_gfx_encoder *enc, const void *data, uint32_t size)
{
    rc_array_bytes_append(&enc->stream, (rc_view_bytes) {.data = (const uint8_t *)data, .num = size}, enc->arena);
}

static void enc_push_cmd(rc_gfx_encoder *enc, rc_gfx_op op, const void *payload, uint32_t size)
{
    RC_ASSERT(!enc->finished);
    uint32_t op_u32 = (uint32_t)op;
    enc_push_bytes(enc, &op_u32, (uint32_t)sizeof(op_u32));
    if (size != 0) {
        enc_push_bytes(enc, payload, size);
    }
}

/* ---- lifecycle ---- */

rc_gfx_encoder *rc_gfx_encoder_begin(rc_arena *arena)
{
    RC_ASSERT(arena != NULL);
    rc_gfx_encoder *enc = rc_arena_alloc_zero(arena, (uint32_t)sizeof(rc_gfx_encoder));
    enc->arena = arena;
    return enc;
}

rc_gfx_cmd_buffer rc_gfx_encoder_finish(rc_gfx_encoder *enc)
{
    RC_ASSERT(!enc->finished);
    RC_ASSERT(!enc->in_pass);
    RC_ASSERT(enc->debug_group_depth == 0);
    enc->finished = true;
    return (rc_gfx_cmd_buffer) {.data = enc->stream.data, .size = enc->stream.num};
}

/* ---- passes ---- */

void rc_gfx_encoder_pass_begin(rc_gfx_encoder *enc, const rc_gfx_pass_desc *desc)
{
    RC_ASSERT(desc != NULL);
    RC_ASSERT(!enc->in_pass);
    enc->in_pass = true;
    enc_push_cmd(enc, RC_GFX_OP_PASS_BEGIN, desc, (uint32_t)sizeof(*desc));
}

void rc_gfx_encoder_pass_end(rc_gfx_encoder *enc)
{
    RC_ASSERT(enc->in_pass);
    enc->in_pass = false;
    enc_push_cmd(enc, RC_GFX_OP_PASS_END, NULL, 0);
}

/* ---- state ---- */

void rc_gfx_encoder_set_pipeline(rc_gfx_encoder *enc, rc_gfx_pipeline pip)
{
    RC_ASSERT(enc->in_pass);
    RC_ASSERT(!rc_genpool_handle_is_null(pip.h));
    rc_gfx_cmd_set_pipeline cmd = {.pipeline = pip};
    enc_push_cmd(enc, RC_GFX_OP_SET_PIPELINE, &cmd, (uint32_t)sizeof(cmd));
}

void rc_gfx_encoder_set_bind_group(rc_gfx_encoder *enc, uint32_t group_index,
                                   rc_gfx_bind_group group,
                                   const uint32_t *dynamic_offsets, uint32_t dynamic_offset_count)
{
    RC_ASSERT(enc->in_pass);
    RC_ASSERT(group_index < RC_GFX_MAX_BIND_GROUPS);
    RC_ASSERT(!rc_genpool_handle_is_null(group.h));
    RC_ASSERT(dynamic_offset_count == 0 || dynamic_offsets != NULL);
    rc_gfx_cmd_set_bind_group cmd = {
        .group_index = group_index,
        .group = group,
        .offset_count = dynamic_offset_count,
    };
    enc_push_cmd(enc, RC_GFX_OP_SET_BIND_GROUP, &cmd, (uint32_t)sizeof(cmd));
    if (dynamic_offset_count != 0) {
        enc_push_bytes(enc, dynamic_offsets, dynamic_offset_count * (uint32_t)sizeof(uint32_t));
    }
}

void rc_gfx_encoder_set_vertex_buffer(rc_gfx_encoder *enc, uint32_t slot,
                                      rc_gfx_buffer buf, uint32_t offset)
{
    RC_ASSERT(enc->in_pass);
    RC_ASSERT(slot < RC_GFX_MAX_VERTEX_BUFFERS);
    RC_ASSERT(!rc_genpool_handle_is_null(buf.h));
    rc_gfx_cmd_set_vertex_buffer cmd = {.slot = slot, .buffer = buf, .offset = offset};
    enc_push_cmd(enc, RC_GFX_OP_SET_VERTEX_BUFFER, &cmd, (uint32_t)sizeof(cmd));
}

void rc_gfx_encoder_set_index_buffer(rc_gfx_encoder *enc, rc_gfx_buffer buf,
                                     rc_gfx_index_format fmt, uint32_t offset)
{
    RC_ASSERT(enc->in_pass);
    RC_ASSERT(!rc_genpool_handle_is_null(buf.h));
    RC_ASSERT(fmt != RC_GFX_INDEX_FORMAT_NONE);
    rc_gfx_cmd_set_index_buffer cmd = {.buffer = buf, .format = (uint32_t)fmt, .offset = offset};
    enc_push_cmd(enc, RC_GFX_OP_SET_INDEX_BUFFER, &cmd, (uint32_t)sizeof(cmd));
}

void rc_gfx_encoder_set_viewport(rc_gfx_encoder *enc, rc_box2i rect, float min_depth, float max_depth)
{
    RC_ASSERT(enc->in_pass);
    rc_gfx_cmd_set_viewport cmd = {.rect = rect, .min_depth = min_depth, .max_depth = max_depth};
    enc_push_cmd(enc, RC_GFX_OP_SET_VIEWPORT, &cmd, (uint32_t)sizeof(cmd));
}

void rc_gfx_encoder_set_scissor(rc_gfx_encoder *enc, rc_box2i rect)
{
    RC_ASSERT(enc->in_pass);
    rc_gfx_cmd_set_scissor cmd = {.rect = rect};
    enc_push_cmd(enc, RC_GFX_OP_SET_SCISSOR, &cmd, (uint32_t)sizeof(cmd));
}

void rc_gfx_encoder_set_blend_constant(rc_gfx_encoder *enc, rc_vec4f color)
{
    RC_ASSERT(enc->in_pass);
    rc_gfx_cmd_set_blend_constant cmd = {.color = color};
    enc_push_cmd(enc, RC_GFX_OP_SET_BLEND_CONSTANT, &cmd, (uint32_t)sizeof(cmd));
}

void rc_gfx_encoder_set_stencil_reference(rc_gfx_encoder *enc, uint32_t reference)
{
    RC_ASSERT(enc->in_pass);
    rc_gfx_cmd_set_stencil_reference cmd = {.reference = reference};
    enc_push_cmd(enc, RC_GFX_OP_SET_STENCIL_REFERENCE, &cmd, (uint32_t)sizeof(cmd));
}

/* ---- draws ---- */

void rc_gfx_encoder_draw(rc_gfx_encoder *enc, const rc_gfx_draw_desc *desc)
{
    RC_ASSERT(enc->in_pass);
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->vertex_count > 0);
    enc_push_cmd(enc, RC_GFX_OP_DRAW, desc, (uint32_t)sizeof(*desc));
}

void rc_gfx_encoder_draw_indexed(rc_gfx_encoder *enc, const rc_gfx_draw_indexed_desc *desc)
{
    RC_ASSERT(enc->in_pass);
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->index_count > 0);
    enc_push_cmd(enc, RC_GFX_OP_DRAW_INDEXED, desc, (uint32_t)sizeof(*desc));
}

/* ---- debug groups ---- */

void rc_gfx_encoder_push_debug_group(rc_gfx_encoder *enc, rc_str label)
{
    enc->debug_group_depth += 1;
    enc_push_cmd(enc, RC_GFX_OP_PUSH_DEBUG_GROUP, &label, (uint32_t)sizeof(label));
}

void rc_gfx_encoder_pop_debug_group(rc_gfx_encoder *enc)
{
    RC_ASSERT(enc->debug_group_depth > 0);
    enc->debug_group_depth -= 1;
    enc_push_cmd(enc, RC_GFX_OP_POP_DEBUG_GROUP, NULL, 0);
}

/* ---- dump ---- */

static void dump_handle(rc_mstr *out, const char *field, rc_genpool_handle h, rc_arena *arena)
{
    rc_mstr_append(out, rc_str_from_cstr(field), arena);
    rc_mstr_append_char(out, '=', arena);
    if (rc_genpool_handle_is_null(h)) {
        rc_mstr_append(out, RC_STR("none"), arena);
        return;
    }
    // index:generation
    rc_mstr_append_u32(out, rc_genpool_handle_index(h), arena);
    rc_mstr_append_char(out, ':', arena);
    rc_mstr_append_u32(out, rc_genpool_handle_gen(h), arena);
}

static void dump_u32(rc_mstr *out, const char *field, uint32_t value, rc_arena *arena)
{
    rc_mstr_append(out, rc_str_from_cstr(field), arena);
    rc_mstr_append_char(out, '=', arena);
    rc_mstr_append_u32(out, value, arena);
}

static void dump_box(rc_mstr *out, rc_box2i rect, rc_arena *arena)
{
    rc_vec2i pos = rc_box2i_min(rect);
    rc_vec2i size = rc_box2i_size(rect);
    rc_mstr_append(out, RC_STR("rect=["), arena);
    rc_mstr_append_i32(out, pos.x, arena);
    rc_mstr_append_char(out, ',', arena);
    rc_mstr_append_i32(out, pos.y, arena);
    rc_mstr_append_char(out, ' ', arena);
    rc_mstr_append_i32(out, size.x, arena);
    rc_mstr_append_char(out, 'x', arena);
    rc_mstr_append_i32(out, size.y, arena);
    rc_mstr_append_char(out, ']', arena);
}

static void dump_vec4(rc_mstr *out, rc_vec4f v, rc_arena *arena)
{
    rc_mstr_append_char(out, '(', arena);
    rc_mstr_append_f32(out, v.x, arena);
    rc_mstr_append_char(out, ',', arena);
    rc_mstr_append_f32(out, v.y, arena);
    rc_mstr_append_char(out, ',', arena);
    rc_mstr_append_f32(out, v.z, arena);
    rc_mstr_append_char(out, ',', arena);
    rc_mstr_append_f32(out, v.w, arena);
    rc_mstr_append_char(out, ')', arena);
}

static void dump_pass_begin(rc_mstr *out, rc_gfx_cmd_reader *r, rc_arena *arena)
{
    rc_gfx_pass_desc desc;
    rc_gfx_cmd_read(r, &desc, (uint32_t)sizeof(desc));
    rc_mstr_append(out, RC_STR("pass_begin "), arena);
    dump_handle(out, "target", desc.target.h, arena);
    for (uint32_t i = 0; i < RC_GFX_MAX_COLOR_ATTACHMENTS; i += 1) {
        const rc_gfx_color_attachment_action *action = &desc.colors[i];
        if (action->load_op == RC_GFX_LOAD_OP_CLEAR && i == 0) {
            rc_mstr_append(out, RC_STR(" clear0="), arena);
            dump_vec4(out, action->clear_value, arena);
        }
    }
    if (rc_str_is_valid(desc.label)) {
        rc_mstr_append(out, RC_STR(" label=\""), arena);
        rc_mstr_append(out, desc.label, arena);
        rc_mstr_append_char(out, '"', arena);
    }
}

rc_mstr rc_gfx_cmd_buffer_dump(rc_gfx_cmd_buffer cb, rc_arena *arena)
{
    rc_mstr out = {0};
    rc_gfx_cmd_reader r = {.p = cb.data, .end = cb.data + cb.size};
    while (!rc_gfx_cmd_reader_done(&r)) {
        rc_gfx_op op = (rc_gfx_op)rc_gfx_cmd_read_u32(&r);
        switch (op) {
        case RC_GFX_OP_PASS_BEGIN: {
            dump_pass_begin(&out, &r, arena);
            break;
        }
        case RC_GFX_OP_PASS_END: {
            rc_mstr_append(&out, RC_STR("pass_end"), arena);
            break;
        }
        case RC_GFX_OP_SET_PIPELINE: {
            rc_gfx_cmd_set_pipeline cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_pipeline "), arena);
            dump_handle(&out, "id", cmd.pipeline.h, arena);
            break;
        }
        case RC_GFX_OP_SET_BIND_GROUP: {
            rc_gfx_cmd_set_bind_group cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_bind_group "), arena);
            dump_u32(&out, "group", cmd.group_index, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_handle(&out, "id", cmd.group.h, arena);
            rc_mstr_append(&out, RC_STR(" offsets=["), arena);
            for (uint32_t i = 0; i < cmd.offset_count; i += 1) {
                if (i != 0) {
                    rc_mstr_append_char(&out, ',', arena);
                }
                rc_mstr_append_u32(&out, rc_gfx_cmd_read_u32(&r), arena);
            }
            rc_mstr_append_char(&out, ']', arena);
            break;
        }
        case RC_GFX_OP_SET_VERTEX_BUFFER: {
            rc_gfx_cmd_set_vertex_buffer cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_vertex_buffer "), arena);
            dump_u32(&out, "slot", cmd.slot, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_handle(&out, "id", cmd.buffer.h, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_u32(&out, "offset", cmd.offset, arena);
            break;
        }
        case RC_GFX_OP_SET_INDEX_BUFFER: {
            rc_gfx_cmd_set_index_buffer cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_index_buffer "), arena);
            dump_handle(&out, "id", cmd.buffer.h, arena);
            rc_mstr_append(&out, cmd.format == RC_GFX_INDEX_FORMAT_U16 ? RC_STR(" fmt=u16 ") : RC_STR(" fmt=u32 "), arena);
            dump_u32(&out, "offset", cmd.offset, arena);
            break;
        }
        case RC_GFX_OP_SET_VIEWPORT: {
            rc_gfx_cmd_set_viewport cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_viewport "), arena);
            dump_box(&out, cmd.rect, arena);
            rc_mstr_append(&out, RC_STR(" depth=["), arena);
            rc_mstr_append_f32(&out, cmd.min_depth, arena);
            rc_mstr_append_char(&out, ',', arena);
            rc_mstr_append_f32(&out, cmd.max_depth, arena);
            rc_mstr_append_char(&out, ']', arena);
            break;
        }
        case RC_GFX_OP_SET_SCISSOR: {
            rc_gfx_cmd_set_scissor cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_scissor "), arena);
            dump_box(&out, cmd.rect, arena);
            break;
        }
        case RC_GFX_OP_SET_BLEND_CONSTANT: {
            rc_gfx_cmd_set_blend_constant cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_blend_constant "), arena);
            dump_vec4(&out, cmd.color, arena);
            break;
        }
        case RC_GFX_OP_SET_STENCIL_REFERENCE: {
            rc_gfx_cmd_set_stencil_reference cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("set_stencil_reference "), arena);
            dump_u32(&out, "ref", cmd.reference, arena);
            break;
        }
        case RC_GFX_OP_DRAW: {
            rc_gfx_draw_desc cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("draw "), arena);
            dump_u32(&out, "vertices", cmd.vertex_count, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_u32(&out, "instances", cmd.instance_count == 0 ? 1 : cmd.instance_count, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_u32(&out, "first_vertex", cmd.first_vertex, arena);
            break;
        }
        case RC_GFX_OP_DRAW_INDEXED: {
            rc_gfx_draw_indexed_desc cmd;
            rc_gfx_cmd_read(&r, &cmd, (uint32_t)sizeof(cmd));
            rc_mstr_append(&out, RC_STR("draw_indexed "), arena);
            dump_u32(&out, "indices", cmd.index_count, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_u32(&out, "instances", cmd.instance_count == 0 ? 1 : cmd.instance_count, arena);
            rc_mstr_append_char(&out, ' ', arena);
            dump_u32(&out, "first_index", cmd.first_index, arena);
            rc_mstr_append(&out, RC_STR(" base_vertex="), arena);
            rc_mstr_append_i32(&out, cmd.base_vertex, arena);
            break;
        }
        case RC_GFX_OP_PUSH_DEBUG_GROUP: {
            rc_str label;
            rc_gfx_cmd_read(&r, &label, (uint32_t)sizeof(label));
            rc_mstr_append(&out, RC_STR("push_debug_group \""), arena);
            rc_mstr_append(&out, label, arena);
            rc_mstr_append_char(&out, '"', arena);
            break;
        }
        case RC_GFX_OP_POP_DEBUG_GROUP: {
            rc_mstr_append(&out, RC_STR("pop_debug_group"), arena);
            break;
        }
        default:
            RC_UNREACHABLE();
        }
        rc_mstr_append_char(&out, '\n', arena);
    }
    return out;
}
