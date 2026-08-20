# richc app reference

Reference for the **app** layer - the `richc_app` library, built on core:
windowing and input, CPU-side images, font loading and SDF glyph atlasing, and
the gfx GPU abstraction. See the [README](../README.md) for the shared
philosophy and conventions, and [core.md](core.md) for the core layer. Link the
`richc_app` target (it pulls in core, GLFW, and glad; GLFW and glad are private
implementation details that never appear in the public API). The documentation
is grouped by category; every public type, function, and macro is covered.

## Contents

- [Application](#application) - `richc/app/`
  - [app.h - window and event loop](#richcappapph---window-and-event-loop)
  - [keys.h - scancodes, mouse buttons, modifiers](#richcappkeysh---scancodes-mouse-buttons-modifiers)
- [Image](#image) - `richc/image/`
  - [image.h - CPU image](#richcimageimageh---cpu-image)
  - [image_png.h - PNG decoder](#richcimageimage_pngh---png-decoder)
  - [image_pack.h - image atlas packer](#richcimageimage_packh---image-atlas-packer)
  - [image_atlas.h - incremental image atlas](#richcimageimage_atlash---incremental-image-atlas)
- [Font](#font) - `richc/font/`
  - [font.h - TrueType loading and SDF glyphs](#richcfontfonth---truetype-loading-and-sdf-glyphs)
  - [font_atlas.h - glyph table and atlas builder](#richcfontfont_atlash---glyph-table-and-atlas-builder)
- [Gfx](#gfx) - `richc/gfx/`
  - [Coordinate conventions](#coordinate-conventions)
  - [Colour and sRGB](#colour-and-srgb)
  - [gfx.h - device, handles, shared vocabulary](#richcgfxgfxh---device-handles-shared-vocabulary)
  - [color.h - sRGB conversion helpers](#richcgfxcolorh---srgb-conversion-helpers)
  - [buffer.h - GPU buffers](#richcgfxbufferh---gpu-buffers)
  - [texture.h - textures and samplers](#richcgfxtextureh---textures-and-samplers)
  - [shader.h - shaders](#richcgfxshaderh---shaders)
  - [bindings.h - bind groups and layouts](#richcgfxbindingsh---bind-groups-and-layouts)
  - [pipeline.h - pipelines](#richcgfxpipelineh---pipelines)
  - [pass.h - render targets and passes](#richcgfxpassh---render-targets-and-passes)
  - [encoder.h - command recording and submission](#richcgfxencoderh---command-recording-and-submission)
  - [OpenGL 3.3 backend notes](#opengl-33-backend-notes)

---

## Application

`richc/app/`. A single-window application with an OpenGL context, driven by a
global event loop - so the functions take no handle. The backend (GLFW) is
private.

### richc/app/app.h - window and event loop

Open a window with `rc_app_init`, then drive the loop: `poll`,
`request_update`, `request_render`. Call the request functions rather than
invoking the callbacks directly - the backend also fires `on_render` from the
OS window-refresh, so rendering stays live during a modal resize; and when a
refresh has already rendered since the last `rc_app_poll`,
`rc_app_request_render` is a no-op, so each loop iteration draws at most one
frame and never blocks on a second vsync interval.

```c
rc_app_init(&(rc_app_desc) {
    .title     = RC_STR("My App"),
    .size      = {1280, 720},
    .resizable = true,
    .callbacks = {.ctx = &state, .on_render = my_render},
});
while (rc_app_is_running()) {
    rc_app_poll();
    rc_app_request_update();
    rc_app_request_render();
}
rc_app_destroy();
```

`rc_app_desc` holds `title` (`rc_str`), `size` (`rc_vec2i`), `resizable`,
`hidden` (create the window invisible - offscreen use, tests), and a
`callbacks` block. There are no graphics hints: colour space, depth, and MSAA
are configured in gfx descriptors, and an sRGB-capable default framebuffer is
requested unconditionally. Viewport state belongs to the renderer (gfx sets it
per pass); the app layer never touches it.

`rc_app_callbacks` - all optional (leave NULL to ignore), each receiving the
`ctx` field as its first argument:

| Callback | Fired |
|----------|-------|
| `on_key_down(ctx, scancode, mods)`<br>`on_key_up` | physical key press / release (repeats filtered out) |
| `on_key_char(ctx, codepoint, mods)` | Unicode text input, at the OS auto-repeat cadence - use for typing |
| `on_mouse_down(ctx, button, mods)`<br>`on_mouse_up` | mouse button press / release |
| `on_mouse_enter(ctx)`<br>`on_mouse_leave(ctx)` | cursor entered / left the client area |
| `on_mouse_move(ctx, pos)` | cursor moved; window pixels, origin top-left |
| `on_mouse_wheel(ctx, delta)` | scroll wheel / trackpad, in scroll-click units |
| `on_resize(ctx, size)` | framebuffer resized; physical pixels |
| `on_focus_gained/lost(ctx)`, `on_minimize/maximize(ctx)` | window state changes |
| `on_update(ctx, dt)` | frame update, via `rc_app_request_update` |
| `on_render(ctx, size)` | frame render, via `rc_app_request_render`; `size` is the framebuffer in physical pixels, only invoked while non-zero (never while minimised) |

| API | Description |
|-----|-------------|
| `rc_app_init(desc)` | open the window and establish the GL context |
| `rc_app_destroy()` | close the window and shut down |
| `rc_app_poll()` | pump OS events |
| `rc_app_is_running() -> bool` | false once the window should close |
| `rc_app_size() -> rc_vec2i` | framebuffer size in physical pixels |
| `rc_app_request_update()` | invoke `on_update` with the elapsed `dt` |
| `rc_app_request_render()` | invoke `on_render`, then swap buffers; no-op while minimised or already rendered this poll |
| `rc_app_swap_buffers()` | swap directly, when driving rendering outside `on_render` (e.g. a render thread) |
| `rc_app_time() -> double` | seconds since init; for animation timers |

Diagnostics - environment variables checked once at `rc_app_init`, logging to
stderr:

| Variable | Effect |
|----------|--------|
| `RC_APP_TRACE=1` | log window events, each render and swap duration, skipped duplicate renders, and any `rc_app_poll` over 1 ms |
| `RC_APP_TRACE_FINISH=1` | additionally `glFinish` between render and swap and report its duration - the true per-frame GPU cost async GL otherwise hides (serialises the GPU, so pacing shifts slightly) |

### richc/app/keys.h - scancodes, mouse buttons, modifiers

| Type | Description |
|------|-------------|
| `rc_scancode` | physical key positions (layout-independent): `RC_SCANCODE_A`..`_Z`, digits, `_SPACE`, punctuation, `_ESCAPE`, `_ENTER`, arrows, `_F1`..`_F12`, the keypad (`_KP_0`..), the modifier keys (`_LEFT_SHIFT`..), and `RC_SCANCODE_UNKNOWN` (-1). Values match GLFW key constants. Use scancodes for physical controls, `on_key_char` for text |
| `rc_mod` | modifier bit flags, OR-combined: `RC_MOD_SHIFT`, `RC_MOD_CTRL`, `RC_MOD_ALT`, `RC_MOD_SUPER`, `RC_MOD_CAPS`, `RC_MOD_NUMLOCK` |
| `rc_mouse_button` | `RC_MOUSE_BUTTON_LEFT` (0), `_RIGHT` (1), `_MIDDLE` (2) |

---

## Image

`richc/image/`. CPU-side images: a non-owning pixel window, a PNG decoder
built on core's inflate, and two atlas packers over core's rectangle packer.
`richc/image/array/image.h` provides the ready-made `rc_view/span/array_image`
family.

### richc/image/image.h - CPU image

`rc_image { rc_span_bytes data; rc_vec2i size; uint32_t stride; uint32_t
format; }` - a non-owning window over arena-backed pixel bytes; the arena owns
the bytes, the image just describes their layout. `format` holds an
`rc_pixel_format` value. The origin is the top-left
corner: pixels run left-to-right, rows top-to-bottom, `stride` bytes apart
(`stride >= size.x * bytes_per_pixel`). Pixels are read and written as a packed
`uint32_t` colour with R in bits 0-7, G in 8-15, B in 16-23, A in 24-31:
reading widens narrower formats (R8 -> opaque grayscale, RGB8 -> alpha 255) and
writing keeps only the channels the format stores, so one packed-colour path
serves every format.

| API | Description |
|-----|-------------|
| `rc_pixel_format` | `RC_PIXEL_FORMAT_NONE` (0), `_R8` (1), `_RGB8` (3), `_RGBA8` (4); the value is the bytes-per-pixel count |
| `rc_pixel_format_bytes_per_pixel(fmt) -> uint32_t` | that count |
| `rc_image_make(size, format, arena) -> rc_image` | fresh image, cleared to zero |
| `rc_image_make_filled(size, format, fill, arena) -> rc_image` | every pixel set to the packed colour `fill` |
| `rc_image_make_subimage(img, region) -> rc_image` | borrowed view of a `rc_box2i` region, clamped to bounds, sharing the parent's stride; an empty region yields a zero-size image still pointing into the parent |
| `rc_image_blit(dst, dst_pos, src) -> bool` | copy `src` into `dst` at `dst_pos`, clipping to `dst` and widening to `dst`'s format; false only if `src` is wider than `dst` (no narrowing). A fully clipped blit is a no-op returning true |
| `rc_image_get_pixel(img, x, y) -> uint32_t`<br>`rc_image_set_pixel(img, x, y, color)` | packed-colour access, dispatching on the format |
| `rc_image_get/set_pixel_r8/rgb8/rgba8(...)` | per-format variants that assume the format (asserted) and skip the dispatch, for hot loops |

### richc/image/image_png.h - PNG decoder

Decodes an in-memory PNG into an `rc_image`, decompressing the zlib-wrapped
IDAT stream with core's `rc_zip_inflate_zlib`. PNG bytes are untrusted input:
malformed data returns an error, never traps. Supported: grayscale,
truecolour, palette, and both alpha variants at 8 bits per channel, plus
1/2/4-bit grayscale and palette indices; all five scanline filters; multiple
IDAT chunks; palette transparency (tRNS). Not supported (reported
`_UNSUPPORTED`): 16-bit channels and Adam7 interlace. Chunk CRC-32s are not
validated - the IDAT Adler-32 that inflate checks already covers the pixels.

The `pixel_format_hint` widens but never narrows: the result format is the
wider of the PNG's natural richc format (R8 for grayscale, RGB8 for truecolour,
RGBA8 for the alpha variants, RGB8/RGBA8 for palette without/with tRNS) and the
hint; pass `RC_PIXEL_FORMAT_NONE` to keep the natural format.

| API | Description |
|-----|-------------|
| `rc_image_png_error` | `RC_IMAGE_PNG_OK`, `_ERROR_NOT_PNG`, `_ERROR_TRUNCATED`, `_ERROR_BAD_HEADER`, `_ERROR_UNSUPPORTED`, `_ERROR_BAD_DATA`, `_ERROR_IO` |
| `rc_image_from_png(png_data, hint, arena, scratch) -> rc_image_png_result` | `{ rc_image image; rc_image_png_error error; }`; pixels from `arena`, transients in the by-value `scratch` (necessarily a *different* arena). On error the image is the all-zero state |
| `rc_image_load_png(filename, hint, arena, scratch) -> rc_image_png_result` | convenience: read the file into `scratch` and decode, so only the image survives in `arena`; `_ERROR_IO` if it cannot be read |

### richc/image/image_pack.h - image atlas packer

Packs a known set of CPU images into a single atlas image (for upload as one
GPU texture) via core's [rectangle packer](core.md#rectangle-packing). The
atlas takes the widest pixel format among the inputs, each source is widened in
by `rc_image_blit`, and the atlas is cleared so the gaps read as zero. Use this
for known-up-front sets (sorted, denser); use `image_atlas` below when images
arrive incrementally.

| API | Description |
|-----|-------------|
| `rc_image_pack(images, size, spacing, arena, scratch) -> rc_image_pack_result` | `{ rc_image image; rc_span_vec2i positions; }` - `positions[i]` is the top-left of `images[i]` (derive a rect from `position + images[i].size`). Atlas pixels and positions from `arena`; transient packing state in the by-value `scratch` |

`size` is the atlas dimensions, or `{0, 0}` to auto-size: it starts from a
power-of-two square (or 2:1 rectangle) covering the total image area and grows,
alternately doubling width then height, until everything fits (the chosen size
is `result.image.size`). Packing fails - the all-zero result - if `images` is
empty, a fixed `size` cannot hold every image, or an auto-sized atlas would
exceed the addressable byte size.

### richc/image/image_atlas.h - incremental image atlas

`rc_image_atlas { rc_image image; rc_rect_pack packer; }` - the incremental
counterpart to `image_pack`: add images one at a time over a session (e.g. font
glyphs as first encountered), with earlier placements staying put. One arena
and no scratch: the atlas pixels are allocated once and never grow, so the
packer's free list is the arena's only growable. Pass the same arena to `make`
and every `add` (the atlas captures no arena); read `atlas.image` to upload or
sample.

| API | Description |
|-----|-------------|
| `rc_image_atlas_make(size, format, spacing, arena) -> rc_image_atlas` | a cleared atlas of the given size and format, keeping a `spacing`-pixel gap between images |
| `rc_image_atlas_add(atlas, src, arena) -> rc_rect_pack_result` | place `src` and blit it in; `{ rc_vec2i pos; bool placed; }`, `placed` false when it no longer fits. `src` must be valid and no wider than the atlas format |

---

## Font

`richc/font/`. TrueType loading and signed-distance-field glyph rasterisation,
written from scratch (no stb/freetype), plus a glyph-table-and-atlas tier on
top. The SDF is analytic - exact distance to each line and quadratic-Bezier
edge with a nonzero-winding inside test, via core's `math/solve.h` - not a
rasterise-then-distance-transform. Metrics are full-precision floats from true
font units and the fractional placement is carried in the field itself, so one
rasterisation per glyph reproduces every subpixel position: keep the pen in
floats and draw each image at `pen + offset`. Scope: cmap formats 0/4/6/12 and
simple glyph outlines; composite glyphs currently yield an empty image with a
correct advance. The reference SDF shader lives in `example/text`.

### richc/font/font.h - TrueType loading and SDF glyphs

`rc_font_make` parses an in-memory `.ttf` and configures it for one
rasterisation size (`pixel_size` is the em height in pixels; `scale =
pixel_size / unitsPerEm`). It **borrows** the ttf bytes - outlines are read
lazily by `rc_font_get_glyph` - so they must outlive the font. Font data is
untrusted input: validates and returns an `rc_font_error`, never traps.

Each glyph image is `RC_PIXEL_FORMAT_R8` with 128 exactly on the outline,
brighter inside: byte = `clamp(round(128 + distance_px * 128 / spread), 0,
255)`. `spread` - the distance in pixels mapped onto the full byte range, and
equally the padding margin around the glyph - is derived from `pixel_size` and
exposed on `rc_font`, because an SDF shader needs it to turn sampled values
back into distances. Images are sized to drop straight into `rc_image_atlas`.

| API | Description |
|-----|-------------|
| `rc_font_error` | `RC_FONT_OK`, `RC_FONT_ERROR_NOT_TTF`, `_ERROR_TRUNCATED`, `_ERROR_BAD_TABLE`, `_ERROR_UNSUPPORTED` (e.g. no usable cmap subtable) |
| `rc_font` | public fields, all in pixels: `ascent` (baseline to top, >= 0), `descent` (baseline to bottom, <= 0), `line_gap` (extra leading), `spread` (SDF half-range and padding). The trailing-`_` fields are internal parse state |
| `rc_font_make(ttf, pixel_size, arena) -> rc_font_result` | `{ rc_font font; rc_font_error error; }`; the font is zeroed on error |
| `rc_font_glyph` | `{ rc_image image; rc_vec2f offset; float advance; uint32_t codepoint, index; }` - the R8 SDF (empty for whitespace and outline-less glyphs, which still carry a valid advance), the image top-left relative to the pen origin (screen y-down), the pen advance, and the resolved glyph index (0 = .notdef) |
| `rc_font_get_glyph(font, codepoint, arena, scratch) -> rc_font_glyph_result` | `{ rc_font_glyph glyph; rc_font_error error; }` - rasterise one glyph; image from `arena`, transient outline in the by-value `scratch` (a distinct arena). An unmapped codepoint resolves to .notdef, not an error |

### richc/font/font_atlas.h - glyph table and atlas builder

The tier above `rc_font_get_glyph`: two types with deliberately different
lifetimes. `rc_glyph_table` is the persistent data you keep to lay out text - a
codepoint-to-`rc_glyph` map with a direct-indexed printable-ASCII block
(`RC_GLYPH_ASCII_FIRST` 0x20 .. `RC_GLYPH_ASCII_LAST` 0x7E,
`RC_GLYPH_ASCII_COUNT` 95) and a codepoint-sorted tail for everything else.
`rc_font_atlas` is the transient builder - the font, the R8 SDF atlas pixels,
and a retained rectangle packer. Fill it, upload `atlas.image` to a GPU
texture, then free `build_arena`; the table lives on in `table_arena`. Neither
type captures an arena; renders are transient (by-value arena copies), so no
call needs a separate scratch.

Layout model: a pen sits on the baseline. For each glyph, draw a quad at
`pen + offset` (pixels, screen y-down) of pixel size `uv.size *
atlas_dimensions`, textured by `uv`, then advance the pen by `advance`. The
quad carries the SDF spread padding (the shader thresholds it away); line pitch
uses the font's `ascent` / `descent` / `line_gap`.

| API | Description |
|-----|-------------|
| `rc_glyph` | `{ rc_box2f uv; rc_vec2f offset; float advance; bool placed; }` - a normalized [0,1] atlas rect (zero-size for whitespace; not a pointer, so it survives freeing the pixels), pen-to-quad-top-left offset, true advance, and `placed` false when it did not fit (advance still valid) |
| `rc_glyph_table_make(table_arena) -> rc_glyph_table` | an empty table (allocates the ASCII block) |
| `rc_glyph_table_find(table, codepoint) -> rc_glyph` | pure lookup - no font, no arenas, never renders, so it is the call to use after `build_arena` is freed. `{ .placed = false }` for a codepoint never added |
| `rc_font_atlas_make(font, size, spacing, build_arena) -> rc_font_atlas` | a cleared size-by-size R8 atlas keeping `spacing` px between glyphs; takes the font by value (which still borrows the ttf bytes) |
| `rc_font_atlas_add(atlas, table, codepoint, build_arena, table_arena) -> rc_glyph` | rasterise, pack, blit, and record one codepoint; idempotent (an already-present codepoint returns from the table without re-rendering). Overflow -> `placed` false |
| `rc_font_atlas_add_range(atlas, table, first, last, build_arena, table_arena) -> uint32_t` | pre-load the codepoint range `[first, last]` (e.g. 0x20..0x7E), packed densest-first like `rc_rect_pack_all`; records every entry, returns the count that did not fit |

---

## Gfx

`richc/gfx/`. A backend-agnostic GPU layer whose object model follows WebGPU:
immutable descriptor structs where zero means default, typed handles, bind
groups against explicit layouts, pipelines that bake shader + vertex layout +
render state, render passes with per-attachment load/store actions, and
arena-backed command encoders. The phase-1 backend is OpenGL 3.3 core,
selected at compile time (CMake option `RICHC_GFX_BACKEND`, default `gl33`;
define `RC_GFX_BACKEND_GL33`) - no vtable, no runtime polymorphism. There is
no umbrella header; include the category headers you use.

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
semantics historically diverge across drivers).

### Colour and sRGB

Everything a shader sees is linear. Encoding is applied by fixed-function
hardware on read (sRGB texture formats decode before filtering) and on write
(sRGB render target formats encode after blending, which therefore blends in
linear space). There is no shader-side gamma math. Consequences:

- Clear values and the blend constant are **linear**, even for sRGB
  attachments (an artist's "mid grey" is 0.216, not 0.5).
- Alpha is never encoded; sRGB applies to RGB only.
- Hex colour constants from design tools are sRGB-encoded: convert with
  `richc/gfx/color.h` before they reach a shader.
- Albedo / UI art / emissive textures want `_SRGB` formats; normal maps,
  masks, font atlases, and data textures want `_UNORM` / float formats.
- The swapchain colour space is set at init: `RC_GFX_COLOR_SPACE_SRGB`
  (default) or `_LINEAR` (no encode; for bit-exact output such as an
  emulator's framebuffer, or a pipeline applying its own display transform).

### richc/gfx/gfx.h - device, handles, shared vocabulary

Handles are distinct one-member structs wrapping core's 8-byte
`rc_genpool_handle` (member `h`: slot index + generation), so handles of
different resource types cannot be mixed: `rc_gfx_buffer`, `rc_gfx_texture`,
`rc_gfx_sampler`, `rc_gfx_shader`, `rc_gfx_bind_group_layout`,
`rc_gfx_bind_group`, `rc_gfx_pipeline_layout`, `rc_gfx_pipeline`,
`rc_gfx_render_target`. `{0}` is the invalid "none" handle; generations bump on
destroy, so stale handles trap rather than aliasing. Inspect through the
`rc_genpool_handle_*` functions on the `h` member.

Limits: `RC_GFX_MAX_BIND_GROUPS` (4), `RC_GFX_MAX_BINDINGS_PER_GROUP` (16),
`RC_GFX_MAX_VERTEX_BUFFERS` (8), `RC_GFX_MAX_VERTEX_ATTRIBUTES` (16),
`RC_GFX_MAX_COLOR_ATTACHMENTS` (4), `RC_GFX_MAX_MIP_LEVELS` (16),
`RC_GFX_FRAMES_IN_FLIGHT` (2), `RC_GFX_UNIFORM_ALIGN` (256).

Shared enums: `rc_gfx_compare` (ALWAYS default), `rc_gfx_index_format`
(NONE/U16/U32), `rc_gfx_stage` bit flags (VERTEX, FRAGMENT),
`rc_gfx_color_space` (SRGB default, LINEAR). `rc_gfx_texture_format` covers
R8/RG8/RGBA8/BGRA8 (+`_SRGB` variants), 8/16/32-bit UINT, 16F/32F float,
RGB10A2_UNORM, RG11B10F, the depth/stencil formats (DEPTH16_UNORM,
DEPTH24_PLUS, DEPTH32F, DEPTH24_PLUS_STENCIL8, DEPTH32F_STENCIL8), and the BC
compressed families (BC1/3/4/5/6H/7).

| API | Description |
|-----|-------------|
| `rc_gfx_texture_format_is_srgb/_is_depth/_is_stencil/_is_compressed(fmt) -> bool` | format classification |
| `rc_gfx_texture_format_block_size(fmt) -> uint32_t`<br>`_block_dim(fmt) -> rc_vec2i` | bytes per block; 1x1 or 4x4 |
| `rc_gfx_texture_format_to_linear(fmt)`<br>`_to_srgb(fmt)` | sRGB pairing; identity when there is none |
| `rc_gfx_desc` | `arena` (required; gfx's persistent allocations), `color_space`, `swapchain_depth_format` (NONE = colour-only; e.g. DEPTH32F gives the swapchain target a window-sized depth/stencil buffer), `swapchain_sample_count` (0/1 = no MSAA), `uniform_ring_size` (bytes per in-flight frame, 0 => 1 MB), `validation` (extra load-time checks; forced on in debug) |
| `rc_gfx_init(desc)`<br>`rc_gfx_shutdown()` | device lifecycle; shutdown destroys anything still alive |
| `rc_gfx_begin_frame(size)` | start a frame at the current framebuffer size (the swapchain target recreates on change) |
| `rc_gfx_end_frame()` | present pass + retire deferred destructions (destroys are deferred `RC_GFX_FRAMES_IN_FLIGHT` frames; the swap itself stays with the app layer) |
| `rc_gfx_swapchain_size() -> rc_vec2i`<br>`rc_gfx_swapchain_format()`<br>`rc_gfx_swapchain_depth_format()` | swapchain queries (depth NONE when colour-only) |
| `rc_gfx_features_query() -> rc_gfx_features` | bools: compute, storage_buffers, storage_buffers_via_tbo, base_instance, indirect_draw, native_depth_zero_to_one, cube_map_array, texture_view, anisotropic_filtering, srgb_default_framebuffer, debug_markers, timer_queries |
| `rc_gfx_limits_query() -> rc_gfx_limits` | texture sizes, array layers, colour attachments, vertex attributes, UBO range/alignment, MSAA samples, anisotropy |
| `rc_gfx_format_caps_query(fmt) -> uint32_t` | `rc_gfx_format_caps` bit flags: SAMPLE, FILTER, RENDER, BLEND, MSAA, RESOLVE |
| `rc_gfx_backend_name() -> rc_str` | e.g. "gl33" |

### richc/gfx/color.h - sRGB conversion helpers

The exact piecewise sRGB curve (not a 2.2 power approximation - the linear
segment near black matters for dark UI colours). Alpha is never converted.

| API | Description |
|-----|-------------|
| `rc_gfx_srgb_to_linear(s)`<br>`rc_gfx_linear_to_srgb(l) -> float` | one channel |
| `rc_gfx_color_from_srgb_u32(rgba) -> rc_vec4f` | packed `0xAABBGGRR` (R low byte) -> linear |
| `rc_gfx_color_to_srgb_u32(linear) -> uint32_t` | the inverse; clamps to [0, 1] |

### richc/gfx/buffer.h - GPU buffers

| API | Description |
|-----|-------------|
| `rc_gfx_buffer_usage` | bit flags: VERTEX, INDEX, UNIFORM, STORAGE, COPY_SRC, COPY_DST |
| `rc_gfx_buffer_update_mode` | IMMUTABLE (default; contents at creation, never changed), DYNAMIC (occasional updates), STREAM (rewritten per frame) |
| `rc_gfx_buffer_desc` | `size` (required), `usage` (required), `update`, `data` (initial contents; required if IMMUTABLE), `label` |
| `rc_gfx_buffer_make(desc) -> rc_gfx_buffer`<br>`rc_gfx_buffer_destroy(buf)` | lifecycle |
| `rc_gfx_buffer_update(buf, offset, data)` | whole-or-partial update of a DYNAMIC or STREAM buffer; takes effect at that point in submission order, not between pass begin/end |

### richc/gfx/texture.h - textures and samplers

Samplers are separate objects on every backend (the GL backend re-combines
them with textures internally).

| API | Description |
|-----|-------------|
| `rc_gfx_texture_dim` | 2D (default), 2D_ARRAY, 3D, CUBE |
| `rc_gfx_texture_sample_type` | FLOAT (default), UNFILTERABLE_FLOAT, DEPTH, SINT, UINT |
| `rc_gfx_texture_usage` | bit flags: SAMPLED, RENDER_ATTACHMENT, COPY_SRC, COPY_DST |
| `rc_gfx_filter`<br>`rc_gfx_address` | NEAREST (default), LINEAR / CLAMP_TO_EDGE (default), REPEAT, MIRROR_REPEAT, CLAMP_TO_BORDER |
| `rc_gfx_texture_desc` | `dim`, `format` (required), `size` (required), `depth` (3D depth or array layers, default 1; CUBE is 6), `mip_count` (0 => 1), `sample_count` (0/1 => no MSAA; MSAA is 2D, one mip, no data), `usage` (default SAMPLED), `data` (optional `rc_gfx_texture_data { subresources; count }`, indexed `[slice * mip_count + mip]`, per-mip whole volumes for 3D; rows top-first, tightly packed to the block size), `label` |
| `rc_gfx_texture_make(desc) -> rc_gfx_texture`<br>`rc_gfx_texture_destroy(tex)` | lifecycle |
| `rc_gfx_texture_update(tex, mip, slice, region, data)` | region update; uncompressed formats only, tightly packed rows covering the region |
| `rc_gfx_texture_generate_mipmaps(tex)` | GPU mip generation; convenience only (driver sRGB downsampling has diverged historically - generate mips on the CPU in linear space where quality matters) |
| `rc_gfx_mip_count(size) -> uint32_t` | mips in a full chain |
| `rc_gfx_texture_from_image(img, srgb, mipmaps) -> rc_gfx_texture` | bridge from `rc_image`: R8 -> R8_UNORM (never sRGB), RGB8 widens to RGBA on upload, RGBA8 -> RGBA8_SRGB or _UNORM per `srgb` |
| `rc_gfx_sampler_desc` | min/mag/mip filters, address_u/v/w, lod_min / lod_max (0 => 1000), max_anisotropy (0/1 = off), `compare` (any value other than ALWAYS makes a comparison sampler), `border_color` (linear), `label` |
| `rc_gfx_sampler_make(desc) -> rc_gfx_sampler`<br>`rc_gfx_sampler_destroy(smp)` | lifecycle |

### richc/gfx/shader.h - shaders

Phase 1 takes GLSL 330 source directly (no shading-language abstraction). The
backend prepends a preamble - sources must not contain a `#version` line -
defining `RC_GFX_BACKEND_GL33`, `RC_STORAGE_LOAD(name, i)` (the storage-buffer
access macro: `texelFetch` on a `samplerBuffer` under GL 3.3, a buffer index
elsewhere), and `rc_clip(...)`, the injected hook the vertex shader must write
`gl_Position` through - it absorbs every clip-space convention difference
between backends. Vertex attributes are matched by `layout(location = N)`
only. All shader constants live in std140 uniform blocks declared without
instance names; no loose uniforms, and no gamma math ever.

| API | Description |
|-----|-------------|
| `rc_gfx_uniform_member` | `name` (GLSL member), `offset` / `size` (from the C struct): optional per-block table for debug std140 validation, turning silent padding corruption (vec3, mat3, arrays) into a load-time trap |
| `rc_gfx_shader_uniform_block` | `glsl_name`, `group`, `binding`, `size` (sizeof the C struct), optional `members` / `member_count` |
| `rc_gfx_shader_texture_sampler_pair` | maps a GLSL combined sampler uniform back onto separate texture and sampler bindings; for a `samplerBuffer` backing a STORAGE_BUFFER_READ binding, point the texture fields at it and leave the sampler fields unused. Dropped entirely under SPIR-V / WGSL backends |
| `rc_gfx_shader_desc` | `vs_source` / `fs_source` (GLSL 330 bodies), the uniform-block and texture-sampler tables, `label` |
| `rc_gfx_shader_make(desc) -> rc_gfx_shader`<br>`rc_gfx_shader_destroy(shd)` | lifecycle |

### richc/gfx/bindings.h - bind groups and layouts

Group indices are by update frequency - follow this convention in every
renderer built on the layer: group 0 per frame (camera, time, environment),
group 1 per pass, group 2 per material, group 3 per draw (dynamic offsets).
Bind groups are immutable; a material creates one once at load time.

| API | Description |
|-----|-------------|
| `rc_gfx_binding_type` | UNIFORM_BUFFER, TEXTURE, SAMPLER, COMPARISON_SAMPLER, STORAGE_BUFFER_READ (a TBO on GL 3.3, a real read-only storage buffer later, hidden behind `RC_STORAGE_LOAD`) |
| `rc_gfx_bind_group_layout_entry` | `binding`, `visibility` (stage flags), `type`, plus per-type fields: uniform buffers take `has_dynamic_offset` and `min_binding_size` (0 = unvalidated; set it, it catches std140 bugs), textures take `texture_dim` / `sample_type` / `multisampled`, storage buffers take `texel_format` |
| `rc_gfx_bind_group_layout_desc` | entries + count + label |
| `rc_gfx_bind_group_layout_make(desc)`<br>`_destroy(l)` | layout lifecycle |
| `rc_gfx_pipeline_layout_desc` | up to 4 group layouts, in group order; 0 groups is a valid empty layout |
| `rc_gfx_pipeline_layout_make(desc)`<br>`_destroy(l)` | pipeline-layout lifecycle |
| `rc_gfx_simple_layout_make(entries, entry_count, label) -> rc_gfx_simple_layout` | the common one-group case in one call: `{ layout; group0; }`; the internal group layout is owned by (and destroyed with) the pipeline layout |
| `rc_gfx_bind_group_entry` | `binding` plus exactly one of `buffer` (+ `buffer_offset`, 256-aligned, and `buffer_size`, 0 => rest of buffer), `texture`, `sampler` |
| `rc_gfx_bind_group_desc` | layout + entries, one per layout entry |
| `rc_gfx_bind_group_make(desc)`<br>`_destroy(g)` | bind-group lifecycle; dynamic offsets are consumed in increasing binding order at bind time |

### richc/gfx/pipeline.h - pipelines

Colour formats, depth format, and sample count are baked into the pipeline
(the D3D12/Vulkan PSO contract) and validated against the render target when
it is bound.

| API | Description |
|-----|-------------|
| `rc_gfx_vertex_format` | U8/I8/U16/I16 x2/x4 (plain and `_NORM`), F16X2/X4, F32/U32/I32 x1..x4, RGB10A2_NORM |
| `rc_gfx_primitive` | TRIANGLES (default), TRIANGLE_STRIP, LINES, LINE_STRIP, POINTS |
| `rc_gfx_stencil_op`, `rc_gfx_blend_factor`, `rc_gfx_blend_op`, `rc_gfx_cull` (NONE default), `rc_gfx_front_face` (CCW default) | render-state enums |
| `rc_gfx_color_mask` | write-mask bits DISABLE a channel, so 0 means "write RGBA" and `{0}` stays a valid default |
| `rc_gfx_vertex_layout` | up to 8 `rc_gfx_vertex_buffer_layout` (stride 0 => computed from the attributes, `per_instance`, `step_rate` 0 => 1) and 16 `rc_gfx_vertex_attribute` (`location`, `buffer_index`, `offset` 0 => packed after the previous attribute of the same buffer, `format`) |
| `rc_gfx_depth_stencil_state` | `format` (NONE => no depth attachment), `depth_write`, `depth_compare` (GREATER_EQUAL for reverse-Z), `stencil_enabled` + front/back `rc_gfx_stencil_face` + read/write masks (0 => 0xFF), depth bias / slope scale / clamp |
| `rc_gfx_blend_state`<br>`rc_gfx_color_target_state` | enabled + colour and alpha factor/op pairs / `format` (required), `blend`, `write_mask` |
| `rc_gfx_pipeline_desc` | `shader` + `layout` (required), `vertex_layout`, `primitive`, `index_format` (NONE => non-indexed only), `cull`, `front_face`, `colors` + `color_count`, `depth_stencil`, `sample_count`, `alpha_to_coverage`, `label` |
| `rc_gfx_pipeline_make(desc) -> rc_gfx_pipeline`<br>`rc_gfx_pipeline_destroy(pip)` | lifecycle |
| `rc_gfx_blend_state_make_alpha()`<br>`_make_premultiplied()`<br>`_make_additive()` | blend helpers (all linear, like every blend). Prefer premultiplied for anything composited more than once - the only form that composes associatively |

GL 3.3 note: blending has per-target enables but one blend function, so every
enabled target must share target 0's blend state (validated).

### richc/gfx/pass.h - render targets and passes

A render target bakes the attachment set; load/store actions are chosen
per-pass. A pass target of `{0}` is the swapchain target: the internal
window-sized colour image `rc_gfx_end_frame` presents, plus the optional
depth/stencil buffer requested via `rc_gfx_desc.swapchain_depth_format`. A
pipeline drawing to it must declare exactly that depth format - NONE when no
depth was requested (validated at bind). It has no texture handle, so it can
never be sampled: anything that must be read back (post-processing inputs)
renders to an explicit render target instead.

| API | Description |
|-----|-------------|
| `rc_gfx_load_op`<br>`rc_gfx_store_op` | CLEAR (default), LOAD, DISCARD / STORE (default), DISCARD, RESOLVE |
| `rc_gfx_attachment` | `texture`, `mip`, `slice` (array layer or cube face) |
| `rc_gfx_render_target_desc` | colour attachments + count, `depth_stencil`, `resolves` (MSAA resolve destinations: single-sample, same format), `label` |
| `rc_gfx_render_target_make(desc)`<br>`_destroy(rt)`<br>`rc_gfx_render_target_size(rt) -> rc_vec2i` | lifecycle and size |
| `rc_gfx_color_attachment_action` | `load_op`, `store_op`, `clear_value` (**linear**, even for sRGB attachments) |
| `rc_gfx_depth_stencil_action` | depth and stencil load/store + clear values (reverse-Z: clear depth to 0.0) |
| `rc_gfx_pass_desc` | `target` (`{0}` => the swapchain target), per-attachment actions, `label` |

MSAA resolve is `RC_GFX_STORE_OP_RESOLVE` on a colour attachment whose render
target has a resolve destination; it happens at pass end. A multisampled
swapchain (`swapchain_sample_count > 1`) resolves internally at end of frame.

### richc/gfx/encoder.h - command recording and submission

Commands are recorded into an arena-backed encoder as a packed opcode stream
and played back at submit. Recording touches no device state, so each thread
may record with its own arena and encoder; `rc_gfx_submit` plays buffers back
in order on the context thread. A command buffer stays valid until its arena
is reset (typical use: a per-frame arena reset each `rc_gfx_begin_frame`).

| API | Description |
|-----|-------------|
| `rc_gfx_encoder_begin(arena) -> rc_gfx_encoder *` | start recording |
| `rc_gfx_encoder_finish(enc) -> rc_gfx_cmd_buffer` | `{ data; size; }` - the recorded stream |
| `rc_gfx_submit(buffers, count)` | play command buffers back in order; context thread only |
| `rc_gfx_encoder_pass_begin(enc, desc)`<br>`_pass_end(enc)` | bracket a pass; viewport and scissor default to the full target, pipeline and binding state starts clean |
| `rc_gfx_encoder_set_pipeline(enc, pip)` | bind a pipeline |
| `rc_gfx_encoder_set_bind_group(enc, group_index, group, dynamic_offsets, count)` | bind a group; offsets add to the entries' `buffer_offset` |
| `rc_gfx_encoder_set_vertex_buffer(enc, slot, buf, offset)`<br>`_set_index_buffer(enc, buf, fmt, offset)` | bind geometry |
| `rc_gfx_encoder_set_viewport(enc, rect, min_depth, max_depth)`<br>`_set_scissor(enc, rect)` | rects in pixels, origin top-left |
| `rc_gfx_encoder_set_blend_constant(enc, color)` | linear, like every blend value |
| `rc_gfx_encoder_set_stencil_reference(enc, ref)` | stencil reference value |
| `rc_gfx_encoder_draw(enc, desc)` | `rc_gfx_draw_desc { vertex_count, instance_count (0 => 1), first_vertex, first_instance }` |
| `rc_gfx_encoder_draw_indexed(enc, desc)` | `rc_gfx_draw_indexed_desc { index_count, instance_count, first_index, base_vertex, first_instance }`. `first_instance != 0` requires `features.base_instance` (absent on GL 3.3) |
| `rc_gfx_uniform_buffer() -> rc_gfx_buffer` | the uniform ring's constant buffer handle; reference it from bind groups, created once |
| `rc_gfx_encoder_alloc_uniforms(enc, size) -> rc_gfx_uniform_alloc` | `{ ptr; buffer; offset; }` - write the std140 struct through `ptr` (valid until end of frame), pass `offset` as the dynamic offset. 256-aligned from a per-frame region of one ring buffer with `RC_GFX_FRAMES_IN_FLIGHT` rotating regions |
| `rc_gfx_encoder_push_debug_group(enc, label)`<br>`_pop_debug_group(enc)` | debug markers (no-ops without GL_KHR_debug) |
| `rc_gfx_cmd_buffer_dump(cb, arena) -> rc_mstr` | human-readable one-command-per-line decode; needs no device |

### OpenGL 3.3 backend notes

- Object mapping: buffers and textures are single GL objects; a pipeline is a
  CPU-side state record (no GL object); a bind group is a resolved unit table;
  a render target is an FBO (plus a resolve FBO); vertex layout lives in an
  internal VAO cache keyed on (pipeline, vertex buffers, offsets), populated
  lazily at draw and evicted on buffer/pipeline destruction.
- Bindings flatten to sequential GL units per class (UBO binding points,
  texture units) by walking groups in order at pipeline layout creation. GLSL
  330 has no `layout(binding=)`, so block and sampler uniforms are assigned at
  pipeline creation; a shader may be shared across pipelines only where the
  flat mapping is identical (asserted).
- `rc_clip` under GL 3.3 negates y; front-face winding is inverted globally to
  compensate. With ARB_clip_control (in the vendored loader, exposed on
  essentially every desktop driver) init calls
  `glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)` and `rc_clip` passes z
  through, so GL consumes canonical [0,1] reverse-Z natively
  (`features.native_depth_zero_to_one` true). Without it `rc_clip` rewrites z
  as `2z - w` - correct, but reverse-Z precision at distance degrades. The
  clip origin stays lower-left in both cases, so the y path never varies.
- Not available on this backend (reported through `rc_gfx_features`): compute,
  read-write storage buffers, `first_instance != 0`, indirect draw, cube map
  arrays, texture views, debug markers. STORAGE_BUFFER_READ is supported via
  texture buffer objects. Store/load DISCARD is a no-op
  (`glInvalidateFramebuffer` is GL 4.3).
