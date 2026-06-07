#include "richc/image/image.h"

/* ---- construction ---- */

rc_image rc_image_make(rc_vec2i size, rc_pixel_format format, rc_arena *arena)
{
    RC_ASSERT(format != RC_PIXEL_FORMAT_NONE);
    RC_ASSERT(size.x >= 0 && size.y >= 0);

    uint32_t bpp    = rc_pixel_format_bytes_per_pixel(format);
    uint32_t stride = (uint32_t)size.x * bpp;
    uint32_t total  = (uint32_t)size.y * stride;

    // push_n_zero sizes the buffer to `total` zeroed bytes, so the image is
    // already cleared - clearing is format-agnostic, hence no per-format split.
    rc_array_bytes arr = rc_array_bytes_make(total, arena);
    rc_array_bytes_push_n_zero(&arr, total, arena);

    return (rc_image) {
        .data   = arr.span,
        .size   = size,
        .stride = stride,
        .format = format
    };
}

// Fill helpers, one per format: write the format's bytes of the packed colour
// across the (contiguous, unpadded) freshly-made buffer, branch hoisted out.
static void image_fill_r8(rc_image img, uint32_t fill)
{
    uint8_t v = (uint8_t)fill;
    for (uint32_t i = 0; i < img.data.num; i++)
        rc_span_bytes_set(img.data, i, v);
}

static void image_fill_rgb8(rc_image img, uint32_t fill)
{
    uint8_t r = (uint8_t)fill, g = (uint8_t)(fill >> 8), b = (uint8_t)(fill >> 16);
    for (uint32_t off = 0; off < img.data.num; off += 3u) {
        rc_span_bytes_set(img.data, off,     r);
        rc_span_bytes_set(img.data, off + 1, g);
        rc_span_bytes_set(img.data, off + 2, b);
    }
}

static void image_fill_rgba8(rc_image img, uint32_t fill)
{
    uint8_t r = (uint8_t)fill,         g = (uint8_t)(fill >> 8);
    uint8_t b = (uint8_t)(fill >> 16), a = (uint8_t)(fill >> 24);
    for (uint32_t off = 0; off < img.data.num; off += 4u) {
        rc_span_bytes_set(img.data, off,     r);
        rc_span_bytes_set(img.data, off + 1, g);
        rc_span_bytes_set(img.data, off + 2, b);
        rc_span_bytes_set(img.data, off + 3, a);
    }
}

rc_image rc_image_make_filled(rc_vec2i size, rc_pixel_format format, uint32_t fill, rc_arena *arena)
{
    rc_image img = rc_image_make(size, format, arena);
    switch (format) {
        case RC_PIXEL_FORMAT_R8:    image_fill_r8(img, fill);    break;
        case RC_PIXEL_FORMAT_RGB8:  image_fill_rgb8(img, fill);  break;
        case RC_PIXEL_FORMAT_RGBA8: image_fill_rgba8(img, fill); break;
        case RC_PIXEL_FORMAT_NONE:
        default:                    break;
    }
    return img;
}

rc_image rc_image_make_subimage(rc_image img, rc_box2i region)
{
    // Clamp the requested region to the image's bounds.
    rc_vec2i lo = rc_box2i_min(region);
    rc_vec2i hi = rc_box2i_max(region);
    int32_t  x0 = lo.x < 0          ? 0          : lo.x;
    int32_t  y0 = lo.y < 0          ? 0          : lo.y;
    int32_t  x1 = hi.x > img.size.x ? img.size.x : hi.x;
    int32_t  y1 = hi.y > img.size.y ? img.size.y : hi.y;

    if (x0 >= x1 || y0 >= y1) {
        // Empty region: an invalid image that still carries the parent's layout.
        return (rc_image) {
            .data   = rc_span_bytes_make(NULL, 0),
            .size   = rc_vec2i_make(0, 0),
            .stride = img.stride,
            .format = img.format
        };
    }

    uint32_t bpp     = rc_pixel_format_bytes_per_pixel(img.format);
    rc_vec2i subsize = rc_vec2i_make(x1 - x0, y1 - y0);
    uint32_t offset  = (uint32_t)y0 * img.stride + (uint32_t)x0 * bpp;
    // The borrowed span shares the parent's stride, so it spans every row of the
    // region: all but the last full row, plus the last row's pixels.
    uint32_t num     = (uint32_t)(subsize.y - 1) * img.stride + (uint32_t)subsize.x * bpp;

    return (rc_image) {
        .data   = rc_span_bytes_make(img.data.data + offset, num),
        .size   = subsize,
        .stride = img.stride,
        .format = img.format
    };
}

