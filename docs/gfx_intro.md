# An introduction to richc gfx

This is a tutorial for the gfx layer, written for someone using it for the
first time. It builds up from an empty window to offscreen rendering, MSAA,
instancing, and storage buffers, with working code at every step. It explains
concepts as they are needed; the exhaustive per-header reference lives in the
gfx section of `docs/app.md`, and the two example programs
(`src/app/example/hellotriangle/` and `src/app/example/cubes/`) are complete,
running versions of everything shown here.

## 1. The mental model

gfx is a backend-agnostic GPU layer. Its object model follows WebGPU - the
formalised intersection of D3D12, Vulkan, and Metal - so code written against
it maps almost directly onto any modern API. The current backend is OpenGL 3.3
core, chosen at compile time; your code never mentions GL and never changes
when the backend does.

The vocabulary, in the order you will meet it:

| Object | What it is |
|--------|------------|
| buffer | a block of GPU memory (vertices, indices, uniforms, storage) |
| texture | an image the GPU can sample or render into |
| sampler | how a texture is filtered and addressed (separate from the texture) |
| shader | a compiled vertex + fragment program |
| bind group layout | the *shape* of a set of shader resources |
| bind group | an actual set of resources matching a layout |
| pipeline layout | which bind group layouts a pipeline uses, in group order |
| pipeline | shader + vertex layout + all render state, baked immutably |
| render target | a set of textures to render into (the screen is implicit) |
| encoder | records passes and draws into a command buffer, then you submit it |

Three habits to internalise now:

- **Everything is created from a descriptor struct, and zero means default.**
  You fill in only the fields you care about with a designated-initializer
  compound literal; `{0}` is always a sane starting point.
- **Objects are referenced by small typed handles**, not pointers. `{0}` is
  the "none" handle. Destroying an object invalidates its handles: using a
  stale handle traps in debug builds instead of silently touching whatever
  reused the slot.
- **Creation is expensive and immutable; per-frame work is cheap.** Pipelines,
  layouts, and bind groups are made once at load time. Each frame you only
  record commands and write uniform data.

## 2. A window and a device

gfx does not create a window or a GL context - the app layer does that, and
gfx attaches to whatever context is current. The skeleton every program shares:

```c
#include "richc/app/app.h"
#include "richc/gfx/gfx.h"

static struct {
    rc_arena arena;         /* persistent: gfx and resources live here */
    rc_arena frame_arena;   /* rewound every frame: command encoding */
} state;

static void on_render(void *ctx, rc_vec2i size)
{
    (void)ctx;

    rc_gfx_begin_frame(size);
    rc_arena_reset(&state.frame_arena);

    // ... record and submit (next section) ...

    rc_gfx_end_frame();
}

int main(void)
{
    state.arena = rc_arena_make_default();
    state.frame_arena = rc_arena_make_default();

    rc_app_init(&(rc_app_desc) {
        .title = RC_STR("my app"),
        .size = {1280, 720},
        .resizable = true,
        .callbacks = {.on_render = on_render},
    });

    rc_gfx_init(&(rc_gfx_desc) {
        .arena = &state.arena,
    });

    while (rc_app_is_running()) {
        rc_app_poll();
        rc_app_request_render();
    }

    rc_gfx_shutdown();
    rc_app_destroy();
    return 0;
}
```

`rc_gfx_desc` has a few more knobs, all optional: `color_space` (sRGB output
by default; `RC_GFX_COLOR_SPACE_LINEAR` for bit-exact output such as an
emulator framebuffer), `swapchain_depth_format` (a depth buffer for the
window itself, section 11; colour-only by default), `swapchain_sample_count`
(MSAA for the window, section 12), `uniform_ring_size` (section 6; default
1 MB per in-flight frame), and `validation` (extra load-time checks; always
on in debug builds).

Per frame, `on_render` receives the framebuffer size in physical pixels and
is only invoked while that size is non-zero (a minimised window renders
nothing), so it can be passed straight on. `rc_gfx_begin_frame(size)` takes
it - the window-sized swapchain texture follows it automatically on resize -
and `rc_gfx_end_frame()` presents. Forwarding the size every frame is the
whole resize story for the swapchain.

Threading rule: resource creation and submission happen on the thread that
owns the context (the one that called `rc_app_init`). Command *recording* may
happen on any thread (section 13).

## 3. Hello triangle

Everything in gfx converges on one loop: create resources once, then each
frame record a pass into an encoder and submit it. This section walks the
whole path once; every later section just swaps in a deeper version of one
step. The full program is `src/app/example/hellotriangle/main.c`.

### 3.1 A vertex buffer

```c
typedef struct vertex {
    rc_vec2f pos;
    rc_vec4f color;      /* linear */
} vertex;

/* Canonical NDC: y up, so the apex is at the TOP of the window. */
static const vertex vertices[3] = {
    {.pos = {0.0f, 0.6f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
    {.pos = {-0.6f, -0.5f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    {.pos = {0.6f, -0.5f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
};

rc_gfx_buffer vbuf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
    .size = sizeof(vertices),
    .usage = RC_GFX_BUFFER_USAGE_VERTEX,
    .data = {.data = (const uint8_t *)vertices, .num = sizeof(vertices)},
    .label = RC_STR("triangle vertices"),
});
```

The default update mode is `RC_GFX_BUFFER_UPDATE_IMMUTABLE`: contents are
supplied at creation and never change, which is what static geometry wants.
Section 5 covers the mutable modes. The `label` appears in debug dumps and
costs nothing; get in the habit of setting it.

### 3.2 A shader

Shaders are GLSL 330 *bodies* - the backend prepends its own preamble, so
three rules apply:

1. **No `#version` line.** The preamble supplies it.
2. **Write `gl_Position` through `rc_clip(...)`.** This injected function is
   the single point where clip-space differences between backends are
   absorbed. Your math targets the canonical conventions (section 4) and
   never changes.
3. **All constants live in std140 uniform blocks** declared without an
   instance name. There are no loose uniforms, and never any gamma math.

```c
typedef struct frame_uniforms {
    rc_mat44f mvp;
} frame_uniforms;

static const char vs_src[] =
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec4 a_color;\n"
    "out vec4 v_color;\n"
    "layout(std140) uniform FrameUniforms {\n"
    "    mat4 u_mvp;\n"
    "};\n"
    "void main() {\n"
    "    v_color = a_color;\n"
    "    gl_Position = rc_clip(u_mvp * vec4(a_pos, 0.0, 1.0));\n"
    "}\n";

static const char fs_src[] =
    "in vec4 v_color;\n"
    "out vec4 o_color;\n"
    "void main() {\n"
    "    o_color = v_color;\n"
    "}\n";

rc_gfx_shader shader = rc_gfx_shader_make(&(rc_gfx_shader_desc) {
    .vs_source = RC_STR(vs_src),
    .fs_source = RC_STR(fs_src),
    .uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
        {
            .glsl_name = RC_STR("FrameUniforms"),
            .group = 0,
            .binding = 0,
            .size = sizeof(frame_uniforms),
        },
    },
    .uniform_block_count = 1,
    .label = RC_STR("triangle shader"),
});
```

The `uniform_blocks` table tells gfx which GLSL block corresponds to which
(group, binding) slot and how big the matching C struct is. Vertex attributes
are matched by `layout(location = N)` only - names do not matter.

### 3.3 A layout and a bind group

A *bind group layout* declares the shape of a resource set: which bindings
exist, their types, and which shader stages see them. A *bind group* then
supplies actual resources in that shape. A *pipeline layout* lists up to four
group layouts, one per group index.

Most programs start with a single group, and there is a one-call shortcut for
that:

```c
rc_gfx_simple_layout layout = rc_gfx_simple_layout_make(
    (const rc_gfx_bind_group_layout_entry[]) {
        {
            .binding = 0,
            .visibility = RC_GFX_STAGE_VERTEX,
            .type = RC_GFX_BINDING_UNIFORM_BUFFER,
            .has_dynamic_offset = true,
            .min_binding_size = sizeof(frame_uniforms),
        },
    },
    1,
    RC_STR("triangle layout"));
// layout.layout is the pipeline layout, layout.group0 the group layout
```

Set `min_binding_size` whenever you can: it is validated at bind-group
creation and catches std140 sizing bugs at load time instead of as garbage on
screen.

The bind group references the *uniform ring* buffer (explained properly in
section 6 - for now: a per-frame scratch constant buffer gfx manages for you).
Because the binding has a dynamic offset, this one bind group is created once
and reused forever; the offset picks the live data each draw:

```c
rc_gfx_bind_group group0 = rc_gfx_bind_group_make(&(rc_gfx_bind_group_desc) {
    .layout = layout.group0,
    .entries = {
        {
            .binding = 0,
            .buffer = rc_gfx_uniform_buffer(),
            .buffer_size = sizeof(frame_uniforms),
        },
    },
    .entry_count = 1,
    .label = RC_STR("triangle bind group"),
});
```

Bind groups are immutable: you create them at load time, not per frame.

### 3.4 A pipeline

The pipeline bakes the shader, the vertex layout, and *all* render state,
including the colour formats it will render into - the D3D12/Vulkan "PSO
contract". A pipeline built for the swapchain declares the swapchain's format:

```c
rc_gfx_pipeline pip = rc_gfx_pipeline_make(&(rc_gfx_pipeline_desc) {
    .shader = shader,
    .layout = layout.layout,
    .vertex_layout = {
        .buffers = {
            {.stride = sizeof(vertex)},
        },
        .attributes = {
            {
                .location = 0,
                .buffer_index = 0,
                .offset = offsetof(vertex, pos),
                .format = RC_GFX_VERTEX_FORMAT_F32X2,
            },
            {
                .location = 1,
                .buffer_index = 0,
                .offset = offsetof(vertex, color),
                .format = RC_GFX_VERTEX_FORMAT_F32X4,
            },
        },
    },
    .colors = {
        {.format = rc_gfx_swapchain_format()},
    },
    .color_count = 1,
    .label = RC_STR("triangle pipeline"),
});
```

Everything not mentioned takes its default: triangles, no culling, CCW front
faces, no blending, no depth. The format/sample-count bake is validated when
the pipeline is bound in a pass, so a mismatch is a debug trap, not a black
screen.

### 3.5 Recording a frame

Inside `on_render`, between `rc_gfx_begin_frame` and `rc_gfx_end_frame`:

```c
rc_gfx_encoder *enc = rc_gfx_encoder_begin(&state.frame_arena);

// target {0} is the swapchain; load defaults to CLEAR, store to STORE
rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
    .colors = {
        {.clear_value = {0.2f, 0.2f, 0.2f, 1.0f}},   // linear!
    },
    .label = RC_STR("main"),
});

rc_gfx_uniform_alloc u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
*(frame_uniforms *)u.ptr = (frame_uniforms) {
    .mvp = rc_mat44f_make_identity(),
};

rc_gfx_encoder_set_pipeline(enc, pip);
rc_gfx_encoder_set_bind_group(enc, 0, group0, &u.offset, 1);
rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});

rc_gfx_encoder_pass_end(enc);

rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);
rc_gfx_submit(&cb, 1);
```

That is the entire per-frame shape of every gfx program: begin a pass, set a
pipeline, bind resources, draw, end the pass, submit. Viewport and scissor
default to the full target at pass begin; pipeline and binding state start
clean each pass.

