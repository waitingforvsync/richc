/*
 * image/image.h - rc_image, a CPU-side image over arena-backed pixel bytes.
 *
 * An rc_image is a non-owning window onto a block of pixel bytes: the arena that
 * allocated the bytes owns them, the image just describes their layout.  The
 * origin is the top-left corner; pixels are stored left-to-right within a row and
 * rows run top-to-bottom.  Rows are spaced `stride` bytes apart, where
 * `stride >= size.x * bytes_per_pixel`; a stride wider than the row lets a
 * subimage share its parent's stride while viewing a narrower span of columns.
 *
 * Packed colour
 * -------------
 * Pixels are read and written as a packed uint32_t: R in bits 0-7, G in 8-15,
 * B in 16-23, A in 24-31 (r | g<<8 | b<<16 | a<<24, the little-endian reading of
 * RGBA8 bytes in memory).  Reading widens narrower formats (R8 -> opaque
 * grayscale, RGB8 -> alpha 255); writing keeps only the channels the format
 * stores (R8 keeps the low/R byte, RGB8 drops alpha).
 *
 * Type
 * ----
 *   rc_image  { rc_span_bytes data; rc_vec2i size; uint32_t stride; uint32_t format; }
 *
 * Pixel format
 * ------------
 *   rc_pixel_format            - NONE (0, unset), R8, RGB8, RGBA8; the enum value
 *                                is the bytes-per-pixel count.
 *   rc_pixel_format_bytes_per_pixel(fmt)
 *
 * Construction
 * ------------
 *   rc_image_make(size, format, arena)            - allocate pixel bytes from the
 *       arena, cleared to zero.
 *   rc_image_make_filled(size, format, fill, arena) - as make, then set every
 *       pixel to the packed colour `fill`.
 *   rc_image_make_subimage(img, region)           - a borrowed view of a
 *       rectangular region of img (clamped to bounds; shares img's stride).  An
 *       empty region yields a zero-size image whose data span still points at
 *       img's base (valid pointer, num 0).
 *
 * Operations
 * ----------
 *   rc_image_blit(dst, dst_pos, src) - copy src into dst at dst_pos, clipping to
 *       dst's bounds.  Returns false if src's format is wider than dst's (no
 *       narrowing); a fully clipped blit is a no-op that returns true.  Source
 *       pixels are widened/narrowed to dst's format as for get/set above.
 *
 * Per-pixel access
 * ----------------
 *   rc_image_get_pixel(img, x, y)            - read pixel as a packed colour.
 *   rc_image_set_pixel(img, x, y, color)     - write pixel from a packed colour.
 *       The generic forms dispatch on img.format; the _r8 / _rgb8 / _rgba8
 *       variants assume that format (asserted) and skip the dispatch, for hot
 *       loops where the caller already knows the format.
 */

#ifndef RC_IMAGE_IMAGE_H_
#define RC_IMAGE_IMAGE_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/bytes.h"
#include "richc/math/box2i.h"

typedef struct rc_arena rc_arena;

/* ---- pixel format ---- */

typedef enum rc_pixel_format {
    RC_PIXEL_FORMAT_NONE  = 0,   /* unset / invalid */
    RC_PIXEL_FORMAT_R8    = 1,   /* 1 byte/pixel  */
    RC_PIXEL_FORMAT_RGB8  = 3,   /* 3 bytes/pixel */
    RC_PIXEL_FORMAT_RGBA8 = 4,   /* 4 bytes/pixel */
} rc_pixel_format;

/* Bytes per pixel for a format - the enum value is the count (NONE -> 0). */
static inline uint32_t rc_pixel_format_bytes_per_pixel(rc_pixel_format fmt)
{
    return (uint32_t)fmt;
}

/* ---- image ---- */

typedef struct rc_image {
    rc_span_bytes   data;    /* raw pixel bytes (borrowed; the arena owns them) */
    rc_vec2i        size;    /* width (x), height (y) in pixels                 */
    uint32_t        stride;  /* bytes per row (>= size.x * bytes_per_pixel)     */
    uint32_t        format;  /* an rc_pixel_format value                        */
} rc_image;

/* ---- construction ---- */

rc_image rc_image_make(rc_vec2i size, rc_pixel_format format, rc_arena *arena);

rc_image rc_image_make_filled(rc_vec2i size, rc_pixel_format format, uint32_t fill, rc_arena *arena);

rc_image rc_image_make_subimage(rc_image img, rc_box2i region);

/* ---- operations ---- */

bool rc_image_blit(rc_image dst, rc_vec2i dst_pos, rc_image src);

/* ---- per-pixel read ---- */

/* Read pixel (x, y) of an R8 image as opaque grayscale (R=G=B=value, A=255). */
static inline uint32_t rc_image_get_pixel_r8(rc_image img, int32_t x, int32_t y)
{
    RC_ASSERT(img.format == RC_PIXEL_FORMAT_R8);
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    uint32_t v = rc_span_bytes_get(img.data, (uint32_t)y * img.stride + (uint32_t)x);
    return v | (v << 8) | (v << 16) | 0xFF000000u;
}

