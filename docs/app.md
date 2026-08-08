# richc app reference

Per-header reference for the **app** layer (the `richc_app` library, built on
core): windowing, input, CPU-side image loading, packing, and atlasing, and
the gfx GPU abstraction. See [overview.md](overview.md) for the shared
philosophy and conventions, and [core.md](core.md) for the core layer.

## richc/app/app.h - window and event loop

The headers below belong to the **app layer** - link the `richc_app` target
(which pulls in core, GLFW, and glad). `rc_app` is a single-window application
with an OpenGL context, driven by a global event loop, so its functions take no
handle. GLFW and glad are private to the backend and never appear in the API.

- `rc_app_desc` configures the window: `title` (`rc_str`), `size` (`rc_vec2i`),
  `resizable`, `hidden` (create the window invisible - offscreen use, tests),
  and a `callbacks` block. There are no graphics hints: colour space, depth,
  and MSAA are configured in gfx descriptors (gfx renders to its own FBOs),
  and an sRGB-capable default framebuffer is requested unconditionally.
- `rc_app_callbacks` holds optional function pointers (leave any NULL to ignore
  that event) plus a `ctx` forwarded as the first argument of every callback:
  keyboard (`on_key_down` / `on_key_up` with an `rc_scancode` and `rc_mod`,
  `on_key_char` for Unicode text codepoints), mouse (`on_mouse_down` / `_up` /
  `_enter` / `_leave` / `_move` / `_wheel`), window state (`on_resize`,
  `on_focus_gained` / `_lost`, `on_minimize` / `_maximize`), and the frame
  callbacks `on_update(ctx, dt)` and `on_render(ctx, size)` (`size` is the
  framebuffer size in physical pixels; only invoked while it is non-zero,
  never while minimised).
- Lifecycle: `rc_app_init(const rc_app_desc *)`, `rc_app_destroy()`,
  `rc_app_poll()` (pump OS events), `rc_app_is_running()` -> `bool`,
  `rc_app_size()` -> `rc_vec2i` (framebuffer pixels), `rc_app_time()` -> `double`
  (seconds since init, for animation timers).
- Frames: drive the loop with `rc_app_request_update()` (invokes `on_update` with
  the elapsed `dt`) and `rc_app_request_render()` (invokes `on_render` with the
  framebuffer size, then swaps buffers; a no-op while the framebuffer is
  zero-sized) rather than calling the callbacks directly - the backend also
  fires `on_render` from the OS window-refresh, so rendering stays live during a
  modal resize. Viewport state belongs to the renderer (gfx sets it per pass);
  the app layer does not touch it. Use `rc_app_swap_buffers()` to swap directly
  when driving rendering elsewhere (e.g. a render thread).

---

## richc/app/keys.h - scancodes, mouse buttons, modifiers

- `rc_mod` - modifier bit flags, OR-combined: `RC_MOD_SHIFT`, `RC_MOD_CTRL`,
  `RC_MOD_ALT`, `RC_MOD_SUPER`, `RC_MOD_CAPS`, `RC_MOD_NUMLOCK`.
- `rc_mouse_button` - `RC_MOUSE_BUTTON_LEFT` (0), `_RIGHT` (1), `_MIDDLE` (2).
- `rc_scancode` - physical key positions (layout-independent), including the
  printable keys (`RC_SCANCODE_A` .. `RC_SCANCODE_Z`, the digits,
  `RC_SCANCODE_SPACE`, punctuation), the navigation and function keys
  (`RC_SCANCODE_ESCAPE`, `_ENTER`, arrows, `_F1` .. `_F12`), the keypad
  (`RC_SCANCODE_KP_0` ..), the modifier keys (`RC_SCANCODE_LEFT_SHIFT` ..), and
  `RC_SCANCODE_UNKNOWN` (-1). Values match GLFW key constants. Use the scancode
  for physical controls; use the `on_key_char` codepoint for text entry.

---

## richc/image/image.h - CPU image

`rc_image { rc_span_bytes data; rc_vec2i size; uint32_t stride; rc_pixel_format format; }`
- a non-owning window over arena-backed pixel bytes. The origin is the top-left
  corner; pixels run left-to-right within a row and rows run top-to-bottom,
  `stride` bytes apart (`stride >= size.x * bytes_per_pixel`). The arena owns the
  bytes; the image just describes their layout.