Run it: a coloured triangle, apex at the top, on a grey background. Note the
grey: the clear value 0.2 is linear, and the sRGB swapchain encodes it to
roughly 124/255 on the way out - visibly brighter than the 51/255 you would
get if 0.2 were written raw. That single observation is the sRGB pipeline
working, which brings us to the two conventions you must actually learn.

## 4. The two conventions that matter

### 4.1 Coordinates

One convention, identical on every backend; the backend conforms, your math
never changes:

| Aspect | Rule |
|--------|------|
| NDC | x right, y **up**, both [-1, 1]; z in **[0, 1]** |
| Depth | **reverse-Z always**: near -> 1, far -> 0; compare GREATER_EQUAL; clear to 0.0 |
| Viewport / scissor | origin top-left, +y down, pixel units |
| Texture coordinates | origin top-left, v down; texel (0,0) first in memory |
| World / view space | right-handed: +X right, +Y up, -Z forward |
| Winding | CCW front faces (per-pipeline `front_face` for mirrored geometry) |
| Matrices | column-major, column vectors, `v' = M * v` |

The `rc_mat44f` projection constructors (`rc_mat44f_make_perspective_inf`,
`_make_perspective`, `_make_ortho`, `_make_ortho_2d`) all produce exactly this
clip space - reverse-Z, depth [0, 1] - so the depth pattern is always the same
trio: clear depth to `0.0`, compare `RC_GFX_COMPARE_GREATER_EQUAL`, use a
provided projection. Reverse-Z with a float depth buffer distributes precision
almost evenly with distance, which is why it is the only convention offered.

`rc_mat44f_make_ortho_2d(w, h)` deserves a note: it maps a pixel rectangle
with a top-left origin and y down - the natural space for UI - onto the
canonical NDC.

### 4.2 Colour is linear

Everything a shader reads, writes, blends, or clears is **linear**. Encoding
is a property of the texture format, applied by fixed-function hardware:
`_SRGB` formats decode on sample (before filtering) and encode on write
(after blending). Consequences:

- Clear values and the blend constant are linear, even for sRGB attachments.
  An artist's "mid grey" is 0.216 linear, not 0.5.
- Choose formats by content: albedo, UI art, and anything colour-like wants
  `_SRGB`; normal maps, masks, font atlases, and data want `_UNORM` or float.
- Hex colours from design tools are sRGB-encoded. Convert them once on the
  CPU with `richc/gfx/color.h`:

```c
#include "richc/gfx/color.h"
rc_vec4f tint = rc_gfx_color_from_srgb_u32(0xFF20A0E6u);   // 0xAABBGGRR
```

Never write gamma math in a shader. If colours look washed out or too dark,
the answer is a format or a conversion, not a `pow`.

## 5. Buffers

`rc_gfx_buffer_usage` flags say what a buffer can be bound as - `VERTEX`,
`INDEX`, `UNIFORM`, `STORAGE` (plus `COPY_SRC`/`COPY_DST`) - and the update
mode says how its contents behave:

| Mode | Meaning |
|------|---------|
| `IMMUTABLE` (default) | contents at creation, never changed |
| `DYNAMIC` | occasional updates |
| `STREAM` | rewritten every frame |

Mutable buffers are updated with `rc_gfx_buffer_update`, whole or in part:

```c
rc_gfx_buffer instance_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
    .size = sizeof(instances),
    .usage = RC_GFX_BUFFER_USAGE_VERTEX,
    .update = RC_GFX_BUFFER_UPDATE_STREAM,
    .label = RC_STR("instance data"),
});

// per frame:
rc_gfx_buffer_update(instance_buf, 0, (rc_view_bytes) {
    .data = (const uint8_t *)instances,
    .num = sizeof(instances),
});
```

An update takes effect at the point of the call in submission order; it cannot
land between a pass begin and end. For per-draw *constants* do not use
buffer updates at all - that is what the uniform ring is for.

## 6. Uniforms and the uniform ring

All shader constants are std140 uniform blocks backed by buffers. Before the
mechanics, be clear about what is shared and what is private, because the
system has three layers with three different owners:

- **Blocks belong to shaders.** Each shader declares its own uniform blocks,
  as many as it likes, and its desc maps them to (group, binding) slots.
  Shaders never share a block declaration or its data - two shaders need not
  even agree on what a binding index means, since every pipeline carries its
  own layout.
- **The ring is a shared allocator, nothing more.** gfx owns one internal
  *uniform ring*: a single buffer with `RC_GFX_FRAMES_IN_FLIGHT` rotating
  per-frame regions, written through a CPU-side shadow that is flushed to the
  GPU at submit. It has no layout of its own - think of it as malloc for
  constant data that lives exactly one frame. It is the only "single" thing
  in the system, and what it hands out are private slices.
- **Allocations belong to individual uses.** Every
  `rc_gfx_encoder_alloc_uniforms` call returns a fresh, independent,
  256-aligned slice of the current frame's region. The dynamic offset you
  pass at `set_bind_group` selects which slice that particular draw reads.
  Two shaders - or two hundred draws using one shader - each take their own
  slice and never see each other's data.

You never create the ring; you allocate from it while encoding:

```c
rc_gfx_uniform_alloc u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
*(frame_uniforms *)u.ptr = (frame_uniforms) { ... };   // write your std140 struct
rc_gfx_encoder_set_bind_group(enc, 0, group0, &u.offset, 1);
```