/* Read pixel (x, y) of an RGB8 image, reporting alpha 255. */
static inline uint32_t rc_image_get_pixel_rgb8(rc_image img, int32_t x, int32_t y)
{
    RC_ASSERT(img.format == RC_PIXEL_FORMAT_RGB8);
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    uint32_t idx = (uint32_t)y * img.stride + (uint32_t)x * 3u;
    uint32_t r = rc_span_bytes_get(img.data, idx);
    uint32_t g = rc_span_bytes_get(img.data, idx + 1);
    uint32_t b = rc_span_bytes_get(img.data, idx + 2);
    return r | (g << 8) | (b << 16) | 0xFF000000u;
}

/* Read pixel (x, y) of an RGBA8 image. */
static inline uint32_t rc_image_get_pixel_rgba8(rc_image img, int32_t x, int32_t y)
{
    RC_ASSERT(img.format == RC_PIXEL_FORMAT_RGBA8);
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    uint32_t idx = (uint32_t)y * img.stride + (uint32_t)x * 4u;
    uint32_t r = rc_span_bytes_get(img.data, idx);
    uint32_t g = rc_span_bytes_get(img.data, idx + 1);
    uint32_t b = rc_span_bytes_get(img.data, idx + 2);
    uint32_t a = rc_span_bytes_get(img.data, idx + 3);
    return r | (g << 8) | (b << 16) | (a << 24);
}

/* Read pixel (x, y) as a packed colour, dispatching on the image's format. */
static inline uint32_t rc_image_get_pixel(rc_image img, int32_t x, int32_t y)
{
    switch (img.format) {
        case RC_PIXEL_FORMAT_R8:    return rc_image_get_pixel_r8(img, x, y);
        case RC_PIXEL_FORMAT_RGB8:  return rc_image_get_pixel_rgb8(img, x, y);
        case RC_PIXEL_FORMAT_RGBA8: return rc_image_get_pixel_rgba8(img, x, y);
        case RC_PIXEL_FORMAT_NONE:
        default:                    RC_ASSERT(false); return 0;
    }
}

/* ---- per-pixel write ---- */

/* Write pixel (x, y) of an R8 image (keeps the low/R byte of color). */
static inline void rc_image_set_pixel_r8(rc_image img, int32_t x, int32_t y, uint32_t color)
{
    RC_ASSERT(img.format == RC_PIXEL_FORMAT_R8);
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    rc_span_bytes_set(img.data, (uint32_t)y * img.stride + (uint32_t)x, (uint8_t)color);
}

/* Write pixel (x, y) of an RGB8 image (drops alpha). */
static inline void rc_image_set_pixel_rgb8(rc_image img, int32_t x, int32_t y, uint32_t color)
{
    RC_ASSERT(img.format == RC_PIXEL_FORMAT_RGB8);
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    uint32_t idx = (uint32_t)y * img.stride + (uint32_t)x * 3u;
    rc_span_bytes_set(img.data, idx,     (uint8_t)color);
    rc_span_bytes_set(img.data, idx + 1, (uint8_t)(color >> 8));
    rc_span_bytes_set(img.data, idx + 2, (uint8_t)(color >> 16));
}

/* Write pixel (x, y) of an RGBA8 image. */
static inline void rc_image_set_pixel_rgba8(rc_image img, int32_t x, int32_t y, uint32_t color)
{
    RC_ASSERT(img.format == RC_PIXEL_FORMAT_RGBA8);
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    uint32_t idx = (uint32_t)y * img.stride + (uint32_t)x * 4u;
    rc_span_bytes_set(img.data, idx,     (uint8_t)color);
    rc_span_bytes_set(img.data, idx + 1, (uint8_t)(color >> 8));
    rc_span_bytes_set(img.data, idx + 2, (uint8_t)(color >> 16));
    rc_span_bytes_set(img.data, idx + 3, (uint8_t)(color >> 24));
}

/* Write pixel (x, y) from a packed colour, dispatching on the image's format. */
static inline void rc_image_set_pixel(rc_image img, int32_t x, int32_t y, uint32_t color)
{
    switch (img.format) {
        case RC_PIXEL_FORMAT_R8:    rc_image_set_pixel_r8(img, x, y, color);    break;
        case RC_PIXEL_FORMAT_RGB8:  rc_image_set_pixel_rgb8(img, x, y, color);  break;
        case RC_PIXEL_FORMAT_RGBA8: rc_image_set_pixel_rgba8(img, x, y, color); break;
        case RC_PIXEL_FORMAT_NONE:
        default:                    RC_ASSERT(false); break;
    }
}

#endif /* RC_IMAGE_IMAGE_H_ */