/* ---- blit ---- */

// The clipped source/destination rectangle shared by all three blit variants.
typedef struct image_blit_clip {
    rc_vec2i src;    // source origin (after clipping)
    rc_vec2i dst;    // destination origin (after clipping)
    rc_vec2i size;   // overlap extent (zero on each axis if fully clipped)
} image_blit_clip;

// Clip src placed at dst_pos against dst's bounds: the overlap, in destination
// coordinates, of the source rect [dst_pos, dst_pos + src.size) with the
// destination rect [0, dst.size).  The source origin follows by subtracting
// dst_pos back out.  The common preamble of blit.
static image_blit_clip image_blit_clip_compute(rc_image dst, rc_vec2i dst_pos, rc_image src)
{
    rc_box2i overlap = rc_box2i_intersection(
        rc_box2i_make_pos_size(dst_pos, src.size),
        rc_box2i_make_pos_size(rc_vec2i_make(0, 0), dst.size));
    rc_vec2i lo = rc_box2i_min(overlap);

    return (image_blit_clip) {
        .src  = rc_vec2i_sub(lo, dst_pos),
        .dst  = lo,
        .size = rc_box2i_size(overlap)
    };
}

// Per-pixel copy loop, monomorphised over the chosen read/write helpers so the
// format branch lives outside the inner loop.  get widens the source to a packed
// colour; set narrows it to the destination format.
#define RC_IMAGE_BLIT_LOOP_(get, set)                                              \
    for (int32_t y = 0; y < c.size.y; y++)                                         \
        for (int32_t x = 0; x < c.size.x; x++)                                     \
            set(dst, c.dst.x + x, c.dst.y + y, get(src, c.src.x + x, c.src.y + y))

static void image_blit_to_r8(rc_image dst, rc_image src, image_blit_clip c)
{
    // dst is R8, the narrowest format, so a non-narrowing blit has an R8 source.
    RC_IMAGE_BLIT_LOOP_(rc_image_get_pixel_r8, rc_image_set_pixel_r8);
}

static void image_blit_to_rgb8(rc_image dst, rc_image src, image_blit_clip c)
{
    switch (src.format) {
        case RC_PIXEL_FORMAT_R8:
            RC_IMAGE_BLIT_LOOP_(rc_image_get_pixel_r8, rc_image_set_pixel_rgb8);
            break;
        case RC_PIXEL_FORMAT_RGB8:
            RC_IMAGE_BLIT_LOOP_(rc_image_get_pixel_rgb8, rc_image_set_pixel_rgb8);
            break;
        default:
            break;
    }
}

static void image_blit_to_rgba8(rc_image dst, rc_image src, image_blit_clip c)
{
    switch (src.format) {
        case RC_PIXEL_FORMAT_R8:
            RC_IMAGE_BLIT_LOOP_(rc_image_get_pixel_r8, rc_image_set_pixel_rgba8);
            break;
        case RC_PIXEL_FORMAT_RGB8:
            RC_IMAGE_BLIT_LOOP_(rc_image_get_pixel_rgb8, rc_image_set_pixel_rgba8);
            break;
        case RC_PIXEL_FORMAT_RGBA8:
            RC_IMAGE_BLIT_LOOP_(rc_image_get_pixel_rgba8, rc_image_set_pixel_rgba8);
            break;
        default:
            break;
    }
}

#undef RC_IMAGE_BLIT_LOOP_

bool rc_image_blit(rc_image dst, rc_vec2i dst_pos, rc_image src)
{
    // Cannot narrow a wider source into a narrower destination.
    if (src.format > dst.format)
        return false;
    RC_ASSERT(src.format != RC_PIXEL_FORMAT_NONE && dst.format != RC_PIXEL_FORMAT_NONE);

    image_blit_clip c = image_blit_clip_compute(dst, dst_pos, src);
    if (c.size.x <= 0 || c.size.y <= 0)
        return true;   // fully clipped; a no-op, not an error

    switch (dst.format) {
        case RC_PIXEL_FORMAT_R8:    image_blit_to_r8(dst, src, c);    break;
        case RC_PIXEL_FORMAT_RGB8:  image_blit_to_rgb8(dst, src, c);  break;
        case RC_PIXEL_FORMAT_RGBA8: image_blit_to_rgba8(dst, src, c); break;
        case RC_PIXEL_FORMAT_NONE:
        default:                    break;
    }
    return true;
}
