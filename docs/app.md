# richc app reference

Per-header reference for the **app** layer (the `richc_app` library, built on
core): windowing, input, and CPU-side image loading, packing, and atlasing. See
[overview.md](overview.md) for the shared philosophy and conventions, and
[core.md](core.md) for the core layer.

## richc/app/app.h - window and event loop

The headers below belong to the **app layer** - link the `richc_app` target
(which pulls in core, GLFW, and glad). `rc_app` is a single-window application
with an OpenGL context, driven by a global event loop, so its functions take no
handle. GLFW and glad are private to the backend and never appear in the API.

- `rc_app_desc` configures the window: `title` (`rc_str`), `size` (`rc_vec2i`),
  `resizable`, the graphics hints `srgb` / `depth_bits` (0 = none) /
  `msaa_samples` (0 or 1 = none), and a `callbacks` block.
- `rc_app_callbacks` holds optional function pointers (leave any NULL to ignore
  that event) plus a `ctx` forwarded as the first argument of every callback:
  keyboard (`on_key_down` / `on_key_up` with an `rc_scancode` and `rc_mod`,
  `on_key_char` for Unicode text codepoints), mouse (`on_mouse_down` / `_up` /
  `_enter` / `_leave` / `_move` / `_wheel`), window state (`on_resize`,
  `on_focus_gained` / `_lost`, `on_minimize` / `_maximize`), and the frame
  callbacks `on_update(ctx, dt)` and `on_render(ctx)`.
- Lifecycle: `rc_app_init(const rc_app_desc *)`, `rc_app_destroy()`,
  `rc_app_poll()` (pump OS events), `rc_app_is_running()` -> `bool`,
  `rc_app_size()` -> `rc_vec2i` (framebuffer pixels), `rc_app_time()` -> `double`
  (seconds since init, for animation timers).
- Frames: drive the loop with `rc_app_request_update()` (invokes `on_update` with
  the elapsed `dt`) and `rc_app_request_render()` (sets the viewport to the full
  framebuffer, invokes `on_render`, then swaps buffers) rather than calling the
  callbacks directly - the backend also fires `on_render` from the OS
  window-refresh, so rendering stays live during a modal resize. Use
  `rc_app_swap_buffers()` to swap directly when driving rendering elsewhere (e.g.
  a render thread).

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