- `rc_pixel_format` - `RC_PIXEL_FORMAT_NONE` (0, unset), `_R8` (1), `_RGB8` (3),
  `_RGBA8` (4); the enum value is the bytes-per-pixel count.
  `rc_pixel_format_bytes_per_pixel(fmt)` returns it.
- Packed colour: pixels are read and written as a `uint32_t` with R in bits 0-7,
  G in 8-15, B in 16-23, A in 24-31. Reading widens narrower formats (R8 ->
  opaque grayscale, RGB8 -> alpha 255); writing keeps only the channels the
  format stores.
- Construction: `rc_image_make(size, format, arena)` (cleared to zero),
  `rc_image_make_filled(size, format, fill, arena)` (every pixel set to the packed
  colour `fill`), `rc_image_make_subimage(img, region)` (a borrowed view of a
  `rc_box2i` region clamped to bounds, sharing the parent's stride; an empty
  region yields a zero-size image whose data pointer still points into the parent).
- Operations: `rc_image_blit(dst, dst_pos, src)` -> `bool` copies `src` into `dst`
  at `dst_pos`, clipping to `dst`'s bounds and widening to `dst`'s format; returns
  false if `src`'s format is wider than `dst`'s (no narrowing), and a fully
  clipped blit is a no-op that returns true.
- Per pixel: `rc_image_get_pixel(img, x, y)` -> packed `uint32_t` and
  `rc_image_set_pixel(img, x, y, color)` dispatch on the image's format; the
  `_r8` / `_rgb8` / `_rgba8` variants assume that format (asserted) and skip the
  dispatch for hot loops.

---

## richc/image/image_png.h - PNG decoder

`rc_image_from_png(rc_view_bytes png_data, rc_pixel_format pixel_format_hint, rc_arena *arena, rc_arena scratch)`
decodes an in-memory PNG into an `rc_image`, decompressing the zlib-wrapped IDAT
stream with core's `rc_zip_inflate_zlib`. The pixels are allocated from `arena`;
`scratch` (passed by value, and necessarily a *different* arena from `arena`)
holds the transient buffers and is released on return.

- Returns `rc_image_png_result { rc_image image; rc_image_png_error error; }`; on
  error the image is the invalid (all-zero) state.
- `rc_image_load_png(filename, hint, arena, scratch)` is a convenience that reads
  the file into `scratch` and decodes it, so only the image survives in `arena`;
  it returns `RC_IMAGE_PNG_ERROR_IO` if the file cannot be read.
- `rc_image_png_error`: `RC_IMAGE_PNG_OK`, `RC_IMAGE_PNG_ERROR_NOT_PNG`,
  `_ERROR_TRUNCATED`, `_ERROR_BAD_HEADER`, `_ERROR_UNSUPPORTED`, `_ERROR_BAD_DATA`,
  `_ERROR_IO`.
- `pixel_format_hint` widens but never narrows: the result format is the wider
  (by bytes per pixel) of the PNG's natural richc format and the hint. Pass
  `RC_PIXEL_FORMAT_NONE` to keep the natural format (R8 for grayscale, RGB8 for
  truecolour, RGBA8 for the alpha variants, and RGB8 / RGBA8 for palette without
  / with a tRNS chunk).
- Supported: colour types grayscale, truecolour, palette, grayscale+alpha, and
  truecolour+alpha at 8 bits per channel, plus 1/2/4-bit grayscale and palette
  indices; all five scanline filters; multiple IDAT chunks; palette transparency.
- Not supported: 16-bit channels and Adam7 interlace (both report
  `_ERROR_UNSUPPORTED`). Chunk CRC-32s are not validated - the IDAT Adler-32 that
  inflate checks already covers the pixel data.

---

## richc/image/image_pack.h - image atlas packer