The plumbing that makes this work, set up once at load time: the bind group
entry points at `rc_gfx_uniform_buffer()` (the ring's handle) with
`buffer_size` = your block size, under a layout entry with
`has_dynamic_offset = true`. Because only the offset varies, the bind group
is created once and reused forever.

### Allocation granularity

Match allocations to how the data actually varies - the examples allocate
once per frame only because their single block (the camera) genuinely is
per-frame data, deliberately shared by every draw through the same offset:

```c
// per-frame data: allocate once, every draw passes the same offset
rc_gfx_uniform_alloc frame_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(frame_uniforms));
*(frame_uniforms *)frame_u.ptr = (frame_uniforms) {.view_proj = view_proj};
rc_gfx_encoder_set_bind_group(enc, 0, frame_group, &frame_u.offset, 1);

// per-draw data: allocate inside the loop, each draw gets its own offset
for (uint32_t i = 0; i < object_count; i += 1) {
    rc_gfx_uniform_alloc obj_u = rc_gfx_encoder_alloc_uniforms(enc, sizeof(object_uniforms));
    *(object_uniforms *)obj_u.ptr = (object_uniforms) {.model = objects[i].model};
    rc_gfx_encoder_set_bind_group(enc, 3, object_group, &obj_u.offset, 1);
    rc_gfx_encoder_draw_indexed(enc, &draw);
}
```

A typical renderer does both at once - a per-frame block in group 0 and a
per-draw block in group 3, following the four-group convention of section 8.
Allocations are cheap (an atomic bump), so one per draw is fine; if a frame
overflows its region, it is a panic telling you to raise `uniform_ring_size`
in `rc_gfx_desc`.

The ring is also entirely optional. Constants that do *not* change every
frame - material parameters fixed at load time, say - belong in your own
`UNIFORM`-usage buffer (IMMUTABLE, or DYNAMIC with `rc_gfx_buffer_update`),
bound in a bind group with a plain `buffer_offset` and no dynamic offset.
The ring earns its keep specifically for transient data, where it replaces
per-frame buffer churn with a pointer bump and one contiguous upload.

### Writing through the pointer

`*(frame_uniforms *)u.ptr = (frame_uniforms) {...}` looks like it builds a
struct and copies it, but it does not: a compound-literal assignment is
direct initialization of the destination, and compiles to plain stores
through `u.ptr`. It is exactly equivalent to the field-by-field form, which
reads better for incremental fills:

```c
frame_uniforms *fu = (frame_uniforms *)u.ptr;
fu->view_proj = view_proj;
fu->light_dir = light_dir;
```

The one real copy in the path is architectural, not syntactic: `u.ptr` points
into the ring's CPU-side shadow, and submit flushes the written range to the
GPU buffer in a single upload. That indirection is deliberate - it is what
makes `alloc_uniforms` safe to call from any recording thread (no GPU memory
mapping involved) and batches all uniform traffic per submit.

### std140 will bite you exactly once

std140 layout rules pad `vec3` to 16 bytes, round array strides up to 16, and
generally disagree with your C struct just often enough to hurt. Two defences
are built in:

- `min_binding_size` on the layout entry (section 3.3) validates overall size.
- The shader desc accepts a per-block **member table** which, in debug builds,
  validates every member's offset and size against GL's introspection of the
  compiled program - turning silent padding corruption into a load-time trap:

```c
.uniform_blocks = (const rc_gfx_shader_uniform_block[]) {
    {
        .glsl_name = RC_STR("FrameUniforms"),
        .group = 0, .binding = 0,
        .size = sizeof(frame_uniforms),
        .members = (const rc_gfx_uniform_member[]) {
            {.name = RC_STR("u_view_proj"), .offset = offsetof(frame_uniforms, view_proj), .size = sizeof(rc_mat44f)},
            {.name = RC_STR("u_light_dir"), .offset = offsetof(frame_uniforms, light_dir), .size = sizeof(rc_vec4f)},
        },
        .member_count = 2,
    },
},
```

Practical std140-safe struct advice: use `rc_mat44f` and `rc_vec4f` freely,
promote `vec3` + a float to one `rc_vec4f`, and keep scalars in groups of
four.

## 7. Textures and samplers

A texture is described by dimension (`2D` default, `2D_ARRAY`, `3D`, `CUBE`),
format, size, `depth` (array layers / 3D depth; a cube is 6), mips, and usage
(`SAMPLED` by default; add `RENDER_ATTACHMENT` to render into it). Initial
data can be supplied in the descriptor (`data.subresources`, indexed
`[slice * mip_count + mip]`, rows top-first and tightly packed), or uploaded
afterwards region by region. From the cubes example - a four-layer array
texture, generated procedurally and mipmapped:

```c
rc_gfx_texture tex = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
    .dim = RC_GFX_TEXTURE_DIM_2D_ARRAY,
    .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
    .size = {256, 256},
    .depth = 4,
    .mip_count = rc_gfx_mip_count((rc_vec2i) {256, 256}),
    .usage = RC_GFX_TEXTURE_USAGE_SAMPLED | RC_GFX_TEXTURE_USAGE_COPY_DST,
    .label = RC_STR("cube textures"),
});

for (uint32_t layer = 0; layer < 4; layer += 1) {
    rc_image img = rc_image_make((rc_vec2i) {256, 256}, RC_PIXEL_FORMAT_RGBA8, &scratch);
    // ... fill img with rc_image_set_pixel ...
    rc_gfx_texture_update(tex, 0, layer,
        rc_box2i_make_pos_size(rc_vec2i_make_zero(), img.size), img.data.view);
}
rc_gfx_texture_generate_mipmaps(tex);
```

`rc_gfx_texture_generate_mipmaps` is a convenience; where mip quality matters
(sRGB content especially), generate mips on the CPU in linear space instead.

For image files there is a bridge from the image layer (PNG bytes are
untrusted input, so decoding returns an error result rather than trapping):

```c
rc_image_png_result png = rc_image_load_png(RC_STR("data/albedo.png"),
                                            RC_PIXEL_FORMAT_RGBA8, &arena, scratch);
RC_ASSERT(png.error == RC_IMAGE_PNG_OK);
rc_gfx_texture tex = rc_gfx_texture_from_image(png.image, /*srgb*/ true, /*mipmaps*/ true);
```

Samplers are separate objects everywhere in the API (the GL backend recombines
them internally). Filtering defaults to NEAREST and addressing to
CLAMP_TO_EDGE, so a textured surface usually wants at least:

```c
rc_gfx_sampler sampler = rc_gfx_sampler_make(&(rc_gfx_sampler_desc) {
    .min_filter = RC_GFX_FILTER_LINEAR,
    .mag_filter = RC_GFX_FILTER_LINEAR,
    .mip_filter = RC_GFX_FILTER_LINEAR,
    .address_u = RC_GFX_ADDRESS_REPEAT,
    .address_v = RC_GFX_ADDRESS_REPEAT,
});
```

Setting `compare` to anything but `RC_GFX_COMPARE_ALWAYS` makes a *comparison
sampler* for shadow mapping (bind it as `RC_GFX_BINDING_COMPARISON_SAMPLER`).

### Binding textures to shaders

Layout entries for textures declare what the shader expects
(`texture_dim`, `sample_type`, `multisampled`); sampler entries are just a
type. GLSL 330 has combined samplers (`sampler2D`), so the shader desc carries
a small table pairing each GLSL uniform back onto the separate texture and
sampler bindings - boilerplate that disappears entirely under future backends:

```c
// GLSL: uniform sampler2DArray u_texture;
.texture_samplers = (const rc_gfx_shader_texture_sampler_pair[]) {
    {
        .glsl_name = RC_STR("u_texture"),
        .texture_group = 0, .texture_binding = 1,
        .sampler_group = 0, .sampler_binding = 2,
    },
},
.texture_sampler_count = 1,
```

with the matching layout entries and bind group entries:

```c
// layout entries (in the simple_layout call):
{.binding = 1, .visibility = RC_GFX_STAGE_FRAGMENT,
 .type = RC_GFX_BINDING_TEXTURE, .texture_dim = RC_GFX_TEXTURE_DIM_2D_ARRAY},
{.binding = 2, .visibility = RC_GFX_STAGE_FRAGMENT,
 .type = RC_GFX_BINDING_SAMPLER},

// bind group entries:
{.binding = 1, .texture = tex},
{.binding = 2, .sampler = sampler},
```

## 8. Organising bindings: the four groups

A pipeline layout holds up to `RC_GFX_MAX_BIND_GROUPS` (4) group layouts, and
`rc_gfx_encoder_set_bind_group` binds a group at an index. The convention -
follow it in any renderer you build on gfx - is by update frequency:

| Group | Contents | Rebinds |
|-------|----------|---------|
| 0 | per frame: camera, time, environment | once per frame |
| 1 | per pass: shadow map inputs, pass constants | per pass |
| 2 | per material: textures, material constants | per material change |
| 3 | per draw: object transform via dynamic offset | offsets only |

For multi-group layouts, build each group layout with
`rc_gfx_bind_group_layout_make` and combine them:

```c
rc_gfx_pipeline_layout pl = rc_gfx_pipeline_layout_make(&(rc_gfx_pipeline_layout_desc) {
    .groups = {frame_layout, pass_layout, material_layout},
    .group_count = 3,
});
```

`rc_gfx_simple_layout_make` (used throughout this tutorial) is the one-group
shortcut; its internal group layout is owned by, and destroyed with, the
pipeline layout it returns.

Bind groups are immutable snapshots - a material makes its group once at load.
The only per-frame variation goes through dynamic offsets, which are consumed
in increasing binding order at `set_bind_group`.

## 9. Pipelines in full

The triangle used a fraction of `rc_gfx_pipeline_desc`. The rest:

- **`primitive`**: TRIANGLES (default), TRIANGLE_STRIP, LINES, LINE_STRIP,
  POINTS.
- **`index_format`**: NONE (default) permits only non-indexed draws; U16/U32
  bakes the index type for `draw_indexed`.
- **`cull` / `front_face`**: culling is off by default; `RC_GFX_CULL_BACK`
  with the default CCW winding is the usual 3D setting. `front_face` exists
  for mirrored geometry.
- **`vertex_layout`**: up to 8 buffers and 16 attributes. Two conveniences:
  an attribute `offset` of 0 means "packed after the previous attribute of
  the same buffer", and a buffer `stride` of 0 means "computed from the
  packing" - so tightly packed layouts can omit both. A buffer with
  `.per_instance = true` steps per instance (section 10).
- **`colors` + `color_count`**: one `rc_gfx_color_target_state` per
  attachment - `format` (baked), `blend`, and `write_mask` (bits *disable*
  channels, so `{0}` writes RGBA).
- **`depth_stencil`**: `format` NONE (default) means no depth attachment.
  Otherwise set `depth_write` and `depth_compare`
  (`RC_GFX_COMPARE_GREATER_EQUAL` for reverse-Z), optional stencil state
  (front/back ops, masks), and depth bias.
- **`sample_count`**: must match the render target (section 12).
- **`alpha_to_coverage`**: MSAA alpha-tested foliage and the like.

Blending has three ready-made states:

```c
.colors = {
    {
        .format = rc_gfx_swapchain_format(),
        .blend = rc_gfx_blend_state_make_premultiplied(),
    },
},
```

`rc_gfx_blend_state_make_alpha()` is classic non-premultiplied alpha;
`_make_premultiplied()` is what you should actually use for anything
composited more than once (it is the only form that composes associatively);
`_make_additive()` for glows and particles. All blending is in linear space,
like everything else. Custom factor/op combinations fill `rc_gfx_blend_state`
directly. (GL 3.3 limitation, validated for you: multiple enabled colour
targets must share target 0's blend state.)

A depth-tested opaque 3D pipeline therefore adds, beyond the triangle's desc:

```c
.index_format = RC_GFX_INDEX_FORMAT_U16,
.cull = RC_GFX_CULL_BACK,
.depth_stencil = {
    .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
    .depth_write = true,
    .depth_compare = RC_GFX_COMPARE_GREATER_EQUAL,   /* reverse-Z */
},
```

## 10. Indexed and instanced drawing

Indexed drawing needs an INDEX buffer, an `index_format` baked in the
pipeline, and `set_index_buffer` while recording:

```c
rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
rc_gfx_encoder_set_index_buffer(enc, index_buf, RC_GFX_INDEX_FORMAT_U16, 0);
rc_gfx_encoder_draw_indexed(enc, &(rc_gfx_draw_indexed_desc) {
    .index_count = 36,
});
```

Instancing is expressed entirely in the vertex layout: a second vertex buffer
marked `per_instance`, whose attributes advance once per instance instead of
once per vertex. The cubes example feeds each instance a model matrix as four
`vec4` attributes plus a texture layer, from a STREAM buffer rewritten each
frame:

```c
.buffers = {
    {.stride = sizeof(vertex)},
    {.stride = sizeof(instance), .per_instance = true},
},
.attributes = {
    // per-vertex: locations 0..2 from buffer 0 (pos, normal, uv)
    // per-instance: locations 3..6 = the four columns of the model matrix,
    // location 7 = texture layer, all from buffer 1
    {.location = 3, .buffer_index = 1, .offset = offsetof(instance, model),
     .format = RC_GFX_VERTEX_FORMAT_F32X4},
    // ... locations 4..6 at +16, +32, +48 ...
},
```

and in the shader:

```c
"layout(location = 3) in vec4 a_model0;\n"
// ...
"mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);\n"
```

Then draw the mesh once with an instance count:

```c
rc_gfx_encoder_set_vertex_buffer(enc, 0, vbuf, 0);
rc_gfx_encoder_set_vertex_buffer(enc, 1, instance_buf, 0);
rc_gfx_encoder_set_index_buffer(enc, index_buf, RC_GFX_INDEX_FORMAT_U16, 0);
rc_gfx_encoder_draw_indexed(enc, &(rc_gfx_draw_indexed_desc) {
    .index_count = 36,
    .instance_count = CUBE_COUNT,
});
```

(`first_instance` requires `features.base_instance`, which GL 3.3 lacks -
keep it 0 there; `instance_count` 0 means 1.)

## 11. The swapchain, render targets, and offscreen passes

So far every pass targeted `{0}`, the swapchain - time to define that
precisely.

### What exactly is "the swapchain"?

"Swapchain" is the modern-API name (Vulkan, D3D12, WebGPU's surface) for the
window's chain of presentable images; GL's equivalent is the default
framebuffer. gfx adopts the modern term, and on GL builds a small virtual
swapchain of its own: pass target `{0}` names an internal window-sized colour
texture that tracks the size passed to `rc_gfx_begin_frame`, and
`rc_gfx_end_frame` presents it to the actual window with a fullscreen draw
(which is also where the single y-flip between gfx's conventions and the
window system happens). Your passes never touch the window's real
framebuffer, on any backend.

To you the swapchain behaves like a render target with one colour attachment
and fixed properties, and its restrictions are enforced:

| Property | Rule |
|----------|------|
| colour | one attachment; the format is `rc_gfx_swapchain_format()`, fixed at init by `color_space` |
| depth / stencil | optional: `rc_gfx_desc.swapchain_depth_format` (NONE by default). A pipeline drawing to `{0}` must declare exactly that depth format - NONE included - validated when the pipeline is bound |
| sample count | `rc_gfx_desc.swapchain_sample_count`, baked into swapchain pipelines, resolved internally at end of frame |
| as a texture | not available - `{0}` has no texture handle, so it cannot be sampled or attached to another target |
| load / store | per-pass actions work exactly as on any other target |

A depth-tested scene that goes straight to the screen therefore needs exactly
one extra init field - the cubes example does this:

```c
rc_gfx_init(&(rc_gfx_desc) {
    .arena = &state.arena,
    .swapchain_depth_format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
});
// pipeline: .colors[0].format = rc_gfx_swapchain_format(),
//           .depth_stencil.format = rc_gfx_swapchain_depth_format()
// pass:     target {0}, .depth_stencil.depth_clear_value = 0.0f
```

gfx creates the depth buffer, resizes it with the window, and never exposes
it as a texture (it cannot be sampled - use an explicit render target for
depth you want to read). Prefer `DEPTH32F`: the reverse-Z precision the depth
convention is built on needs a float format, which is also why depth never
comes from the window system's own framebuffer - its format would be whatever
fixed-point buffer the context happened to get.

### Offscreen render targets

Everything that cannot happen on the swapchain - sampling the result
(post-processing, blur chains), shadow maps, picking, MRT, rendering into
texture mips or layers - happens on an explicit render target: create
textures with `RENDER_ATTACHMENT` usage and bake them into one:

```c
rc_gfx_texture color = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
    .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
    .size = size,
    .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
});
rc_gfx_texture depth = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
    .format = RC_GFX_TEXTURE_FORMAT_DEPTH32F,
    .size = size,
    .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT,
});

rc_gfx_render_target rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
    .colors = {{.texture = color}},
    .color_count = 1,
    .depth_stencil = {.texture = depth},
});
```

An attachment can address a specific `mip` and `slice` (array layer or cube
face), so rendering into one face of a cube map or one layer of an array is
just an attachment field. Up to `RC_GFX_MAX_COLOR_ATTACHMENTS` (4) colour
attachments give you MRT; all attachments must agree on size and sample
count.

The render target bakes *which* textures; the per-pass actions say what
happens to their contents. Each colour attachment has a `load_op`
(CLEAR default, LOAD to preserve, DISCARD when you will overwrite everything)
and a `store_op` (STORE default, DISCARD, RESOLVE); depth/stencil likewise:

```c
rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {
    .target = rt,
    .colors = {
        {.clear_value = {0.012f, 0.014f, 0.02f, 1.0f}},   // linear
    },
    .depth_stencil = {
        .depth_clear_value = 0.0f,   // reverse-Z far plane
    },
    .label = RC_STR("scene"),
});
// ... draw the scene ...
rc_gfx_encoder_pass_end(enc);
```

To get the result onto the screen, sample it in a second pass targeting the
swapchain. The idiom is a single oversized triangle (no quad, no index
buffer):

```c
static const rc_vec2f blit_vertices[3] = {
    {-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f},
};

static const char blit_vs_src[] =
    "layout(location = 0) in vec2 a_pos;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_pos * 0.5 + 0.5;\n"
    "    gl_Position = rc_clip(vec4(a_pos, 0.0, 1.0));\n"
    "}\n";

static const char blit_fs_src[] =
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "void main() { o_color = texture(u_tex, v_uv); }\n";
```

```c
// second pass in the same encoder, after the scene pass
rc_gfx_encoder_pass_begin(enc, &(rc_gfx_pass_desc) {.label = RC_STR("blit")});
rc_gfx_encoder_set_pipeline(enc, blit_pip);
rc_gfx_encoder_set_bind_group(enc, 0, blit_group, NULL, 0);
rc_gfx_encoder_set_vertex_buffer(enc, 0, blit_vbuf, 0);
rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {.vertex_count = 3});
rc_gfx_encoder_pass_end(enc);
```

Window-sized offscreen targets must follow resizes yourself (only the
swapchain and its optional depth buffer are automatic). The idiom is
destroy-and-recreate when the size changes; deferred destruction (section 14)
makes this safe even with frames in flight:

```c
static void ensure_target(rc_vec2i size)
{
    if (rc_vec2i_is_equal(size, state.target_size)) {
        return;
    }
    if (!rc_genpool_handle_is_null(state.rt.h)) {   // {0} until first created
        rc_gfx_bind_group_destroy(state.blit_group);
        rc_gfx_render_target_destroy(state.rt);
        rc_gfx_texture_destroy(state.color);
        rc_gfx_texture_destroy(state.depth);
    }
    // ... recreate textures, target, and the bind group that samples them ...
    state.target_size = size;
}
```

`rc_gfx_render_target_size(rt)` returns a target's size when you need it
back.

## 12. MSAA

Multisampling has two independent forms:

**A multisampled swapchain** is one field at init -
`.swapchain_sample_count = 4` in `rc_gfx_desc` - plus the matching
`.sample_count = 4` in every pipeline that draws to the swapchain. The
resolve happens internally at end of frame.

**A multisampled offscreen target** is explicit: multisampled attachments
(2D, one mip, no initial data), a single-sample *resolve* texture of the same
format, and a RESOLVE store op:

```c
rc_gfx_texture msaa_color = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
    .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
    .size = size,
    .sample_count = 4,
    .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT,
});
rc_gfx_texture resolved = rc_gfx_texture_make(&(rc_gfx_texture_desc) {
    .format = RC_GFX_TEXTURE_FORMAT_RGBA8_SRGB,
    .size = size,
    .usage = RC_GFX_TEXTURE_USAGE_RENDER_ATTACHMENT | RC_GFX_TEXTURE_USAGE_SAMPLED,
});

rc_gfx_render_target rt = rc_gfx_render_target_make(&(rc_gfx_render_target_desc) {
    .colors = {{.texture = msaa_color}},
    .color_count = 1,
    .resolves = {{.texture = resolved}},
});

// in the pass: resolve at pass end instead of storing the samples
.colors = {
    {
        .clear_value = {1.0f, 0.0f, 0.0f, 1.0f},
        .store_op = RC_GFX_STORE_OP_RESOLVE,
    },
},
```

Afterwards you sample `resolved`; the multisampled texture itself is never
sampled in this flow. Check `rc_gfx_format_caps_query(fmt)` for
`MSAA`/`RESOLVE` support and `rc_gfx_limits_query().max_msaa_samples` for the
sample count.

## 13. Storage buffers

For bulk read-only data too large or too dynamic for uniform blocks (skinning
matrices, particle data, lookup tables), use `STORAGE` buffers bound as
`RC_GFX_BINDING_STORAGE_BUFFER_READ`. Because GL 3.3 has no real storage
buffers, the backend routes them through texture buffer objects - hidden
behind one portable macro, `RC_STORAGE_LOAD(name, i)`, which the preamble
defines appropriately per backend:

```c
// buffer of floats
rc_gfx_buffer data_buf = rc_gfx_buffer_make(&(rc_gfx_buffer_desc) {
    .size = sizeof(values),
    .usage = RC_GFX_BUFFER_USAGE_STORAGE,
    .data = {.data = (const uint8_t *)values, .num = sizeof(values)},
});

// layout entry: the texel format the shader reads
{.binding = 1, .visibility = RC_GFX_STAGE_FRAGMENT,
 .type = RC_GFX_BINDING_STORAGE_BUFFER_READ,
 .texel_format = RC_GFX_TEXTURE_FORMAT_R32F},

// bind group entry: just the buffer
{.binding = 1, .buffer = data_buf},
```

```c
// GLSL 330: declared as a samplerBuffer, read through the macro
"uniform samplerBuffer u_data;\n"
"...\n"
"float v = RC_STORAGE_LOAD(u_data, index).r;\n"
```

Under GL 3.3 the `samplerBuffer` uniform also needs a texture-sampler pair
entry in the shader desc pointing its *texture* fields at the storage binding
(the sampler fields stay zero). On future backends both the pair entry and
the `samplerBuffer` spelling disappear behind the same macro.

## 14. Destroying things, and the object lifecycle

Every `_make` has a `_destroy`. Handles are generation-checked: after a
destroy, any use of the old handle traps in debug builds (including
double-destroy) rather than aliasing a recycled slot. `{0}` is the "none"
handle - useful as a sentinel, as `ensure_target` showed; test for it with
`rc_genpool_handle_is_null(handle.h)`.

Destruction is *deferred*: the GPU object lives `RC_GFX_FRAMES_IN_FLIGHT` (2)
frames past the destroy call, so destroying a resource the previous frame
still references is safe by construction. The handle itself is invalid
immediately.

Ownership notes worth knowing:

- A pipeline layout from `rc_gfx_simple_layout_make` owns its group0 layout
  and destroys it with itself.
- Bind groups reference their resources but do not own them; destroy order
  between a bind group and its textures does not matter (deferral covers the
  GPU side), but do not *bind* a group whose resources are gone.
- `rc_gfx_shutdown()` releases everything still alive - leak-free teardown
  does not require destroying each object by hand (the examples do not).

## 15. Asking the device what it can do

Backends differ; capabilities are data, not compile-time guesses:

```c
rc_gfx_features f = rc_gfx_features_query();
if (!f.base_instance) { /* first_instance must be 0 */ }

rc_gfx_limits lim = rc_gfx_limits_query();          // texture sizes, MSAA, anisotropy...
uint32_t caps = rc_gfx_format_caps_query(RC_GFX_TEXTURE_FORMAT_RG11B10F);
if (caps & RC_GFX_FORMAT_CAP_RENDER) { /* usable as an attachment */ }

rc_str name = rc_gfx_backend_name();
```

On the GL 3.3 backend, expect `compute`, `indirect_draw`, `base_instance`,
`cube_map_array`, `texture_view`, and `debug_markers` to be false, and
`storage_buffers_via_tbo` and (on desktop drivers) `native_depth_zero_to_one`
to be true.

## 16. Debugging

- **Label everything.** Every descriptor takes an `rc_str label`.
- **Dump command buffers.** `rc_gfx_cmd_buffer_dump(cb, &arena)` decodes a
  finished command buffer into one line per command, with no device needed -
  handles print as `index:generation`, so you can diff frames or log exactly
  what a thread recorded:

```
push_debug_group "frame"
pass_begin target=none clear0=(0.2,0.2,0.2,1) label="main"
set_pipeline id=0:0
set_bind_group group=0 id=1:0 offsets=[256]
set_vertex_buffer slot=0 id=2:0 offset=0
draw vertices=3 instances=1 first_vertex=0
pass_end
pop_debug_group
```

- **Debug groups** (`rc_gfx_encoder_push_debug_group` / `_pop`) structure
  dumps and, on backends with marker support, GPU captures.
- **Leave validation on.** It is forced on in debug builds and validates
  pipeline-vs-target formats, texture upload sizes, binding compatibility,
  and (with member tables) std140 layouts.
- The classic first-run failure modes, in order: triangle upside down (you
  bypassed `rc_clip` or flipped y yourself), everything too dark or washed
  out (sRGB format or linear-clear-value mistake), nothing draws with depth
  on (cleared depth to 1.0 or compared LESS - reverse-Z clears to 0.0 and
  compares GREATER_EQUAL), garbage constants (std140 padding - add the
  member table).

## 17. Multi-threaded recording

Encoders have no device state: each records into its own arena, so threads
can build passes concurrently and the context thread submits the results in
order:

```c
// worker threads, each with its own arena:
rc_gfx_encoder *enc = rc_gfx_encoder_begin(&worker_arena);
// ... record a pass ...
rc_gfx_cmd_buffer cb = rc_gfx_encoder_finish(enc);

// context thread:
rc_gfx_cmd_buffer buffers[] = {shadow_cb, scene_cb, ui_cb};
rc_gfx_submit(buffers, 3);
```

`rc_gfx_encoder_alloc_uniforms` is likewise thread-safe (an atomic bump).
Everything else - resource creation, destruction, submit, frame begin/end -
belongs to the context thread. A command buffer stays valid until its arena
is reset, which is why the examples reset the frame arena at the top of each
frame, after `rc_gfx_begin_frame`.

## 18. Where to go from here

- `src/app/example/hellotriangle/main.c` - sections 2 and 3 as a complete
  program.
- `src/app/example/cubes/main.c` - sections 5, 7, 9, 10, and 11 in one scene:
  instanced textured cubes drawn with reverse-Z depth straight to a swapchain
  carrying a `DEPTH32F` buffer.
- `src/app/example/text/main.c` - 2D instancing over the same machinery: SDF
  text through the font layer, pixel-space rendering via
  `rc_mat44f_make_ortho_2d`, alpha blending, and a derivative-based
  antialiasing shader.
- `src/app/example/cubes_overlay/main.c` - section 11 end to end: the cubes
  scene through an offscreen colour + depth render target, a fixed-size
  regional blur chain for the frosted rect (tent downsample to quarter rect
  resolution, then a separable gaussian), and a composite on the swapchain
  with SDF text on top, including the destroy-and-recreate resize idiom for
  the window-sized scene target.
- `docs/app.md`, "richc/gfx - GPU abstraction" - the exhaustive reference for
  every descriptor field, enum, and backend note that this tutorial
  paraphrased.
- `src/app/test/test_gfx_gl.c` - focused, assertion-checked samples of nearly
  every feature (MSAA resolve, storage buffers, scissoring, sRGB readback),
  useful as known-good snippets.
