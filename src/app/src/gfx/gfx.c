/*
 * gfx.c - gfx frontend: handle pools, validation, lifecycle, and the uniform
 * ring.
 *
 * The frontend owns every API-visible object as a genpool slot; the slot
 * carries its own generation, bumped on destroy, and each public handle wraps
 * the pool's rc_genpool_handle in its distinct one-member struct.  Resolving
 * asserts the handle is still valid, so stale handles trap instead of
 * aliasing recycled slots.  GL objects are created through the
 * rc_gfx_backend_* interface and released through a deferred queue
 * RC_GFX_FRAMES_IN_FLIGHT frames after their destroy.
 */

#include "richc/gfx/gfx.h"

#include "gfx_internal.h"
#include "richc/arena.h"
#include "richc/gfx/color.h"
#include "richc/hash.h"
#include "richc/ops.h"
#include "richc/thread/atomic.h"
#include "richc/thread/thread.h"

/* ---- pools ---- */

#define RC_GENPOOL_TYPE rc_gfx_buffer_obj
#define RC_GENPOOL_NAME rc_gfx_buffer_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_texture_obj
#define RC_GENPOOL_NAME rc_gfx_texture_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_sampler_obj
#define RC_GENPOOL_NAME rc_gfx_sampler_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_shader_obj
#define RC_GENPOOL_NAME rc_gfx_shader_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_bind_group_layout_obj
#define RC_GENPOOL_NAME rc_gfx_bind_group_layout_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_bind_group_obj
#define RC_GENPOOL_NAME rc_gfx_bind_group_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_pipeline_layout_obj
#define RC_GENPOOL_NAME rc_gfx_pipeline_layout_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_pipeline_obj
#define RC_GENPOOL_NAME rc_gfx_pipeline_pool
#include "richc/template/genpool.h"

#define RC_GENPOOL_TYPE rc_gfx_render_target_obj
#define RC_GENPOOL_NAME rc_gfx_render_target_pool
#include "richc/template/genpool.h"

/* ---- deferred destruction queue ---- */

typedef struct rc_gfx_release_entry {
    rc_gfx_release_kind kind;
    uint32_t            name;
    uint64_t            frame;   /* frame_index at destroy time */
} rc_gfx_release_entry;

#define RC_ARRAY_TYPE rc_gfx_release_entry
#define RC_ARRAY_NAME rc_gfx_release_entry
#include "richc/template/array.h"

/* ---- device state ---- */

static struct {
    bool               initialized;
    rc_arena          *arena;
    bool               validation;
    rc_gfx_color_space color_space;
    uint32_t           swapchain_samples;
    rc_gfx_texture_format swapchain_format;
    rc_gfx_texture_format swapchain_depth_format;
    rc_vec2i           swapchain_size;
    bool               in_frame;
    uint64_t           frame_index;
    uint64_t           context_thread;
    rc_gfx_features    features;
    rc_gfx_limits      limits;

    rc_gfx_buffer_pool            buffer_pool;
    rc_gfx_texture_pool           texture_pool;
    rc_gfx_sampler_pool           sampler_pool;
    rc_gfx_shader_pool            shader_pool;
    rc_gfx_bind_group_layout_pool bind_group_layout_pool;
    rc_gfx_bind_group_pool        bind_group_pool;
    rc_gfx_pipeline_layout_pool   pipeline_layout_pool;
    rc_gfx_pipeline_pool          pipeline_pool;
    rc_gfx_render_target_pool     render_target_pool;

    /* uniform ring: one GL buffer with RC_GFX_FRAMES_IN_FLIGHT rotating
     * regions, written through a CPU shadow flushed at submit */
    rc_gfx_buffer ring_handle;
    uint32_t      ring_region_size;
    uint8_t      *ring_shadow;      /* RC_GFX_FRAMES_IN_FLIGHT x region bytes */
    rc_atomic_u32 ring_cursor;      /* offset within the current region */
    uint32_t      ring_flushed;     /* flushed watermark within the region */
    uint32_t      ring_base;        /* current region base (ring-absolute) */

    rc_array_rc_gfx_release_entry releases;
    uint32_t                      release_head;
} gfx;

static void gfx_check_context_thread(void)
{
    RC_ASSERT(gfx.initialized);
    RC_ASSERT(rc_thread_current_id() == gfx.context_thread);
}

/*
 * Backend-facing resolvers.  A null or stale handle looks up as NULL, which
 * traps here rather than at the use site.
 */