`rc_image_pack(images, size, spacing, arena, scratch)` packs a set of CPU images
into a single atlas image (for upload as one GPU texture), via core's rectangle
packer ([rect_pack.h](core.md#richcrect_packh---rectangle-packing)). The atlas
takes the widest pixel format among the inputs and each source is widened into it
by `rc_image_blit`; the atlas is cleared, so the gaps between images read as zero.

- Returns `rc_image_pack_result { rc_image image; rc_span_vec2i positions; }`; on
  failure the result is the all-zero state (invalid image, empty positions).
  `positions[i]` is the top-left of `images[i]` in the atlas, indexed by input -
  the caller derives a rectangle when it needs one from `position + images[i].size`.
- `size` is the atlas dimensions in pixels, or `{0, 0}` to size the atlas
  automatically: it starts from a power-of-two square (or 2:1 rectangle) covering
  the total image area and grows, alternately doubling width then height, until
  everything fits. The chosen size is reported as `result.image.size`.
- Packing fails (yielding the all-zero result) if `images` is empty, a fixed
  `size` cannot hold every image, or an auto-sized atlas would exceed the
  addressable byte size.
- `scratch` (passed by value, a different arena from `arena`) holds the transient
  packing state; the atlas pixels and the positions are allocated from `arena`.

---

## richc/image/image_atlas.h - incremental image atlas

`rc_image_atlas { rc_image image; rc_rect_pack packer; }` is the incremental
counterpart to [image_pack.h](#richcimageimage_packh---image-atlas-packer): where
`image_pack` packs a known set all at once, `rc_image_atlas` adds images one at a
time over a session - e.g. font glyphs the first time each is encountered - with
earlier placements staying put.

- `rc_image_atlas_make(size, format, spacing, arena)` creates a cleared atlas of
  the given size and pixel format, maintaining a `spacing`-pixel gap between added
  images.
- `rc_image_atlas_add(atlas, src, arena)` places `src` and blits its pixels in,
  returning `rc_rect_pack_result { rc_vec2i pos; bool placed; }` (`placed` is false
  when `src` no longer fits). `src` must be valid and no wider than the atlas
  format (it is widened in, never narrowed).
- One arena, no scratch: the atlas pixels are allocated once and never grow, so
  only the packer's free list grows on `add`. Pass the same arena to `make` and
  every `add`; the atlas captures no arena. Read `atlas.image` to upload or sample
  the atlas.

---

## richc/gfx - GPU abstraction

The gfx module is a backend-agnostic GPU layer whose object model follows
WebGPU: immutable descriptor structs where zero means default, typed handles,
bind groups against explicit layouts, pipelines that bake shader + vertex
layout + render state, render passes with per-attachment load/store actions,
and arena-backed command encoders. The phase-1 backend is OpenGL 3.3 core,
selected at compile time (CMake option `RICHC_GFX_BACKEND`, default `gl33`;
the backend define is `RC_GFX_BACKEND_GL33`). There is no umbrella header;
include the category headers you use.

A GL context must be current before `rc_gfx_init` (`rc_app_init` establishes
one); gfx never creates a context. Resource creation and `rc_gfx_submit` must
happen on the context thread; command encoding may happen on any thread.

### Coordinate conventions

One convention, identical on every backend; user math never changes when the
backend does.

| Aspect | Rule |
|--------|------|
| NDC | x right, y **up**, both in [-1, 1]; z in **[0, 1]** |
| Depth | **Reverse-Z always**: near -> 1, far -> 0; compare GREATER_EQUAL, clear to 0.0 |
| Viewport / scissor | Origin top-left, +y down, pixel units |
| Texture coordinates | Origin top-left, v down; texel (0,0) first in memory |
| World / view space | Right-handed, +X right, +Y up, -Z forward |
| Winding | CCW front-facing by default (`front_face` settable per pipeline) |
| Matrices | Column-major, column vectors, `v' = M * v` (`rc_mat44f` as-is) |
| Colour | Linear throughout the shader; encoding is a property of texture format |

User passes never target the default framebuffer: a pass target of `{0}` is an
internal window-sized swapchain texture, and `rc_gfx_end_frame` presents it
with a fullscreen-triangle draw that flips v (a draw, not a blit - blit sRGB
semantics historically diverge across drivers; texture decode and
fragment-output encode do not).

### Colour space and sRGB

Everything a shader sees is linear. Encoding is applied by fixed-function
hardware on read (sRGB texture formats decode before filtering) and on write
(sRGB render target formats encode after blending, which therefore blends in
linear space). There is no shader-side gamma math. Consequences worth
remembering:

- Clear values and the blend constant are **linear**, even for sRGB
  attachments (an artist's "mid grey" is 0.216, not 0.5).
- Alpha is never encoded; sRGB applies to RGB only.
- Colour constants authored as hex from design tools are sRGB-encoded:
  convert with `richc/gfx/color.h` before they reach a shader.
- Albedo / UI art / emissive textures want `_SRGB` formats; normal maps,
  masks, font atlases, and data textures want `_UNORM` / float formats.
- The swapchain colour space is set at init: `RC_GFX_COLOR_SPACE_SRGB`
  (default; 8-bit sRGB-encoded output) or `_LINEAR` (no encode; for bit-exact
  output such as an emulator's framebuffer, or a pipeline applying its own
  display transform).

### richc/gfx/gfx.h - device, handles, shared vocabulary

- Handles: `rc_gfx_buffer`, `rc_gfx_texture`, `rc_gfx_sampler`,
  `rc_gfx_shader`, `rc_gfx_bind_group_layout`, `rc_gfx_bind_group`,
  `rc_gfx_pipeline_layout`, `rc_gfx_pipeline`, `rc_gfx_render_target` - each a
  distinct one-member struct wrapping core's 8-byte `rc_genpool_handle` (member
  `h`: slot index + generation), so handles of different resource types cannot
  be mixed. `{0}` is the invalid "none" handle. Generations bump on destroy, so
  stale handles trap rather than aliasing. Inspect a handle through the
  `rc_genpool_handle_*` functions (`_is_null`, `_index`, `_gen`, `_equal`,
  `_make`) on the `h` member.
- Limits: `RC_GFX_MAX_BIND_GROUPS` (4), `RC_GFX_MAX_BINDINGS_PER_GROUP` (16),
  `RC_GFX_MAX_VERTEX_BUFFERS` (8), `RC_GFX_MAX_VERTEX_ATTRIBUTES` (16),
  `RC_GFX_MAX_COLOR_ATTACHMENTS` (4), `RC_GFX_MAX_MIP_LEVELS` (16),
  `RC_GFX_FRAMES_IN_FLIGHT` (2), `RC_GFX_UNIFORM_ALIGN` (256).
- `rc_gfx_texture_format` - R8/RG8/RGBA8/BGRA8 (+_SRGB variants), 8/16/32-bit
  UINT, 16F/32F float, RGB10A2_UNORM, RG11B10F, the depth/stencil formats
  (DEPTH16_UNORM, DEPTH24_PLUS, DEPTH32F, DEPTH24_PLUS_STENCIL8,
  DEPTH32F_STENCIL8), and the BC compressed families (BC1/3/4/5/6H/7).
  Helpers: `rc_gfx_texture_format_is_srgb/_is_depth/_is_stencil/
  _is_compressed`, `_block_size` (bytes per block), `_block_dim` (1x1 or 4x4),
  `_to_linear` / `_to_srgb` (sRGB pairing, identity when none).
- Shared enums: `rc_gfx_compare` (ALWAYS default), `rc_gfx_index_format`
  (NONE/U16/U32), `rc_gfx_stage` bit flags (VERTEX, FRAGMENT),
  `rc_gfx_color_space` (SRGB default, LINEAR).
- `rc_gfx_desc` - `arena` (required; gfx's persistent allocations),
  `color_space`, `swapchain_depth_format` (NONE = colour-only swapchain; a
  depth format such as DEPTH32F gives the swapchain target a depth/stencil
  buffer that follows the window size), `swapchain_sample_count` (0/1 = no
  MSAA), `uniform_ring_size` (bytes per in-flight frame, 0 => 1 MB),
  `validation` (extra load-time checks; forced on in debug builds).
- Lifecycle: `rc_gfx_init(desc)` / `rc_gfx_shutdown()` (destroys anything
  still alive); per frame `rc_gfx_begin_frame(size)` (current framebuffer
  size; the swapchain target recreates on change) and `rc_gfx_end_frame()`
  (present pass + retires deferred destructions - the swap itself stays with
  the app layer). Destroys are deferred `RC_GFX_FRAMES_IN_FLIGHT` frames.
- Queries: `rc_gfx_swapchain_size()` / `rc_gfx_swapchain_format()` /
  `rc_gfx_swapchain_depth_format()` (NONE when colour-only),
  `rc_gfx_features_query()` (`rc_gfx_features`: compute, storage_buffers,
  storage_buffers_via_tbo, base_instance, indirect_draw,
  native_depth_zero_to_one, cube_map_array, texture_view,
  anisotropic_filtering, srgb_default_framebuffer, debug_markers,
  timer_queries), `rc_gfx_limits_query()` (`rc_gfx_limits`: texture sizes,
  array layers, colour attachments, vertex attributes, UBO range/alignment,
  MSAA samples, anisotropy), `rc_gfx_format_caps_query(fmt)` ->
  `rc_gfx_format_caps` bit flags (SAMPLE, FILTER, RENDER, BLEND, MSAA,
  RESOLVE), `rc_gfx_backend_name()`.

### richc/gfx/color.h - sRGB conversion helpers

The exact piecewise sRGB curve (not a 2.2 power approximation - the linear
segment near black matters for dark UI colours). Alpha is never converted.

- `rc_gfx_srgb_to_linear(s)` / `rc_gfx_linear_to_srgb(l)` - one channel.
- `rc_gfx_color_from_srgb_u32(rgba)` - packed `0xAABBGGRR` (R low byte) ->
  linear `rc_vec4f`.
- `rc_gfx_color_to_srgb_u32(linear)` - the inverse; clamps to [0, 1].

### richc/gfx/buffer.h - GPU buffers

- `rc_gfx_buffer_usage` bit flags: VERTEX, INDEX, UNIFORM, STORAGE, COPY_SRC,
  COPY_DST.
- `rc_gfx_buffer_update_mode` - IMMUTABLE (default; contents at creation,
  never changed), DYNAMIC (occasional updates), STREAM (rewritten per frame).
- `rc_gfx_buffer_desc` - `size` (required), `usage` (required), `update`,
  `data` (initial contents; required if IMMUTABLE), `label`.
- `rc_gfx_buffer_make(desc)` / `rc_gfx_buffer_destroy(buf)`.
- `rc_gfx_buffer_update(buf, offset, data)` - whole-or-partial update of a
  DYNAMIC or STREAM buffer; takes effect at the point of the call in
  submission order; not between pass begin/end.

### richc/gfx/texture.h - textures and samplers

- Enums: `rc_gfx_texture_dim` (2D default, 2D_ARRAY, 3D, CUBE),
  `rc_gfx_texture_sample_type` (FLOAT default, UNFILTERABLE_FLOAT, DEPTH,
  SINT, UINT), `rc_gfx_texture_usage` bit flags (SAMPLED,
  RENDER_ATTACHMENT, COPY_SRC, COPY_DST), `rc_gfx_filter` (NEAREST default,
  LINEAR), `rc_gfx_address` (CLAMP_TO_EDGE default, REPEAT, MIRROR_REPEAT,
  CLAMP_TO_BORDER).
- `rc_gfx_texture_desc` - `dim`, `format` (required), `size` (required),
  `depth` (3D depth or array layers; default 1; CUBE is 6), `mip_count`
  (0 => 1), `sample_count` (0/1 => no MSAA; MSAA is 2D, one mip, no data),
  `usage` (default SAMPLED), `data` (optional initial contents:
  `rc_gfx_texture_data { subresources; count }` indexed
  `[slice * mip_count + mip]`, per-mip whole volumes for 3D), `label`. Rows
  are supplied top-first, tightly packed to the format's block size.
- `rc_gfx_texture_make(desc)` / `rc_gfx_texture_destroy(tex)`.
- `rc_gfx_texture_update(tex, mip, slice, region, data)` - region update,
  uncompressed formats only, tightly packed rows covering the region.
- `rc_gfx_texture_generate_mipmaps(tex)` - GPU mip generation; convenience
  only (driver sRGB downsampling has historically diverged - generate mips on
  the CPU in linear space where quality matters).
- `rc_gfx_mip_count(size)` - mips in a full chain.
- `rc_gfx_texture_from_image(img, srgb, mipmaps)` - bridge from `rc_image`:
  R8 -> R8_UNORM (never sRGB), RGB8 widens to RGBA on upload, RGBA8 ->
  RGBA8_SRGB or _UNORM per `srgb`.
- `rc_gfx_sampler_desc` - min/mag/mip filters, address_u/v/w, lod_min /
  lod_max (0 => 1000), max_anisotropy (0/1 = off), `compare` (any value other
  than ALWAYS makes a comparison sampler), `border_color` (linear), `label`.
- `rc_gfx_sampler_make(desc)` / `rc_gfx_sampler_destroy(smp)`. Samplers are
  separate objects on every backend; the GL backend re-combines them with
  textures internally.

### richc/gfx/shader.h - shaders

Phase 1 takes GLSL 330 source directly (no shading-language abstraction). The
backend prepends a preamble, so sources must not contain a `#version` line.
The vertex shader must write `gl_Position` through `rc_clip(...)`, the
injected hook that absorbs every clip-space convention difference between
backends. The preamble also defines `RC_GFX_BACKEND_GL33` and
`RC_STORAGE_LOAD(name, i)` (the storage-buffer access macro - `texelFetch` on
a `samplerBuffer` under GL 3.3, a buffer index elsewhere). Vertex attributes
are matched by `layout(location = N)` only. All shader constants live in
std140 uniform blocks declared without instance names; there are no loose
uniforms, and no gamma math ever.

- `rc_gfx_uniform_member` - `name` (GLSL member), `offset` / `size` (from the
  C struct); optional per-block table for debug std140 validation, which
  turns silent padding corruption (vec3, mat3, arrays) into a load-time trap.
- `rc_gfx_shader_uniform_block` - `glsl_name`, `group`, `binding`, `size`
  (sizeof the C struct), optional `members` / `member_count`.
- `rc_gfx_shader_texture_sampler_pair` - maps a GLSL combined sampler uniform
  (`glsl_name`) back onto separate texture and sampler bindings; for a
  `samplerBuffer` backing a STORAGE_BUFFER_READ binding, point the texture
  fields at it and leave the sampler fields unused. Dropped entirely under
  SPIR-V / WGSL backends.
- `rc_gfx_shader_desc` - `vs_source` / `fs_source` (GLSL 330 bodies), the
  uniform block and texture-sampler tables, `label`.
- `rc_gfx_shader_make(desc)` / `rc_gfx_shader_destroy(shd)`.

### richc/gfx/bindings.h - bind groups and layouts

Group indices are by update frequency - follow this convention in every
renderer built on the layer: group 0 per frame (camera, time, environment),
group 1 per pass, group 2 per material, group 3 per draw (dynamic offsets).

- `rc_gfx_binding_type` - UNIFORM_BUFFER, TEXTURE, SAMPLER,
  COMPARISON_SAMPLER, STORAGE_BUFFER_READ (a TBO on GL 3.3, a real read-only
  storage buffer on later backends, hidden behind `RC_STORAGE_LOAD`).
- `rc_gfx_bind_group_layout_entry` - `binding`, `visibility` (stage flags),
  `type`, plus per-type fields: uniform buffers take `has_dynamic_offset` and
  `min_binding_size` (0 = unvalidated; set it, it catches std140 bugs),
  textures take `texture_dim` / `sample_type` / `multisampled`, storage
  buffers take `texel_format`.
- `rc_gfx_bind_group_layout_desc` (entries + count + label) ->
  `rc_gfx_bind_group_layout_make` / `_destroy`.
- `rc_gfx_pipeline_layout_desc` (up to 4 group layouts, in group order; 0
  groups is a valid empty layout) -> `rc_gfx_pipeline_layout_make` /
  `_destroy`.
- `rc_gfx_simple_layout_make(entries, entry_count, label)` - the common
  one-group case in one call; returns `rc_gfx_simple_layout { layout;
  group0; }`. The internal group layout is owned by (and destroyed with) the
  pipeline layout.
- `rc_gfx_bind_group_entry` - `binding` plus exactly one of: `buffer` (+
  `buffer_offset`, 256-aligned, and `buffer_size`, 0 => rest of buffer),
  `texture`, `sampler`.
- `rc_gfx_bind_group_desc` (layout + entries, one per layout entry) ->
  `rc_gfx_bind_group_make` / `_destroy`. Bind groups are immutable; a
  material creates one once at load time. Dynamic offsets are consumed in
  increasing binding order at bind time.

### richc/gfx/pipeline.h - pipelines

- `rc_gfx_vertex_format` - U8/I8/U16/I16 x2/x4 (plain and _NORM), F16X2/X4,
  F32/U32/I32 x1..x4, RGB10A2_NORM.
- `rc_gfx_primitive` - TRIANGLES (default), TRIANGLE_STRIP, LINES,
  LINE_STRIP, POINTS.
- `rc_gfx_stencil_op`, `rc_gfx_blend_factor`, `rc_gfx_blend_op`,
  `rc_gfx_cull` (NONE default), `rc_gfx_front_face` (CCW default).
- `rc_gfx_color_mask` - write-mask bits DISABLE a channel, so 0 means "write
  RGBA" and `{0}` stays a valid default.
- `rc_gfx_vertex_layout` - up to 8 `rc_gfx_vertex_buffer_layout` (stride 0 =>
  computed from the attributes, `per_instance`, `step_rate` 0 => 1) and 16
  `rc_gfx_vertex_attribute` (`location`, `buffer_index`, `offset` 0 => packed
  after the previous attribute of the same buffer, `format`).
- `rc_gfx_depth_stencil_state` - `format` (NONE => no depth attachment),
  `depth_write`, `depth_compare` (GREATER_EQUAL for reverse-Z),
  `stencil_enabled` + front/back `rc_gfx_stencil_face` + read/write masks
  (0 => 0xFF), depth bias / slope scale / clamp.
- `rc_gfx_blend_state` (enabled + colour and alpha factor/op pairs) and
  `rc_gfx_color_target_state` (`format` required, `blend`, `write_mask`).
- `rc_gfx_pipeline_desc` - `shader` + `layout` (required), `vertex_layout`,
  `primitive`, `index_format` (NONE => non-indexed only), `cull`,
  `front_face`, `colors` + `color_count`, `depth_stencil`, `sample_count`,
  `alpha_to_coverage`, `label`. Colour formats, depth format and sample count
  are baked in (the D3D12/Vulkan PSO contract) and validated against the
  render target when the pipeline is bound.
- `rc_gfx_pipeline_make(desc)` / `rc_gfx_pipeline_destroy(pip)`.
- Blend helpers (all linear, like every blend): `rc_gfx_blend_state_make_alpha`
  (non-premultiplied), `_make_premultiplied` (prefer it for anything
  composited more than once - it is the only form that composes
  associatively), `_make_additive`.
- GL 3.3 note: blending has per-target enables but one blend function, so
  every enabled target must share target 0's blend state (validated).

### richc/gfx/pass.h - render targets and passes

- `rc_gfx_load_op` - CLEAR (default), LOAD, DISCARD; `rc_gfx_store_op` -
  STORE (default), DISCARD, RESOLVE.
- `rc_gfx_attachment` - `texture`, `mip`, `slice` (array layer or cube face).
- `rc_gfx_render_target_desc` - colour attachments + count, `depth_stencil`,
  `resolves` (MSAA resolve destinations: single-sample, same format), `label`.
  A render target bakes the attachment set; load/store actions are per-pass.
- `rc_gfx_render_target_make(desc)` / `_destroy(rt)` /
  `rc_gfx_render_target_size(rt)`.
- `rc_gfx_color_attachment_action` - `load_op`, `store_op`, `clear_value`
  (**linear**, even for sRGB attachments). `rc_gfx_depth_stencil_action` -
  depth and stencil load/store + clear values (reverse-Z: clear depth to
  0.0). `rc_gfx_pass_desc` - `target` (`{0}` => the swapchain target),
  per-attachment actions, `label`. The swapchain target is the internal
  window-sized colour image `rc_gfx_end_frame` presents, plus the optional
  depth/stencil buffer requested via `rc_gfx_desc.swapchain_depth_format`. A
  pipeline drawing to it must declare exactly that depth format - NONE when
  no depth was requested (validated at bind; depth/stencil actions are
  ignored on a colour-only swapchain). It has no texture handle, so it can
  never be sampled - anything that must be read back (post-processing
  inputs) renders to an explicit render target instead.
- MSAA resolve is expressed as `RC_GFX_STORE_OP_RESOLVE` on a colour
  attachment whose render target has a resolve destination; the resolve
  happens at pass end. A multisampled swapchain
  (`rc_gfx_desc.swapchain_sample_count > 1`) resolves internally at end of
  frame.

### richc/gfx/encoder.h - command recording and submission

Commands are recorded into an arena-backed encoder as a packed opcode stream
and played back at submit. Recording touches no device state, so each thread
may record with its own arena and encoder; `rc_gfx_submit` plays buffers back
in order on the context thread. A command buffer stays valid until its arena
is reset (typical use: a per-frame arena reset each `rc_gfx_begin_frame`).

- `rc_gfx_encoder_begin(arena)` -> `rc_gfx_encoder *`;
  `rc_gfx_encoder_finish(enc)` -> `rc_gfx_cmd_buffer { data; size; }`;
  `rc_gfx_submit(buffers, count)`.
- Passes: `rc_gfx_encoder_pass_begin(enc, desc)` / `rc_gfx_encoder_pass_end`.
  Viewport and scissor default to the full render target at pass begin;
  pipeline and bindings state starts clean each pass.
- State: `rc_gfx_encoder_set_pipeline`, `_set_bind_group(enc, group_index,
  group, dynamic_offsets, count)` (offsets add to the entries'
  `buffer_offset`), `_set_vertex_buffer(enc, slot, buf, offset)`,
  `_set_index_buffer(enc, buf, fmt, offset)`, `_set_viewport(enc, rect,
  min_depth, max_depth)`, `_set_scissor(enc, rect)` (rects in pixels, origin
  top-left), `_set_blend_constant(enc, color)` (linear),
  `_set_stencil_reference(enc, ref)`.
- Draws: `rc_gfx_encoder_draw(enc, &(rc_gfx_draw_desc) {vertex_count,
  instance_count (0 => 1), first_vertex, first_instance})` and
  `_draw_indexed(enc, &(rc_gfx_draw_indexed_desc) {index_count,
  instance_count, first_index, base_vertex, first_instance})`.
  `first_instance` requires `features.base_instance` (absent on GL 3.3 -
  must be 0 there).
- Per-draw uniforms: `rc_gfx_uniform_buffer()` returns the uniform ring's
  constant buffer handle (reference it from bind groups, created once);
  `rc_gfx_encoder_alloc_uniforms(enc, size)` returns
  `rc_gfx_uniform_alloc { ptr; buffer; offset; }` - write the std140 struct
  through `ptr` (valid until end of frame) and pass `offset` as the dynamic
  offset. Allocations are 256-aligned from a per-frame region of a single
  ring buffer with `RC_GFX_FRAMES_IN_FLIGHT` rotating regions.
- Debug: `rc_gfx_encoder_push_debug_group(enc, label)` / `_pop_debug_group`
  (no-ops without GL_KHR_debug), and `rc_gfx_cmd_buffer_dump(cb, arena)` ->
  `rc_mstr`, a human-readable one-command-per-line decode needing no device.

### OpenGL 3.3 backend notes

- Object mapping: buffers and textures are single GL objects; a pipeline is a
  CPU-side state record (no GL object); a bind group is a resolved unit
  table; a render target is an FBO (plus a resolve FBO); vertex layout lives
  in an internal VAO cache keyed on (pipeline, vertex buffers, offsets),
  populated lazily at draw and evicted on buffer/pipeline destruction.
- Bindings flatten to sequential GL units per class (UBO binding points;
  texture units) by walking groups in order at pipeline layout creation.
  GLSL 330 has no `layout(binding=)`, so block and sampler uniforms are
  assigned at pipeline creation; a shader may be shared across pipelines only
  where the flat mapping is identical (asserted).
- `rc_clip` under GL 3.3 negates y; front-face winding is inverted globally
  to compensate. Depth: with ARB_clip_control (in the vendored loader, and
  exposed on essentially every desktop driver) the backend calls
  `glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)` at init and `rc_clip`
  passes z through, so GL consumes canonical [0,1] reverse-Z natively with
  its precision intact, and `features.native_depth_zero_to_one` reports
  true. Without the extension `rc_clip` rewrites z to [-1,1] as `2z - w`,
  which is correct but loses reverse-Z precision at distance (z-fighting far
  away is the only consequence). The clip origin stays lower-left in both
  cases so the y path never varies.
- Not available on this backend (reported through `rc_gfx_features`):
  compute, read-write storage buffers, `first_instance != 0`, indirect draw,
  cube map arrays, texture views, debug markers. STORAGE_BUFFER_READ is
  supported via texture buffer objects. Store/load DISCARD is a no-op
  (`glInvalidateFramebuffer` is GL 4.3).
