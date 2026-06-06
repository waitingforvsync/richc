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
 * Type
 * ----
 *   rc_image  { rc_span_bytes data; rc_vec2i size; uint32_t stride; rc_pixel_format format; }
 *
 * Pixel format
 * ------------
 *   rc_pixel_format            - NONE (0, unset), R8, RGB8, RGBA8; the enum value
 *                                is the bytes-per-pixel count.
 *   rc_pixel_format_bytes_per_pixel(fmt)
 *
 * Construction
 * ------------
 *   rc_image_make(size, format, fill_pixel, arena) - allocate pixel bytes from the
 *       arena.  fill_pixel, if non-NULL, is a bytes-per-pixel template repeated
 *       across every pixel; NULL zero-fills.
 *   rc_image_make_subimage(img, region)            - a borrowed view of a
 *       rectangular region of img (clamped to bounds; shares img's stride).  An
 *       empty region yields an invalid image (data = {NULL, 0}).
 *
 * Operations
 * ----------
 *   rc_image_blit(dst, dst_pos, src) - copy src into dst at dst_pos, clipping to
 *       dst's bounds.  Returns false if src's format is wider than dst's (no
 *       narrowing); a fully clipped blit is a no-op that returns true.  Expands
 *       R8 -> RGB8/RGBA8 (grayscale) and RGB8 -> RGBA8 (opaque alpha).
 *   rc_image_get_pixel(img, x, y) / rc_image_set_pixel(img, x, y, color)
 *       - read/write one pixel as a packed uint32_t colour: R in bits 0-7, G in
 *         8-15, B in 16-23, A in 24-31 (i.e. r | g<<8 | b<<16 | a<<24, the
 *         little-endian reading of RGBA8 bytes).  get expands narrower formats
 *         (R8 -> grayscale, missing alpha -> 255); set keeps only the channels the
 *         format stores (R8 -> the low byte).
 */

#ifndef RC_IMAGE_IMAGE_H_
#define RC_IMAGE_IMAGE_H_

#include <stdbool.h>
#include <stdint.h>

#include "richc/bytes.h"
#include "richc/math/box2i.h"

/* Only pointers to rc_arena appear below, so forward-declare it rather than
 * pulling in arena.h (repeated typedef, valid C11+). */
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
    rc_pixel_format format;
} rc_image;

/* ---- construction ---- */

rc_image rc_image_make(rc_vec2i size, rc_pixel_format format,
                       const uint8_t *fill_pixel, rc_arena *arena);

rc_image rc_image_make_subimage(rc_image img, rc_box2i region);

/* ---- operations ---- */

bool rc_image_blit(rc_image dst, rc_vec2i dst_pos, rc_image src);

/* ---- per-pixel access ---- */

/*
 * Read pixel (x, y) as a packed colour: R in bits 0-7, G 8-15, B 16-23, A 24-31.
 * Narrower formats are expanded: R8 replicates the value across R/G/B with A=255;
 * RGB8 reports A=255.  Asserts the coordinates are in range and the format is set.
 */
static inline uint32_t rc_image_get_pixel(rc_image img, int32_t x, int32_t y)
{
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    RC_ASSERT(img.format != RC_PIXEL_FORMAT_NONE);

    uint32_t bpp = rc_pixel_format_bytes_per_pixel(img.format);
    uint32_t idx = (uint32_t)y * img.stride + (uint32_t)x * bpp;

    switch (img.format) {
        case RC_PIXEL_FORMAT_R8: {
            uint32_t v = rc_span_bytes_get(img.data, idx);
            return v | (v << 8) | (v << 16) | 0xFF000000u;
        }
        case RC_PIXEL_FORMAT_RGB8: {
            uint32_t r = rc_span_bytes_get(img.data, idx);
            uint32_t g = rc_span_bytes_get(img.data, idx + 1);
            uint32_t b = rc_span_bytes_get(img.data, idx + 2);
            return r | (g << 8) | (b << 16) | 0xFF000000u;
        }
        case RC_PIXEL_FORMAT_RGBA8: {
            uint32_t r = rc_span_bytes_get(img.data, idx);
            uint32_t g = rc_span_bytes_get(img.data, idx + 1);
            uint32_t b = rc_span_bytes_get(img.data, idx + 2);
            uint32_t a = rc_span_bytes_get(img.data, idx + 3);
            return r | (g << 8) | (b << 16) | (a << 24);
        }
        case RC_PIXEL_FORMAT_NONE:
        default:
            return 0;
    }
}

/*
 * Write pixel (x, y) from a packed colour (same layout as rc_image_get_pixel).
 * Only the channels the format stores are written: R8 keeps the low byte, RGB8
 * drops alpha, RGBA8 keeps all four.  Asserts coordinates in range, format set.
 */
static inline void rc_image_set_pixel(rc_image img, int32_t x, int32_t y, uint32_t color)
{
    RC_ASSERT(x >= 0 && x < img.size.x && y >= 0 && y < img.size.y);
    RC_ASSERT(img.format != RC_PIXEL_FORMAT_NONE);

    uint32_t bpp = rc_pixel_format_bytes_per_pixel(img.format);
    uint32_t idx = (uint32_t)y * img.stride + (uint32_t)x * bpp;

    switch (img.format) {
        case RC_PIXEL_FORMAT_RGBA8:
            rc_span_bytes_set(img.data, idx + 3, (uint8_t)(color >> 24));
            /* fallthrough */
        case RC_PIXEL_FORMAT_RGB8:
            rc_span_bytes_set(img.data, idx + 1, (uint8_t)(color >> 8));
            rc_span_bytes_set(img.data, idx + 2, (uint8_t)(color >> 16));
            /* fallthrough */
        case RC_PIXEL_FORMAT_R8:
            rc_span_bytes_set(img.data, idx, (uint8_t)color);
            break;
        case RC_PIXEL_FORMAT_NONE:
        default:
            break;
    }
}

#endif /* RC_IMAGE_IMAGE_H_ */