rc_gfx_buffer_obj *rc_gfx_resolve_buffer(rc_gfx_buffer handle)
{
    rc_gfx_buffer_obj *obj = rc_gfx_buffer_pool_at(&gfx.buffer_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_texture_obj *rc_gfx_resolve_texture(rc_gfx_texture handle)
{
    rc_gfx_texture_obj *obj = rc_gfx_texture_pool_at(&gfx.texture_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_sampler_obj *rc_gfx_resolve_sampler(rc_gfx_sampler handle)
{
    rc_gfx_sampler_obj *obj = rc_gfx_sampler_pool_at(&gfx.sampler_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_shader_obj *rc_gfx_resolve_shader(rc_gfx_shader handle)
{
    rc_gfx_shader_obj *obj = rc_gfx_shader_pool_at(&gfx.shader_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_bind_group_layout_obj *rc_gfx_resolve_bind_group_layout(rc_gfx_bind_group_layout handle)
{
    rc_gfx_bind_group_layout_obj *obj = rc_gfx_bind_group_layout_pool_at(&gfx.bind_group_layout_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_bind_group_obj *rc_gfx_resolve_bind_group(rc_gfx_bind_group handle)
{
    rc_gfx_bind_group_obj *obj = rc_gfx_bind_group_pool_at(&gfx.bind_group_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_pipeline_layout_obj *rc_gfx_resolve_pipeline_layout(rc_gfx_pipeline_layout handle)
{
    rc_gfx_pipeline_layout_obj *obj = rc_gfx_pipeline_layout_pool_at(&gfx.pipeline_layout_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_pipeline_obj *rc_gfx_resolve_pipeline(rc_gfx_pipeline handle)
{
    rc_gfx_pipeline_obj *obj = rc_gfx_pipeline_pool_at(&gfx.pipeline_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_gfx_render_target_obj *rc_gfx_resolve_render_target(rc_gfx_render_target handle)
{
    rc_gfx_render_target_obj *obj = rc_gfx_render_target_pool_at(&gfx.render_target_pool, handle.h);
    RC_ASSERT(obj != NULL);
    return obj;
}

rc_arena *rc_gfx_internal_arena(void)
{
    return gfx.arena;
}

bool rc_gfx_internal_validation(void)
{
    return gfx.validation;
}

static void gfx_queue_release(rc_gfx_release_kind kind, uint32_t name)
{
    if (name == 0) {
        return;
    }
    rc_array_rc_gfx_release_entry_push(&gfx.releases, (rc_gfx_release_entry) {
        .kind = kind,
        .name = name,
        .frame = gfx.frame_index,
    }, gfx.arena);
}

/* ---- device lifecycle ---- */

void rc_gfx_init(const rc_gfx_desc *desc)
{
    RC_ASSERT(!gfx.initialized);
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->arena != NULL);

    gfx.arena = desc->arena;
    gfx.color_space = desc->color_space;
    gfx.swapchain_samples = desc->swapchain_sample_count > 1 ? desc->swapchain_sample_count : 1;
    gfx.swapchain_format = desc->color_space == RC_GFX_COLOR_SPACE_LINEAR
        ? RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM
        : RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB;
    RC_ASSERT(desc->swapchain_depth_format == RC_GFX_TEXTURE_FORMAT_NONE
              || rc_gfx_texture_format_is_depth(desc->swapchain_depth_format));
    gfx.swapchain_depth_format = desc->swapchain_depth_format;
#ifndef NDEBUG
    gfx.validation = true;
#else
    gfx.validation = desc->validation;
#endif
    gfx.context_thread = rc_thread_current_id();

    uint32_t region = desc->uniform_ring_size != 0 ? desc->uniform_ring_size : (1u << 20);
    region = (region + RC_GFX_UNIFORM_ALIGN - 1u) & ~(uint32_t)(RC_GFX_UNIFORM_ALIGN - 1u);
    gfx.ring_region_size = region;
    uint32_t ring_total = region * RC_GFX_FRAMES_IN_FLIGHT;

    uint32_t ring_gl_buffer = 0;
    gfx.initialized = true;
    gfx.features = rc_gfx_backend_init(ring_total, &ring_gl_buffer);
    gfx.limits = rc_gfx_backend_limits();
    RC_ASSERT(gfx.limits.uniform_buffer_offset_alignment <= RC_GFX_UNIFORM_ALIGN);

    gfx.ring_shadow = rc_arena_alloc(gfx.arena, ring_total);

    gfx.ring_handle = (rc_gfx_buffer) {.h = rc_gfx_buffer_pool_alloc(&gfx.buffer_pool, gfx.arena)};
    rc_gfx_buffer_pool_set(&gfx.buffer_pool, gfx.ring_handle.h, (rc_gfx_buffer_obj) {
        .is_ring = true,
        .size = ring_total,
        .usage = RC_GFX_BUFFER_USAGE_UNIFORM,
        .update = RC_GFX_BUFFER_UPDATE_STREAM,
        .gl_buffer = ring_gl_buffer,
    });
}

/*
 * Shutdown sweep iterators: one genpool_foreach per pool that owns GL
 * objects, releasing the objects of every still-live slot.  The layout and
 * pipeline pools hold no GL objects, so they need no sweep.
 */

#define RC_GENPOOL_FOREACH_POOL       rc_gfx_buffer_pool
#define RC_GENPOOL_FOREACH_FUNC(p, h) rc_gfx_backend_release(RC_GFX_RELEASE_BUFFER, rc_gfx_buffer_pool_get(p, h).gl_buffer)
#define RC_GENPOOL_FOREACH_NAME       gfx_release_live_buffers
#include "richc/template/algorithm/genpool_foreach.h"

#define RC_GENPOOL_FOREACH_POOL       rc_gfx_texture_pool
#define RC_GENPOOL_FOREACH_FUNC(p, h) rc_gfx_backend_release(RC_GFX_RELEASE_TEXTURE, rc_gfx_texture_pool_get(p, h).gl_texture)
#define RC_GENPOOL_FOREACH_NAME       gfx_release_live_textures
#include "richc/template/algorithm/genpool_foreach.h"

#define RC_GENPOOL_FOREACH_POOL       rc_gfx_sampler_pool
#define RC_GENPOOL_FOREACH_FUNC(p, h) rc_gfx_backend_release(RC_GFX_RELEASE_SAMPLER, rc_gfx_sampler_pool_get(p, h).gl_sampler)
#define RC_GENPOOL_FOREACH_NAME       gfx_release_live_samplers
#include "richc/template/algorithm/genpool_foreach.h"

#define RC_GENPOOL_FOREACH_POOL       rc_gfx_shader_pool
#define RC_GENPOOL_FOREACH_FUNC(p, h) rc_gfx_backend_release(RC_GFX_RELEASE_PROGRAM, rc_gfx_shader_pool_get(p, h).gl_program)
#define RC_GENPOOL_FOREACH_NAME       gfx_release_live_shaders
#include "richc/template/algorithm/genpool_foreach.h"

// a bind group only owns a GL object for its STORAGE_BUFFER_READ entries (the
// TBO textures created at bind time)
static void gfx_release_bind_group(rc_gfx_bind_group_pool *pool, rc_genpool_handle handle)
{
    rc_gfx_bind_group_obj *obj = rc_gfx_bind_group_pool_at(pool, handle);
    for (uint32_t e = 0; e < obj->entry_count; e += 1) {
        if (obj->entries[e].type == RC_GFX_BINDING_STORAGE_BUFFER_READ) {
            rc_gfx_backend_release(RC_GFX_RELEASE_TEXTURE, obj->entries[e].gl_object);
        }
    }
}

#define RC_GENPOOL_FOREACH_POOL       rc_gfx_bind_group_pool
#define RC_GENPOOL_FOREACH_FUNC(p, h) gfx_release_bind_group(p, h)
#define RC_GENPOOL_FOREACH_NAME       gfx_release_live_bind_groups
#include "richc/template/algorithm/genpool_foreach.h"

static void gfx_release_render_target(rc_gfx_render_target_pool *pool, rc_genpool_handle handle)
{
    rc_gfx_render_target_obj obj = rc_gfx_render_target_pool_get(pool, handle);
    rc_gfx_backend_release(RC_GFX_RELEASE_FBO, obj.gl_fbo);
    rc_gfx_backend_release(RC_GFX_RELEASE_FBO, obj.gl_resolve_fbo);
}

#define RC_GENPOOL_FOREACH_POOL       rc_gfx_render_target_pool
#define RC_GENPOOL_FOREACH_FUNC(p, h) gfx_release_render_target(p, h)
#define RC_GENPOOL_FOREACH_NAME       gfx_release_live_render_targets
#include "richc/template/algorithm/genpool_foreach.h"

void rc_gfx_shutdown(void)
{
    gfx_check_context_thread();
    RC_ASSERT(!gfx.in_frame);

    // each sweep builds its dead-slot bitset in a by-value scratch copy of
    // the gfx arena; nothing else allocates from the arena during the sweep
    gfx_release_live_buffers(&gfx.buffer_pool, *gfx.arena);
    gfx_release_live_textures(&gfx.texture_pool, *gfx.arena);
    gfx_release_live_samplers(&gfx.sampler_pool, *gfx.arena);
    gfx_release_live_shaders(&gfx.shader_pool, *gfx.arena);
    gfx_release_live_bind_groups(&gfx.bind_group_pool, *gfx.arena);
    gfx_release_live_render_targets(&gfx.render_target_pool, *gfx.arena);

    // drain anything already queued, then let the backend drop its own state
    for (uint32_t i = gfx.release_head; i < gfx.releases.num; i += 1) {
        rc_gfx_release_entry entry = rc_array_rc_gfx_release_entry_get(&gfx.releases, i);
        rc_gfx_backend_release(entry.kind, entry.name);
    }
    rc_gfx_backend_shutdown();

    memset(&gfx, 0, sizeof(gfx));
}

void rc_gfx_begin_frame(rc_vec2i size)
{
    gfx_check_context_thread();
    RC_ASSERT(!gfx.in_frame);
    RC_ASSERT(size.x > 0 && size.y > 0);

    gfx.in_frame = true;
    gfx.swapchain_size = size;
    gfx.ring_base = (uint32_t)(gfx.frame_index % RC_GFX_FRAMES_IN_FLIGHT) * gfx.ring_region_size;
    rc_atomic_u32_store(&gfx.ring_cursor, 0, RC_MEMORY_ORDER_RELAXED);
    gfx.ring_flushed = 0;

    rc_gfx_backend_begin_frame(size, gfx.swapchain_format, gfx.swapchain_depth_format,
                               gfx.swapchain_samples);
}

void rc_gfx_end_frame(void)
{
    gfx_check_context_thread();
    RC_ASSERT(gfx.in_frame);

    rc_gfx_backend_end_frame(gfx.color_space);
    gfx.in_frame = false;
    gfx.frame_index += 1;

    // retire deferred destructions that have aged out of all in-flight frames
    while (gfx.release_head < gfx.releases.num) {
        rc_gfx_release_entry entry = rc_array_rc_gfx_release_entry_get(&gfx.releases, gfx.release_head);
        if (entry.frame + RC_GFX_FRAMES_IN_FLIGHT > gfx.frame_index) {
            break;
        }
        rc_gfx_backend_release(entry.kind, entry.name);
        gfx.release_head += 1;
    }
    if (gfx.release_head == gfx.releases.num) {
        rc_array_rc_gfx_release_entry_reset(&gfx.releases);
        gfx.release_head = 0;
    }
}

rc_vec2i rc_gfx_swapchain_size(void)
{
    RC_ASSERT(gfx.initialized);
    return gfx.swapchain_size;
}

rc_gfx_texture_format rc_gfx_swapchain_format(void)
{
    RC_ASSERT(gfx.initialized);
    return gfx.swapchain_format;
}

rc_gfx_texture_format rc_gfx_swapchain_depth_format(void)
{
    RC_ASSERT(gfx.initialized);
    return gfx.swapchain_depth_format;
}

rc_gfx_features rc_gfx_features_query(void)
{
    RC_ASSERT(gfx.initialized);
    return gfx.features;
}

rc_gfx_limits rc_gfx_limits_query(void)
{
    RC_ASSERT(gfx.initialized);
    return gfx.limits;
}

uint32_t rc_gfx_format_caps_query(rc_gfx_texture_format fmt)
{
    RC_ASSERT(gfx.initialized);
    return rc_gfx_backend_format_caps(fmt);
}

/* ---- uniform ring ---- */

rc_gfx_buffer rc_gfx_uniform_buffer(void)
{
    RC_ASSERT(gfx.initialized);
    return gfx.ring_handle;
}

rc_gfx_uniform_alloc rc_gfx_encoder_alloc_uniforms(rc_gfx_encoder *enc, uint32_t size)
{
    RC_ASSERT(enc != NULL);
    RC_ASSERT(gfx.in_frame);
    RC_ASSERT(size > 0);

    uint32_t aligned = (size + RC_GFX_UNIFORM_ALIGN - 1u) & ~(uint32_t)(RC_GFX_UNIFORM_ALIGN - 1u);
    uint32_t local = rc_atomic_u32_fetch_add(&gfx.ring_cursor, aligned, RC_MEMORY_ORDER_RELAXED);
    RC_PANIC(local + aligned <= gfx.ring_region_size);   // grow uniform_ring_size in rc_gfx_desc

    uint32_t offset = gfx.ring_base + local;
    return (rc_gfx_uniform_alloc) {
        .ptr = gfx.ring_shadow + offset,
        .buffer = gfx.ring_handle,
        .offset = offset,
    };
}

/* ---- submission ---- */

void rc_gfx_submit(const rc_gfx_cmd_buffer *buffers, uint32_t count)
{
    gfx_check_context_thread();
    RC_ASSERT(gfx.in_frame);
    RC_ASSERT(buffers != NULL || count == 0);

    // flush uniform bytes written since the last submit
    uint32_t cursor = rc_atomic_u32_load(&gfx.ring_cursor, RC_MEMORY_ORDER_ACQUIRE);
    if (cursor > gfx.ring_flushed) {
        uint32_t start = gfx.ring_base + gfx.ring_flushed;
        rc_gfx_backend_flush_uniforms(start, (rc_view_bytes) {
            .data = gfx.ring_shadow + start,
            .num = cursor - gfx.ring_flushed,
        });
        gfx.ring_flushed = cursor;
    }

    for (uint32_t i = 0; i < count; i += 1) {
        rc_gfx_backend_submit((rc_view_bytes) {.data = buffers[i].data, .num = buffers[i].size});
    }
}

/* ---- buffers ---- */

rc_gfx_buffer rc_gfx_buffer_make(const rc_gfx_buffer_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->size > 0);
    RC_ASSERT(desc->usage != 0);
    if (desc->update == RC_GFX_BUFFER_UPDATE_IMMUTABLE) {
        RC_ASSERT(desc->data.data != NULL && desc->data.num == desc->size);
    } else if (desc->data.data != NULL) {
        RC_ASSERT(desc->data.num <= desc->size);
    }

    rc_gfx_buffer handle = {.h = rc_gfx_buffer_pool_alloc(&gfx.buffer_pool, gfx.arena)};
    rc_gfx_buffer_pool_set(&gfx.buffer_pool, handle.h, (rc_gfx_buffer_obj) {
        .size = desc->size,
        .usage = desc->usage,
        .update = desc->update,
    });
    rc_gfx_backend_make_buffer(rc_gfx_buffer_pool_at(&gfx.buffer_pool, handle.h), desc->data);
    return handle;
}

void rc_gfx_buffer_destroy(rc_gfx_buffer buf)
{
    gfx_check_context_thread();
    rc_gfx_buffer_obj obj = rc_gfx_buffer_pool_get(&gfx.buffer_pool, buf.h);
    RC_ASSERT(!obj.is_ring);
    rc_gfx_backend_evict_buffer(buf);
    gfx_queue_release(RC_GFX_RELEASE_BUFFER, obj.gl_buffer);
    rc_gfx_buffer_pool_free(&gfx.buffer_pool, buf.h);
}

void rc_gfx_buffer_update(rc_gfx_buffer buf, uint32_t offset, rc_view_bytes data)
{
    gfx_check_context_thread();
    rc_gfx_buffer_obj *obj = rc_gfx_buffer_pool_at(&gfx.buffer_pool, buf.h);
    RC_ASSERT(obj != NULL);   // null or stale handle
    RC_ASSERT(!obj->is_ring);
    RC_ASSERT(obj->update != RC_GFX_BUFFER_UPDATE_IMMUTABLE);
    RC_ASSERT(data.data != NULL && data.num > 0);
    RC_ASSERT(offset + (uint64_t)data.num <= obj->size);
    rc_gfx_backend_update_buffer(obj, offset, data);
}

/* ---- textures ---- */

uint32_t rc_gfx_mip_count(rc_vec2i size)
{
    RC_ASSERT(size.x > 0 && size.y > 0);
    uint32_t largest = (uint32_t)rc_max_i32(size.x, size.y);
    return 32u - rc_clz_u32(largest);
}

static rc_vec2i mip_size(rc_vec2i size, uint32_t mip)
{
    return rc_vec2i_make(rc_max_i32(1, size.x >> mip), rc_max_i32(1, size.y >> mip));
}

static uint32_t subresource_bytes(rc_gfx_texture_format fmt, rc_vec2i size)
{
    rc_vec2i block = rc_gfx_texture_format_block_dim(fmt);
    uint32_t bw = ((uint32_t)size.x + (uint32_t)block.x - 1u) / (uint32_t)block.x;
    uint32_t bh = ((uint32_t)size.y + (uint32_t)block.y - 1u) / (uint32_t)block.y;
    return bw * bh * rc_gfx_texture_format_block_size(fmt);
}

rc_gfx_texture rc_gfx_texture_make(const rc_gfx_texture_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->format != RC_GFX_TEXTURE_FORMAT_NONE);
    RC_ASSERT(desc->size.x > 0 && desc->size.y > 0);

    uint32_t depth = desc->depth != 0 ? desc->depth : 1;
    if (desc->dim == RC_GFX_TEXTURE_DIM_CUBE) {
        RC_ASSERT(depth == 1 || depth == 6);
        depth = 6;
        RC_ASSERT(desc->size.x == desc->size.y);
    }
    uint32_t mips = desc->mip_count != 0 ? desc->mip_count : 1;
    RC_ASSERT(mips <= rc_gfx_mip_count(desc->size) && mips <= RC_GFX_MAX_MIP_LEVELS);
    uint32_t samples = desc->sample_count > 1 ? desc->sample_count : 1;
    if (samples > 1) {
        RC_ASSERT(desc->dim == RC_GFX_TEXTURE_DIM_2D);
        RC_ASSERT(mips == 1);
        RC_ASSERT(desc->data.count == 0);
    }
    uint32_t usage = desc->usage != 0 ? desc->usage : RC_GFX_TEXTURE_USAGE_SAMPLED;
    if (rc_gfx_texture_format_is_compressed(desc->format)) {
        RC_ASSERT((usage & RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT) == 0);
    }
    if (desc->data.count != 0) {
        uint32_t expected = desc->dim == RC_GFX_TEXTURE_DIM_3D ? mips : depth * mips;
        RC_ASSERT(desc->data.count == expected);
        RC_ASSERT(desc->data.subresources != NULL);
        if (gfx.validation) {
            for (uint32_t s = 0; s < desc->data.count; s += 1) {
                uint32_t mip = s % mips;
                uint32_t bytes = subresource_bytes(desc->format, mip_size(desc->size, mip));
                if (desc->dim == RC_GFX_TEXTURE_DIM_3D) {
                    bytes *= (uint32_t)rc_max_i32(1, (int32_t)depth >> mip);
                }
                RC_ASSERT(desc->data.subresources[s].num == bytes);
            }
        }
    }

    rc_gfx_texture handle = {.h = rc_gfx_texture_pool_alloc(&gfx.texture_pool, gfx.arena)};
    rc_gfx_texture_pool_set(&gfx.texture_pool, handle.h, (rc_gfx_texture_obj) {
        .dim = desc->dim,
        .format = desc->format,
        .size = desc->size,
        .depth = depth,
        .mip_count = mips,
        .sample_count = samples,
        .usage = usage,
    });
    rc_gfx_backend_make_texture(rc_gfx_texture_pool_at(&gfx.texture_pool, handle.h), desc);
    return handle;
}

void rc_gfx_texture_destroy(rc_gfx_texture tex)
{
    gfx_check_context_thread();
    rc_gfx_texture_obj obj = rc_gfx_texture_pool_get(&gfx.texture_pool, tex.h);
    gfx_queue_release(RC_GFX_RELEASE_TEXTURE, obj.gl_texture);
    rc_gfx_texture_pool_free(&gfx.texture_pool, tex.h);
}

void rc_gfx_texture_update(rc_gfx_texture tex, uint32_t mip, uint32_t slice,
                           rc_box2i region, rc_view_bytes data)
{
    gfx_check_context_thread();
    rc_gfx_texture_obj *obj = rc_gfx_texture_pool_at(&gfx.texture_pool, tex.h);
    RC_ASSERT(obj != NULL);   // null or stale handle
    RC_ASSERT(!rc_gfx_texture_format_is_compressed(obj->format));
    RC_ASSERT(obj->sample_count == 1);
    RC_ASSERT(mip < obj->mip_count);
    RC_ASSERT(slice < obj->depth);
    rc_vec2i bounds = mip_size(obj->size, mip);
    rc_vec2i pos = rc_box2i_min(region);
    rc_vec2i size = rc_box2i_size(region);
    RC_ASSERT(pos.x >= 0 && pos.y >= 0 && size.x > 0 && size.y > 0);
    RC_ASSERT(pos.x + size.x <= bounds.x && pos.y + size.y <= bounds.y);
    RC_ASSERT(data.num == (uint32_t)size.x * (uint32_t)size.y * rc_gfx_texture_format_block_size(obj->format));
    rc_gfx_backend_update_texture(obj, mip, slice, region, data);
}

void rc_gfx_texture_generate_mipmaps(rc_gfx_texture tex)
{
    gfx_check_context_thread();
    rc_gfx_texture_obj *obj = rc_gfx_texture_pool_at(&gfx.texture_pool, tex.h);
    RC_ASSERT(obj != NULL);   // null or stale handle
    RC_ASSERT(obj->mip_count > 1);
    rc_gfx_backend_generate_mipmaps(obj);
}

rc_gfx_texture rc_gfx_texture_from_image(rc_image img, bool srgb, bool mipmaps)
{
    gfx_check_context_thread();
    RC_ASSERT(img.format != RC_PIXEL_FORMAT_NONE);
    RC_ASSERT(img.size.x > 0 && img.size.y > 0);

    bool single = img.format == RC_PIXEL_FORMAT_R8;
    rc_gfx_texture_format fmt = single
        ? RC_GFX_TEXTURE_FORMAT_R8_UNORM
        : (srgb ? RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB : RC_GFX_TEXTURE_FORMAT_RGBA8_UNORM);
    uint32_t bpp = single ? 1 : 4;
    uint32_t w = (uint32_t)img.size.x;
    uint32_t h = (uint32_t)img.size.y;

    rc_gfx_texture tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
        .format = fmt,
        .size = img.size,
        .mip_count = mipmaps ? rc_gfx_mip_count(img.size) : 1,
        .usage = RC_GFX_TEXTURE_USAGE_SAMPLED | RC_GFX_TEXTURE_USAGE_COPY_DST,
    });

    // repack into tight rows (and widen RGB8 / R8-with-stride sources); the
    // staging block is freed straight after the upload, so it never outlives
    // the call even though it comes from the persistent arena
    uint32_t packed_size = w * h * bpp;
    uint8_t *packed = rc_arena_alloc(gfx.arena, packed_size);
    for (uint32_t y = 0; y < h; y += 1) {
        for (uint32_t x = 0; x < w; x += 1) {
            uint32_t color = rc_image_get_pixel(img, (int32_t)x, (int32_t)y);
            uint8_t *dst = packed + (y * w + x) * bpp;
            if (single) {
                dst[0] = (uint8_t)(color & 0xFFu);
            } else {
                memcpy(dst, &color, 4);   // packed colour is RGBA byte order in memory
            }
        }
    }

    rc_gfx_texture_update(tex, 0, 0, rc_box2i_make_pos_size(rc_vec2i_make_zero(), img.size),
                          (rc_view_bytes) {.data = packed, .num = packed_size});
    if (mipmaps) {
        rc_gfx_texture_generate_mipmaps(tex);
    }
    rc_arena_free(gfx.arena, packed, packed_size);
    return tex;
}

/* ---- samplers ---- */

rc_gfx_sampler rc_gfx_sampler_make(const rc_gfx_sampler_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    rc_gfx_sampler handle = {.h = rc_gfx_sampler_pool_alloc(&gfx.sampler_pool, gfx.arena)};
    rc_gfx_sampler_pool_set(&gfx.sampler_pool, handle.h, (rc_gfx_sampler_obj) {
        .comparison = desc->compare != RC_GFX_COMPARE_ALWAYS,
    });
    rc_gfx_backend_make_sampler(rc_gfx_sampler_pool_at(&gfx.sampler_pool, handle.h), desc);
    return handle;
}

void rc_gfx_sampler_destroy(rc_gfx_sampler smp)
{
    gfx_check_context_thread();
    rc_gfx_sampler_obj obj = rc_gfx_sampler_pool_get(&gfx.sampler_pool, smp.h);
    gfx_queue_release(RC_GFX_RELEASE_SAMPLER, obj.gl_sampler);
    rc_gfx_sampler_pool_free(&gfx.sampler_pool, smp.h);
}

/* ---- shaders ---- */

rc_gfx_shader rc_gfx_shader_make(const rc_gfx_shader_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    RC_ASSERT(rc_str_is_valid(desc->vs_source) && desc->vs_source.len > 0);
    RC_ASSERT(rc_str_is_valid(desc->fs_source) && desc->fs_source.len > 0);
    RC_ASSERT(desc->uniform_block_count == 0 || desc->uniform_blocks != NULL);
    RC_ASSERT(desc->texture_sampler_count == 0 || desc->texture_samplers != NULL);

    rc_gfx_shader handle = {.h = rc_gfx_shader_pool_alloc(&gfx.shader_pool, gfx.arena)};
    rc_gfx_shader_obj *obj = rc_gfx_shader_pool_at(&gfx.shader_pool, handle.h);
    obj->block_count = desc->uniform_block_count;
    obj->pair_count = desc->texture_sampler_count;
    if (obj->block_count > 0) {
        obj->blocks = rc_arena_alloc_zero(gfx.arena,
            obj->block_count * (uint32_t)sizeof(rc_gfx_shader_block_info));
        for (uint32_t i = 0; i < obj->block_count; i += 1) {
            const rc_gfx_shader_uniform_block *block = &desc->uniform_blocks[i];
            RC_ASSERT(block->group < RC_GFX_MAX_BIND_GROUPS);
            RC_ASSERT(block->binding < RC_GFX_MAX_BINDINGS_PER_GROUP);
            RC_ASSERT(block->size > 0);
            obj->blocks[i] = (rc_gfx_shader_block_info) {
                .group = block->group,
                .binding = block->binding,
                .size = block->size,
            };
        }
    }
    if (obj->pair_count > 0) {
        obj->pairs = rc_arena_alloc_zero(gfx.arena,
            obj->pair_count * (uint32_t)sizeof(rc_gfx_shader_pair_info));
        for (uint32_t i = 0; i < obj->pair_count; i += 1) {
            const rc_gfx_shader_texture_sampler_pair *pair = &desc->texture_samplers[i];
            RC_ASSERT(pair->texture_group < RC_GFX_MAX_BIND_GROUPS);
            RC_ASSERT(pair->texture_binding < RC_GFX_MAX_BINDINGS_PER_GROUP);
            obj->pairs[i] = (rc_gfx_shader_pair_info) {
                .texture_group = pair->texture_group,
                .texture_binding = pair->texture_binding,
                .sampler_group = pair->sampler_group,
                .sampler_binding = pair->sampler_binding,
            };
        }
    }
    rc_gfx_backend_make_shader(obj, desc);
    return handle;
}

void rc_gfx_shader_destroy(rc_gfx_shader shd)
{
    gfx_check_context_thread();
    rc_gfx_shader_obj obj = rc_gfx_shader_pool_get(&gfx.shader_pool, shd.h);
    gfx_queue_release(RC_GFX_RELEASE_PROGRAM, obj.gl_program);
    rc_gfx_shader_pool_free(&gfx.shader_pool, shd.h);
}

/* ---- bind group layouts ---- */

rc_gfx_bind_group_layout rc_gfx_bind_group_layout_make(const rc_gfx_bind_group_layout_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->entry_count > 0 && desc->entry_count <= RC_GFX_MAX_BINDINGS_PER_GROUP);

    rc_gfx_bind_group_layout handle = {.h = rc_gfx_bind_group_layout_pool_alloc(&gfx.bind_group_layout_pool, gfx.arena)};
    rc_gfx_bind_group_layout_obj *obj = rc_gfx_bind_group_layout_pool_at(&gfx.bind_group_layout_pool, handle.h);
    obj->entry_count = desc->entry_count;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < desc->entry_count; i += 1) {
        const rc_gfx_bind_group_layout_entry *entry = &desc->entries[i];
        RC_ASSERT(entry->binding < RC_GFX_MAX_BINDINGS_PER_GROUP);
        RC_ASSERT((seen & (1u << entry->binding)) == 0);   // bindings must be unique
        seen |= 1u << entry->binding;
        RC_ASSERT(entry->visibility != 0);
        if (entry->type == RC_GFX_BINDING_STORAGE_BUFFER_READ) {
            RC_ASSERT(entry->texel_format != RC_GFX_TEXTURE_FORMAT_NONE);
            RC_ASSERT(!rc_gfx_texture_format_is_depth(entry->texel_format));
            RC_ASSERT(!rc_gfx_texture_format_is_compressed(entry->texel_format));
        }
        obj->entries[i] = *entry;
    }
    return handle;
}

void rc_gfx_bind_group_layout_destroy(rc_gfx_bind_group_layout layout)
{
    gfx_check_context_thread();
    // pool_free asserts the handle is valid, so no separate resolve is needed
    rc_gfx_bind_group_layout_pool_free(&gfx.bind_group_layout_pool, layout.h);
}

/* ---- pipeline layouts ---- */

static rc_gfx_pipeline_layout pipeline_layout_make_impl(const rc_gfx_pipeline_layout_desc *desc,
                                                        bool owns_group0)
{
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->group_count <= RC_GFX_MAX_BIND_GROUPS);   // 0 groups is a valid empty layout

    rc_gfx_pipeline_layout handle = {.h = rc_gfx_pipeline_layout_pool_alloc(&gfx.pipeline_layout_pool, gfx.arena)};
    rc_gfx_pipeline_layout_obj *obj = rc_gfx_pipeline_layout_pool_at(&gfx.pipeline_layout_pool, handle.h);
    obj->group_count = desc->group_count;
    obj->owns_group0 = owns_group0;

    // flatten (group, binding) to sequential GL units per binding class,
    // walking groups in order (see the backend notes in the design)
    uint32_t next_ubo = 0;
    uint32_t next_tex = 0;
    for (uint32_t g = 0; g < desc->group_count; g += 1) {
        rc_gfx_bind_group_layout_obj *group = rc_gfx_bind_group_layout_pool_at(&gfx.bind_group_layout_pool, desc->groups[g].h);
        RC_ASSERT(group != NULL);   // null or stale handle
        obj->groups[g] = desc->groups[g];
        for (uint32_t e = 0; e < group->entry_count; e += 1) {
            switch (group->entries[e].type) {
            case RC_GFX_BINDING_UNIFORM_BUFFER:
                obj->flat_unit[g][e] = (uint8_t)next_ubo;
                next_ubo += 1;
                break;
            case RC_GFX_BINDING_TEXTURE:
            case RC_GFX_BINDING_STORAGE_BUFFER_READ:
                obj->flat_unit[g][e] = (uint8_t)next_tex;
                next_tex += 1;
                break;
            case RC_GFX_BINDING_SAMPLER:
            case RC_GFX_BINDING_COMPARISON_SAMPLER:
                obj->flat_unit[g][e] = 0xFF;   // samplers bind via their paired texture's unit
                break;
            default:
                RC_UNREACHABLE();
            }
        }
    }
    obj->ubo_count = next_ubo;
    obj->tex_count = next_tex;

    uint32_t h = rc_hash_bytes(obj->flat_unit, (uint32_t)sizeof(obj->flat_unit));
    h = rc_hash_combine(h, rc_hash_u32(next_ubo * 97u + next_tex));
    obj->mapping_hash = ((uint64_t)h << 32) | h | 1u;   // never 0, so 0 can mean unresolved
    return handle;
}

rc_gfx_pipeline_layout rc_gfx_pipeline_layout_make(const rc_gfx_pipeline_layout_desc *desc)
{
    gfx_check_context_thread();
    return pipeline_layout_make_impl(desc, false);
}

void rc_gfx_pipeline_layout_destroy(rc_gfx_pipeline_layout layout)
{
    gfx_check_context_thread();
    rc_gfx_pipeline_layout_obj obj = rc_gfx_pipeline_layout_pool_get(&gfx.pipeline_layout_pool, layout.h);
    if (obj.owns_group0) {
        rc_gfx_bind_group_layout_destroy(obj.groups[0]);
    }
    rc_gfx_pipeline_layout_pool_free(&gfx.pipeline_layout_pool, layout.h);
}

rc_gfx_simple_layout rc_gfx_simple_layout_make(
    const rc_gfx_bind_group_layout_entry *entries, uint32_t entry_count, rc_str label)
{
    gfx_check_context_thread();
    RC_ASSERT(entries != NULL && entry_count > 0 && entry_count <= RC_GFX_MAX_BINDINGS_PER_GROUP);

    rc_gfx_bind_group_layout_desc group_desc = {.entry_count = entry_count, .label = label};
    for (uint32_t i = 0; i < entry_count; i += 1) {
        group_desc.entries[i] = entries[i];
    }
    rc_gfx_bind_group_layout group0 = rc_gfx_bind_group_layout_make(&group_desc);

    rc_gfx_pipeline_layout layout = pipeline_layout_make_impl(&(rc_gfx_pipeline_layout_desc) {
        .groups = {group0},
        .group_count = 1,
        .label = label,
    }, true);
    return (rc_gfx_simple_layout) {.layout = layout, .group0 = group0};
}

/* ---- bind groups ---- */

rc_gfx_bind_group rc_gfx_bind_group_make(const rc_gfx_bind_group_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    rc_gfx_bind_group_layout_obj *layout = rc_gfx_bind_group_layout_pool_at(&gfx.bind_group_layout_pool, desc->layout.h);
    RC_ASSERT(layout != NULL);   // null or stale handle
    RC_ASSERT(desc->entry_count == layout->entry_count);

    rc_gfx_bind_group handle = {.h = rc_gfx_bind_group_pool_alloc(&gfx.bind_group_pool, gfx.arena)};
    rc_gfx_bind_group_obj *obj = rc_gfx_bind_group_pool_at(&gfx.bind_group_pool, handle.h);
    obj->layout = desc->layout;
    obj->entry_count = layout->entry_count;

    // resolve each layout entry against the matching desc entry; entries are
    // stored in layout order (the flat-unit tables are parallel to it), and
    // dynamic offsets are consumed in that order at playback
    for (uint32_t e = 0; e < layout->entry_count; e += 1) {
        const rc_gfx_bind_group_layout_entry *lentry = &layout->entries[e];
        const rc_gfx_bind_group_entry *dentry = NULL;
        for (uint32_t i = 0; i < desc->entry_count; i += 1) {
            if (desc->entries[i].binding == lentry->binding) {
                dentry = &desc->entries[i];
                break;
            }
        }
        RC_ASSERT(dentry != NULL);   // every layout binding must be supplied

        rc_gfx_bind_entry_resolved resolved = {
            .binding = lentry->binding,
            .type = lentry->type,
            .has_dynamic_offset = lentry->has_dynamic_offset,
        };
        switch (lentry->type) {
        case RC_GFX_BINDING_UNIFORM_BUFFER: {
            rc_gfx_buffer_obj buffer = rc_gfx_buffer_pool_get(&gfx.buffer_pool, dentry->buffer.h);
            RC_ASSERT(buffer.usage & RC_GFX_BUFFER_USAGE_UNIFORM);
            RC_ASSERT(dentry->buffer_offset % RC_GFX_UNIFORM_ALIGN == 0);
            uint32_t size = dentry->buffer_size != 0
                ? dentry->buffer_size
                : buffer.size - dentry->buffer_offset;
            RC_ASSERT(dentry->buffer_offset + (uint64_t)size <= buffer.size
                      || lentry->has_dynamic_offset);
            if (lentry->min_binding_size != 0) {
                RC_ASSERT(size >= lentry->min_binding_size);
            }
            resolved.gl_object = buffer.gl_buffer;
            resolved.offset = dentry->buffer_offset;
            resolved.size = size;
            if (lentry->has_dynamic_offset) {
                obj->dynamic_count += 1;
            }
            break;
        }
        case RC_GFX_BINDING_TEXTURE: {
            rc_gfx_texture_obj texture = rc_gfx_texture_pool_get(&gfx.texture_pool, dentry->texture.h);
            RC_ASSERT(texture.usage & RC_GFX_TEXTURE_USAGE_SAMPLED);
            if (gfx.validation) {
                RC_ASSERT(texture.dim == lentry->texture_dim);
                RC_ASSERT(lentry->multisampled == (texture.sample_count > 1));
                bool is_depth = rc_gfx_texture_format_is_depth(texture.format);
                RC_ASSERT((lentry->sample_type == RC_GFX_TEXTURE_SAMPLE_DEPTH) == is_depth);
            }
            resolved.gl_object = texture.gl_texture;
            resolved.gl_target = texture.gl_target;
            break;
        }
        case RC_GFX_BINDING_SAMPLER:
        case RC_GFX_BINDING_COMPARISON_SAMPLER: {
            rc_gfx_sampler_obj sampler = rc_gfx_sampler_pool_get(&gfx.sampler_pool, dentry->sampler.h);
            RC_ASSERT(sampler.comparison == (lentry->type == RC_GFX_BINDING_COMPARISON_SAMPLER));
            resolved.gl_object = sampler.gl_sampler;
            break;
        }
        case RC_GFX_BINDING_STORAGE_BUFFER_READ: {
            rc_gfx_buffer_obj buffer = rc_gfx_buffer_pool_get(&gfx.buffer_pool, dentry->buffer.h);
            RC_ASSERT(buffer.usage & RC_GFX_BUFFER_USAGE_STORAGE);
            RC_ASSERT(dentry->buffer_offset == 0 && dentry->buffer_size == 0);   // whole buffer on GL 3.3
            resolved.gl_object = rc_gfx_backend_make_tbo(buffer.gl_buffer, lentry->texel_format);
            break;
        }
        default:
            RC_UNREACHABLE();
        }
        obj->entries[e] = resolved;
    }
    return handle;
}

void rc_gfx_bind_group_destroy(rc_gfx_bind_group group)
{
    gfx_check_context_thread();
    rc_gfx_bind_group_obj *obj = rc_gfx_bind_group_pool_at(&gfx.bind_group_pool, group.h);
    RC_ASSERT(obj != NULL);   // null or stale handle
    for (uint32_t e = 0; e < obj->entry_count; e += 1) {
        if (obj->entries[e].type == RC_GFX_BINDING_STORAGE_BUFFER_READ) {
            gfx_queue_release(RC_GFX_RELEASE_TEXTURE, obj->entries[e].gl_object);
        }
    }
    rc_gfx_bind_group_pool_free(&gfx.bind_group_pool, group.h);
}

/* ---- pipelines ---- */

static uint32_t vertex_format_size(rc_gfx_vertex_format fmt)
{
    switch (fmt) {
    case RC_GFX_VERTEX_FORMAT_U8X2: case RC_GFX_VERTEX_FORMAT_I8X2:
    case RC_GFX_VERTEX_FORMAT_U8X2_NORM: case RC_GFX_VERTEX_FORMAT_I8X2_NORM:
        return 2;
    case RC_GFX_VERTEX_FORMAT_U8X4: case RC_GFX_VERTEX_FORMAT_I8X4:
    case RC_GFX_VERTEX_FORMAT_U8X4_NORM: case RC_GFX_VERTEX_FORMAT_I8X4_NORM:
    case RC_GFX_VERTEX_FORMAT_U16X2: case RC_GFX_VERTEX_FORMAT_I16X2:
    case RC_GFX_VERTEX_FORMAT_U16X2_NORM: case RC_GFX_VERTEX_FORMAT_I16X2_NORM:
    case RC_GFX_VERTEX_FORMAT_F16X2:
    case RC_GFX_VERTEX_FORMAT_F32: case RC_GFX_VERTEX_FORMAT_U32: case RC_GFX_VERTEX_FORMAT_I32:
    case RC_GFX_VERTEX_FORMAT_RGB10A2_NORM:
        return 4;
    case RC_GFX_VERTEX_FORMAT_U16X4: case RC_GFX_VERTEX_FORMAT_I16X4:
    case RC_GFX_VERTEX_FORMAT_U16X4_NORM: case RC_GFX_VERTEX_FORMAT_I16X4_NORM:
    case RC_GFX_VERTEX_FORMAT_F16X4:
    case RC_GFX_VERTEX_FORMAT_F32X2: case RC_GFX_VERTEX_FORMAT_U32X2: case RC_GFX_VERTEX_FORMAT_I32X2:
        return 8;
    case RC_GFX_VERTEX_FORMAT_F32X3: case RC_GFX_VERTEX_FORMAT_U32X3: case RC_GFX_VERTEX_FORMAT_I32X3:
        return 12;
    case RC_GFX_VERTEX_FORMAT_F32X4: case RC_GFX_VERTEX_FORMAT_U32X4: case RC_GFX_VERTEX_FORMAT_I32X4:
        return 16;
    default:
        RC_UNREACHABLE();
    }
}

static bool blend_state_equal(const rc_gfx_blend_state *a, const rc_gfx_blend_state *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

rc_gfx_pipeline rc_gfx_pipeline_make(const rc_gfx_pipeline_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    rc_gfx_shader_obj *shader = rc_gfx_shader_pool_at(&gfx.shader_pool, desc->shader.h);
    RC_ASSERT(shader != NULL);   // null or stale handle
    rc_gfx_pipeline_layout_obj *layout = rc_gfx_pipeline_layout_pool_at(&gfx.pipeline_layout_pool, desc->layout.h);
    RC_ASSERT(layout != NULL);   // null or stale handle
    RC_ASSERT(desc->color_count <= RC_GFX_MAX_COLOR_ATTACHMENTS);

    rc_gfx_pipeline handle = {.h = rc_gfx_pipeline_pool_alloc(&gfx.pipeline_pool, gfx.arena)};
    rc_gfx_pipeline_obj *obj = rc_gfx_pipeline_pool_at(&gfx.pipeline_pool, handle.h);
    obj->shader = desc->shader;
    obj->layout = desc->layout;
    obj->gl_program = shader->gl_program;
    obj->primitive = desc->primitive;
    obj->index_format = desc->index_format;
    obj->cull = desc->cull;
    obj->front_face = desc->front_face;
    obj->color_count = desc->color_count;
    obj->depth_stencil = desc->depth_stencil;
    obj->sample_count = desc->sample_count > 1 ? desc->sample_count : 1;
    obj->alpha_to_coverage = desc->alpha_to_coverage;

    for (uint32_t i = 0; i < desc->color_count; i += 1) {
        RC_ASSERT(desc->colors[i].format != RC_GFX_TEXTURE_FORMAT_NONE);
        obj->colors[i] = desc->colors[i];
        // GL 3.3 has one blend function for all targets (per-target enables
        // only), so every enabled target must share target 0's blend state
        if (i > 0 && desc->colors[i].blend.enabled) {
            RC_ASSERT(blend_state_equal(&desc->colors[i].blend, &desc->colors[0].blend));
        }
    }
    if (desc->depth_stencil.format != RC_GFX_TEXTURE_FORMAT_NONE) {
        RC_ASSERT(rc_gfx_texture_format_is_depth(desc->depth_stencil.format));
    }

    // resolve the vertex layout: offset 0 means packed after the previous
    // attribute of the same buffer, stride 0 means computed from the packing
    uint32_t running[RC_GFX_MAX_VERTEX_BUFFERS] = {0};
    for (uint32_t i = 0; i < RC_GFX_MAX_VERTEX_ATTRIBUTES; i += 1) {
        const rc_gfx_vertex_attribute *attr = &desc->vertex_layout.attributes[i];
        if (attr->format == RC_GFX_VERTEX_FORMAT_NONE) {
            continue;
        }
        RC_ASSERT(attr->buffer_index < RC_GFX_MAX_VERTEX_BUFFERS);
        uint32_t offset = attr->offset != 0 ? attr->offset : running[attr->buffer_index];
        running[attr->buffer_index] = offset + vertex_format_size(attr->format);
        obj->attrs[obj->attr_count] = (rc_gfx_vertex_attribute) {
            .location = attr->location,
            .buffer_index = attr->buffer_index,
            .offset = offset,
            .format = attr->format,
        };
        obj->attr_count += 1;
        obj->vbuf_mask |= 1u << attr->buffer_index;
    }
    for (uint32_t b = 0; b < RC_GFX_MAX_VERTEX_BUFFERS; b += 1) {
        const rc_gfx_vertex_buffer_layout *buffer = &desc->vertex_layout.buffers[b];
        obj->strides[b] = buffer->stride != 0 ? buffer->stride : running[b];
        obj->divisors[b] = buffer->per_instance
            ? (buffer->step_rate != 0 ? buffer->step_rate : 1)
            : 0;
    }

    // validate the shader's bindings against the layout, then bind the
    // program's blocks and sampler uniforms to the layout's flat units
    for (uint32_t i = 0; i < shader->block_count; i += 1) {
        const rc_gfx_shader_block_info *block = &shader->blocks[i];
        RC_ASSERT(block->group < layout->group_count);
        rc_gfx_bind_group_layout_obj *group = rc_gfx_bind_group_layout_pool_at(&gfx.bind_group_layout_pool, layout->groups[block->group].h);
        RC_ASSERT(group != NULL);   // null or stale handle
        bool found = false;
        for (uint32_t e = 0; e < group->entry_count; e += 1) {
            if (group->entries[e].binding != block->binding) {
                continue;
            }
            RC_ASSERT(group->entries[e].type == RC_GFX_BINDING_UNIFORM_BUFFER);
            if (group->entries[e].min_binding_size != 0) {
                RC_ASSERT(group->entries[e].min_binding_size >= block->size);
            }
            found = true;
            break;
        }
        RC_ASSERT(found);
        (void)found;
    }
    if (shader->resolved_hash == 0) {
        rc_gfx_backend_resolve_shader(shader, layout);
        shader->resolved_hash = layout->mapping_hash;
    } else {
        // a shader may be shared by pipelines only across layouts with an
        // identical flat mapping - GL block bindings are program state
        RC_ASSERT(shader->resolved_hash == layout->mapping_hash);
    }
    return handle;
}

void rc_gfx_pipeline_destroy(rc_gfx_pipeline pip)
{
    gfx_check_context_thread();
    rc_gfx_backend_evict_pipeline(pip);
    // pool_free asserts the handle is valid, so no separate resolve is needed
    rc_gfx_pipeline_pool_free(&gfx.pipeline_pool, pip.h);
}

rc_gfx_blend_state rc_gfx_blend_state_make_alpha(void)
{
    return (rc_gfx_blend_state) {
        .enabled = true,
        .src_factor = RC_GFX_BLEND_FACTOR_SRC_ALPHA,
        .dst_factor = RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .src_alpha_factor = RC_GFX_BLEND_FACTOR_ONE,
        .dst_alpha_factor = RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    };
}

rc_gfx_blend_state rc_gfx_blend_state_make_premultiplied(void)
{
    return (rc_gfx_blend_state) {
        .enabled = true,
        .src_factor = RC_GFX_BLEND_FACTOR_ONE,
        .dst_factor = RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .src_alpha_factor = RC_GFX_BLEND_FACTOR_ONE,
        .dst_alpha_factor = RC_GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    };
}

rc_gfx_blend_state rc_gfx_blend_state_make_additive(void)
{
    return (rc_gfx_blend_state) {
        .enabled = true,
        .src_factor = RC_GFX_BLEND_FACTOR_ONE,
        .dst_factor = RC_GFX_BLEND_FACTOR_ONE,
        .src_alpha_factor = RC_GFX_BLEND_FACTOR_ONE,
        .dst_alpha_factor = RC_GFX_BLEND_FACTOR_ONE,
    };
}

/* ---- render targets ---- */

rc_gfx_render_target rc_gfx_render_target_make(const rc_gfx_render_target_desc *desc)
{
    gfx_check_context_thread();
    RC_ASSERT(desc != NULL);
    RC_ASSERT(desc->color_count <= RC_GFX_MAX_COLOR_ATTACHMENTS);
    RC_ASSERT(desc->color_count > 0 || !rc_genpool_handle_is_null(desc->depth_stencil.texture.h));

    rc_gfx_render_target handle = {.h = rc_gfx_render_target_pool_alloc(&gfx.render_target_pool, gfx.arena)};
    rc_gfx_render_target_obj *obj = rc_gfx_render_target_pool_at(&gfx.render_target_pool, handle.h);
    obj->color_count = desc->color_count;
    obj->depth_format = RC_GFX_TEXTURE_FORMAT_NONE;

    rc_vec2i size = {0};
    uint32_t samples = 0;
    for (uint32_t i = 0; i < desc->color_count; i += 1) {
        const rc_gfx_attachment *attachment = &desc->colors[i];
        rc_gfx_texture_obj texture = rc_gfx_texture_pool_get(&gfx.texture_pool, attachment->texture.h);
        RC_ASSERT(texture.usage & RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT);
        RC_ASSERT(attachment->mip < texture.mip_count);
        rc_vec2i attach_size = mip_size(texture.size, attachment->mip);
        if (i == 0 && samples == 0) {
            size = attach_size;
            samples = texture.sample_count;
        }
        RC_ASSERT(attach_size.x == size.x && attach_size.y == size.y);
        RC_ASSERT(texture.sample_count == samples);
        obj->color_formats[i] = texture.format;

        if (!rc_genpool_handle_is_null(desc->resolves[i].texture.h)) {
            RC_ASSERT(samples > 1);
            rc_gfx_texture_obj resolve = rc_gfx_texture_pool_get(&gfx.texture_pool, desc->resolves[i].texture.h);
            RC_ASSERT(resolve.sample_count == 1);
            RC_ASSERT(resolve.format == texture.format);
            RC_ASSERT(resolve.usage & RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT);
            obj->resolve_mask |= 1u << i;
        }
    }
    if (!rc_genpool_handle_is_null(desc->depth_stencil.texture.h)) {
        rc_gfx_texture_obj depth = rc_gfx_texture_pool_get(&gfx.texture_pool, desc->depth_stencil.texture.h);
        RC_ASSERT(rc_gfx_texture_format_is_depth(depth.format));
        RC_ASSERT(depth.usage & RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT);
        rc_vec2i attach_size = mip_size(depth.size, desc->depth_stencil.mip);
        if (desc->color_count == 0) {
            size = attach_size;
            samples = depth.sample_count;
        }
        RC_ASSERT(attach_size.x == size.x && attach_size.y == size.y);
        RC_ASSERT(depth.sample_count == samples);
        obj->depth_format = depth.format;
    }
    obj->size = size;
    obj->sample_count = samples;

    rc_gfx_backend_make_render_target(obj, desc);
    return handle;
}

void rc_gfx_render_target_destroy(rc_gfx_render_target rt)
{
    gfx_check_context_thread();
    rc_gfx_render_target_obj obj = rc_gfx_render_target_pool_get(&gfx.render_target_pool, rt.h);
    gfx_queue_release(RC_GFX_RELEASE_FBO, obj.gl_fbo);
    gfx_queue_release(RC_GFX_RELEASE_FBO, obj.gl_resolve_fbo);
    rc_gfx_render_target_pool_free(&gfx.render_target_pool, rt.h);
}

rc_vec2i rc_gfx_render_target_size(rc_gfx_render_target rt)
{
    gfx_check_context_thread();
    return rc_gfx_render_target_pool_get(&gfx.render_target_pool, rt.h).size;
}
