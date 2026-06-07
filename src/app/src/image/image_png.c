#include "richc/image/image_png.h"

#include "richc/array/u32.h"
#include "richc/file.h"
#include "richc/zip/inflate.h"

// PNG chunk type codes, read as big-endian uint32 of the four type bytes.
enum {
    PNG_IHDR = 0x49484452u,
    PNG_PLTE = 0x504C5445u,
    PNG_IDAT = 0x49444154u,
    PNG_IEND = 0x49454E44u,
    PNG_TRNS = 0x74524E53u
};

// PNG colour types (IHDR byte 9).
enum {
    PNG_COLOR_GRAY       = 0,
    PNG_COLOR_RGB        = 2,
    PNG_COLOR_PALETTE    = 3,
    PNG_COLOR_GRAY_ALPHA = 4,
    PNG_COLOR_RGBA       = 6
};

// PNG per-scanline filter types (the leading byte of each filtered scanline).
enum {
    PNG_FILTER_NONE    = 0,
    PNG_FILTER_SUB     = 1,
    PNG_FILTER_UP      = 2,
    PNG_FILTER_AVERAGE = 3,
    PNG_FILTER_PAETH   = 4
};

static rc_image_png_result png_fail(rc_image_png_error error)
{
    return (rc_image_png_result) {.error = error};
}

// Big-endian uint32 at byte offset (caller guarantees offset + 4 <= data.num).
static uint32_t read_u32_be(rc_view_bytes data, uint32_t offset)
{
    return ((uint32_t)rc_view_bytes_get(data, offset)     << 24)
         | ((uint32_t)rc_view_bytes_get(data, offset + 1) << 16)
         | ((uint32_t)rc_view_bytes_get(data, offset + 2) << 8)
         |  (uint32_t)rc_view_bytes_get(data, offset + 3);
}

// Paeth predictor (PNG spec): returns whichever of a/b/c is closest to a+b-c.
static int png_paeth(int a, int b, int c)
{
    int p  = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc)             return b;
    return c;
}

// Read the x-th sample of width bit_depth (1/2/4/8) from a row.  At 8 bits it is
// a straight byte read; for sub-byte depths the samples are packed MSB-first.
static uint32_t png_read_sample(rc_view_bytes raw, uint32_t data0, uint32_t x, uint32_t bit_depth)
{
    if (bit_depth == 8)
        return rc_view_bytes_get(raw, data0 + x);
    uint32_t bit_index = x * bit_depth;
    uint32_t byte      = rc_view_bytes_get(raw, data0 + bit_index / 8u);
    uint32_t shift     = 8u - bit_depth - (bit_index % 8u);
    uint32_t mask      = (1u << bit_depth) - 1u;
    return (byte >> shift) & mask;
}

// Each png_defilter_* reverses one PNG filter across a single scanline, in
// place.  data0 is the row's first data byte; bpp the filter unit (bytes per
// pixel); row_stride the bytes per row (including the filter byte) for reaching
// the row above.  a = left (bpp bytes back), b = above, c = above-left, all
// already reconstructed when read; out-of-bounds neighbours are taken as zero.

static void png_defilter_sub(rc_span_bytes raw, uint32_t data0, uint32_t width_bytes, uint32_t bpp)
{
    // The first pixel (i < bpp) has a == 0, so recon == x; leave it untouched.
    for (uint32_t i = bpp; i < width_bytes; i++) {
        uint8_t v = rc_span_bytes_get(raw, data0 + i)
                  + rc_span_bytes_get(raw, data0 + i - bpp);
        rc_span_bytes_set(raw, data0 + i, v);
    }
}

static void png_defilter_up(rc_span_bytes raw, uint32_t data0, uint32_t width_bytes, uint32_t row_stride)
{
    // Caller skips this on the first row, where b == 0 and recon == x.
    for (uint32_t i = 0; i < width_bytes; i++) {
        uint8_t v = rc_span_bytes_get(raw, data0 + i)
                  + rc_span_bytes_get(raw, data0 + i - row_stride);
        rc_span_bytes_set(raw, data0 + i, v);
    }
}

static void png_defilter_average(rc_span_bytes raw, uint32_t data0, uint32_t width_bytes,
                                 uint32_t bpp, uint32_t row_stride, bool has_above)
{
    for (uint32_t i = 0; i < width_bytes; i++) {
        uint8_t a = (i >= bpp) ? rc_span_bytes_get(raw, data0 + i - bpp) : 0;
        uint8_t b = has_above  ? rc_span_bytes_get(raw, data0 + i - row_stride) : 0;
        uint8_t v = rc_span_bytes_get(raw, data0 + i) + (a + b) / 2;
        rc_span_bytes_set(raw, data0 + i, v);
    }
}

static void png_defilter_paeth(rc_span_bytes raw, uint32_t data0, uint32_t width_bytes,
                               uint32_t bpp, uint32_t row_stride, bool has_above)
{
    for (uint32_t i = 0; i < width_bytes; i++) {
        uint8_t a = (i >= bpp)              ? rc_span_bytes_get(raw, data0 + i - bpp) : 0;
        uint8_t b = has_above               ? rc_span_bytes_get(raw, data0 + i - row_stride) : 0;
        uint8_t c = (i >= bpp && has_above) ? rc_span_bytes_get(raw, data0 + i - row_stride - bpp) : 0;
        uint8_t v = rc_span_bytes_get(raw, data0 + i) + png_paeth(a, b, c);
        rc_span_bytes_set(raw, data0 + i, v);
    }
}

// Reverse the per-scanline filters in place.  bpp is the filter unit (bytes per
// pixel, rounded up, min 1).  The filter type is constant per row, so it is
// dispatched once per row to the matching helper above.
static rc_image_png_error png_defilter(rc_span_bytes raw, uint32_t width_bytes,
                                       uint32_t height, uint32_t bpp)
{
    uint32_t row_stride = 1u + width_bytes;
    for (uint32_t y = 0; y < height; y++) {
        uint32_t data0  = y * row_stride + 1u;
        uint32_t filter = rc_span_bytes_get(raw, data0 - 1u);
        switch (filter) {
            case PNG_FILTER_NONE:
                break;   // recon == x; nothing to undo
            case PNG_FILTER_SUB:
                png_defilter_sub(raw, data0, width_bytes, bpp);
                break;
            case PNG_FILTER_UP:
                if (y > 0)   // first row: b == 0, row unchanged
                    png_defilter_up(raw, data0, width_bytes, row_stride);
                break;
            case PNG_FILTER_AVERAGE:
                png_defilter_average(raw, data0, width_bytes, bpp, row_stride, y > 0);
                break;
            case PNG_FILTER_PAETH:
                png_defilter_paeth(raw, data0, width_bytes, bpp, row_stride, y > 0);
                break;
            default:
                return RC_IMAGE_PNG_ERROR_BAD_DATA;
        }
    }
    return RC_IMAGE_PNG_OK;
}

// Each png_fill_* decodes one colour type's pixels into img through the packed
// uint32 colour path (R | G<<8 | B<<16 | A<<24); rc_image_set_pixel keeps only
// the channels img's format stores.  data0 is each row's first data byte.

static rc_image_png_error png_fill_gray(rc_image img, rc_view_bytes raw, uint32_t row_stride,
                                        uint32_t width, uint32_t height, uint32_t bit_depth)
{
    // Scale a bit_depth sample up to 8-bit by an exact integer factor
    // (1/2/4/8 bit -> *255 / *85 / *17 / *1).
    uint32_t scale = 255u / ((1u << bit_depth) - 1u);
    for (uint32_t y = 0; y < height; y++) {
        uint32_t data0 = y * row_stride + 1u;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t g = png_read_sample(raw, data0, x, bit_depth) * scale;
            rc_image_set_pixel(img, (int32_t)x, (int32_t)y, g | (g << 8) | (g << 16) | 0xFF000000u);
        }
    }
    return RC_IMAGE_PNG_OK;
}

static rc_image_png_error png_fill_rgb(rc_image img, rc_view_bytes raw, uint32_t row_stride,
                                       uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; y++) {
        uint32_t data0 = y * row_stride + 1u;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t o = data0 + x * 3u;
            uint32_t r = rc_view_bytes_get(raw, o);
            uint32_t g = rc_view_bytes_get(raw, o + 1);
            uint32_t b = rc_view_bytes_get(raw, o + 2);
            rc_image_set_pixel(img, (int32_t)x, (int32_t)y, r | (g << 8) | (b << 16) | 0xFF000000u);
        }
    }
    return RC_IMAGE_PNG_OK;
}

static rc_image_png_error png_fill_palette(rc_image img, rc_view_bytes raw, uint32_t row_stride,
                                           uint32_t width, uint32_t height, uint32_t bit_depth,
                                           rc_view_u32 palette)
{
    for (uint32_t y = 0; y < height; y++) {
        uint32_t data0 = y * row_stride + 1u;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t index = png_read_sample(raw, data0, x, bit_depth);
            if (index >= palette.num)
                return RC_IMAGE_PNG_ERROR_BAD_DATA;
            rc_image_set_pixel(img, (int32_t)x, (int32_t)y, rc_view_u32_get(palette, index));
        }
    }
    return RC_IMAGE_PNG_OK;
}

static rc_image_png_error png_fill_gray_alpha(rc_image img, rc_view_bytes raw, uint32_t row_stride,
                                              uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; y++) {
        uint32_t data0 = y * row_stride + 1u;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t o = data0 + x * 2u;
            uint32_t g = rc_view_bytes_get(raw, o);
            uint32_t a = rc_view_bytes_get(raw, o + 1);
            rc_image_set_pixel(img, (int32_t)x, (int32_t)y, g | (g << 8) | (g << 16) | (a << 24));
        }
    }
    return RC_IMAGE_PNG_OK;
}

static rc_image_png_error png_fill_rgba(rc_image img, rc_view_bytes raw, uint32_t row_stride,
                                        uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; y++) {
        uint32_t data0 = y * row_stride + 1u;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t o = data0 + x * 4u;
            uint32_t r = rc_view_bytes_get(raw, o);
            uint32_t g = rc_view_bytes_get(raw, o + 1);
            uint32_t b = rc_view_bytes_get(raw, o + 2);
            uint32_t a = rc_view_bytes_get(raw, o + 3);
            rc_image_set_pixel(img, (int32_t)x, (int32_t)y, r | (g << 8) | (b << 16) | (a << 24));
        }
    }
    return RC_IMAGE_PNG_OK;
}

// Fill img from the defiltered buffer, dispatching on colour type to the matching
// helper above.  palette holds packed RGBA colours (palette mode only).
static rc_image_png_error png_fill_image(rc_image img, rc_view_bytes raw, uint32_t row_stride,
                                         uint32_t width, uint32_t height,
                                         uint32_t color_type, uint32_t bit_depth,
                                         rc_view_u32 palette)
{
    switch (color_type) {
        case PNG_COLOR_GRAY:       return png_fill_gray(img, raw, row_stride, width, height, bit_depth);
        case PNG_COLOR_RGB:        return png_fill_rgb(img, raw, row_stride, width, height);
        case PNG_COLOR_PALETTE:    return png_fill_palette(img, raw, row_stride, width, height, bit_depth, palette);
        case PNG_COLOR_GRAY_ALPHA: return png_fill_gray_alpha(img, raw, row_stride, width, height);
        case PNG_COLOR_RGBA:       return png_fill_rgba(img, raw, row_stride, width, height);
        default:                   RC_UNREACHABLE();   // color_type validated in IHDR
    }
}

// Channel count for a PNG colour type (caller has validated the type).
static uint32_t png_channels(uint32_t color_type)
{
    switch (color_type) {
        case PNG_COLOR_GRAY:        return 1;
        case PNG_COLOR_RGB:         return 3;
        case PNG_COLOR_PALETTE:     return 1;   // one index byte
        case PNG_COLOR_GRAY_ALPHA:  return 2;
        case PNG_COLOR_RGBA:        return 4;
        default:                    RC_UNREACHABLE();   // color_type validated in IHDR
    }
}

// The richc format a PNG colour type maps to.  Palette gains alpha only when a
// tRNS chunk supplies it; grayscale+alpha has no 2-channel format so becomes
// RGBA8.  Caller has validated the colour type.
static rc_pixel_format png_natural_format(uint32_t color_type, bool have_trns)
{
    switch (color_type) {
        case PNG_COLOR_GRAY:       return RC_PIXEL_FORMAT_R8;
        case PNG_COLOR_RGB:        return RC_PIXEL_FORMAT_RGB8;
        case PNG_COLOR_PALETTE:    return have_trns ? RC_PIXEL_FORMAT_RGBA8 : RC_PIXEL_FORMAT_RGB8;
        case PNG_COLOR_GRAY_ALPHA:
        case PNG_COLOR_RGBA:       return RC_PIXEL_FORMAT_RGBA8;
        default:                   RC_UNREACHABLE();   // color_type validated in IHDR
    }
}

rc_image_png_result rc_image_from_png(rc_view_bytes png, rc_pixel_format pixel_format_hint,
                                      rc_arena *arena, rc_arena scratch)
{
    static const uint8_t signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (png.num < 8)
        return png_fail(RC_IMAGE_PNG_ERROR_NOT_PNG);
    for (uint32_t i = 0; i < 8; i++)
        if (rc_view_bytes_get(png, i) != signature[i])
            return png_fail(RC_IMAGE_PNG_ERROR_NOT_PNG);

    uint32_t width      = 0;
    uint32_t height     = 0;
    uint32_t bit_depth  = 0;
    uint32_t color_type = 0;
    bool     have_ihdr  = false;
    bool     have_trns  = false;
    rc_array_bytes idat    = {0};   // concatenated IDAT data (scratch)
    rc_array_u32   palette = {0};   // packed RGBA colours, one per entry (scratch)

    // ---- chunk walk ----
    uint32_t pos  = 8;
    bool     done = false;
    while (!done) {
        if (pos > png.num - 8u)   // need an 8-byte length+type header
            return png_fail(RC_IMAGE_PNG_ERROR_TRUNCATED);
        uint32_t len      = read_u32_be(png, pos);
        uint32_t type     = read_u32_be(png, pos + 4);
        uint32_t data_off = pos + 8;
        // len is untrusted (read from the file) and may be near UINT32_MAX, so
        // compare against the bytes remaining instead of forming data_off + len.
        uint32_t remaining = png.num - data_off;   // data_off <= png.num here
        if (remaining < 4 || len > remaining - 4)  // need len data bytes + 4 CRC
            return png_fail(RC_IMAGE_PNG_ERROR_TRUNCATED);

        switch (type) {
            case PNG_IHDR: {
                if (have_ihdr || len != 13)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                width            = read_u32_be(png, data_off);
                height           = read_u32_be(png, data_off + 4);
                bit_depth        = rc_view_bytes_get(png, data_off + 8);
                color_type       = rc_view_bytes_get(png, data_off + 9);
                uint32_t comp    = rc_view_bytes_get(png, data_off + 10);
                uint32_t filt    = rc_view_bytes_get(png, data_off + 11);
                uint32_t interl  = rc_view_bytes_get(png, data_off + 12);
                if (width == 0 || height == 0 || comp != 0 || filt != 0)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                if (interl == 1 || bit_depth == 16)
                    return png_fail(RC_IMAGE_PNG_ERROR_UNSUPPORTED);   // Adam7 / 16-bit
                if (interl != 0)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                // valid depths: 1/2/4/8 for grayscale (0) and palette (3); 8 for the rest.
                bool sub8_ok = (color_type == PNG_COLOR_GRAY || color_type == PNG_COLOR_PALETTE);
                bool depth_ok = (bit_depth == 8)
                             || (sub8_ok && (bit_depth == 1 || bit_depth == 2 || bit_depth == 4));
                if (color_type != PNG_COLOR_GRAY && color_type != PNG_COLOR_RGB
                    && color_type != PNG_COLOR_PALETTE && color_type != PNG_COLOR_GRAY_ALPHA
                    && color_type != PNG_COLOR_RGBA)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                if (!depth_ok)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                have_ihdr = true;
                break;
            }
            case PNG_PLTE: {
                if (!have_ihdr)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                if (len == 0 || len % 3 != 0 || len > 256u * 3u)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_DATA);
                uint32_t entries = len / 3u;
                for (uint32_t i = 0; i < entries; i++) {
                    uint32_t r = rc_view_bytes_get(png, data_off + i * 3u);
                    uint32_t g = rc_view_bytes_get(png, data_off + i * 3u + 1);
                    uint32_t b = rc_view_bytes_get(png, data_off + i * 3u + 2);
                    // opaque (alpha 255) until a tRNS chunk overrides it
                    rc_array_u32_push(&palette, r | (g << 8) | (b << 16) | 0xFF000000u, &scratch);
                }
                break;
            }
            case PNG_TRNS: {
                if (!have_ihdr)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                // Only palette transparency is supported; colour-key tRNS is ignored.
                if (color_type == PNG_COLOR_PALETTE) {
                    // The spec requires PLTE before tRNS, so the palette must
                    // already exist; tRNS-before-PLTE is malformed.
                    if (palette.num == 0)
                        return png_fail(RC_IMAGE_PNG_ERROR_BAD_DATA);
                    uint32_t n = len < palette.num ? len : palette.num;
                    for (uint32_t i = 0; i < n; i++) {
                        uint32_t alpha = rc_view_bytes_get(png, data_off + i);
                        uint32_t c     = rc_array_u32_get(&palette, i);
                        rc_array_u32_set(&palette, i, (c & 0x00FFFFFFu) | (alpha << 24));
                    }
                    have_trns = true;
                }
                break;
            }
            case PNG_IDAT: {
                if (!have_ihdr)
                    return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
                rc_array_bytes_append(&idat, rc_view_bytes_get_subview(png, data_off, data_off + len), &scratch);
                break;
            }
            case PNG_IEND:
                done = true;
                break;
            default:
                break;   // skip ancillary / unknown chunks
        }
        pos = data_off + len + 4;
    }

    if (!have_ihdr)
        return png_fail(RC_IMAGE_PNG_ERROR_BAD_HEADER);
    if (idat.num == 0)
        return png_fail(RC_IMAGE_PNG_ERROR_BAD_DATA);
    if (color_type == PNG_COLOR_PALETTE && palette.num == 0)
        return png_fail(RC_IMAGE_PNG_ERROR_BAD_DATA);   // palette image without PLTE

    // ---- sizes ----
    // width and height are untrusted; bound them first so the products below
    // cannot overflow even 64-bit, then compute in 64-bit so an image whose raw
    // size exceeds the uint32 buffer limit is detected and rejected (rather than
    // wrapping).  This is the only place wider-than-32-bit math is needed.
    if (width > INT32_MAX || height > INT32_MAX)
        return png_fail(RC_IMAGE_PNG_ERROR_UNSUPPORTED);
    uint32_t channels        = png_channels(color_type);
    uint32_t bits_per_pixel  = channels * bit_depth;
    uint64_t width_bytes     = ((uint64_t)width * bits_per_pixel + 7u) / 8u;
    uint64_t raw_size        = (uint64_t)height * (1u + width_bytes);
    if (raw_size > UINT32_MAX)
        return png_fail(RC_IMAGE_PNG_ERROR_UNSUPPORTED);

    // ---- decompress IDAT (zlib) ----
    rc_zip_inflate_result inflated = rc_zip_inflate_zlib(idat.view, (uint32_t)raw_size, &scratch);
    if (inflated.error != RC_ZIP_OK || inflated.data.num != (uint32_t)raw_size)
        return png_fail(RC_IMAGE_PNG_ERROR_BAD_DATA);

    // ---- defilter ----
    uint32_t bpp = (bits_per_pixel + 7u) / 8u;   // filter unit, >= 1
    rc_image_png_error filter_err = png_defilter(inflated.data.span, (uint32_t)width_bytes, height, bpp);
    if (filter_err != RC_IMAGE_PNG_OK)
        return png_fail(filter_err);

    // ---- natural format, then widen toward the hint ----
    rc_pixel_format natural = png_natural_format(color_type, have_trns);
    rc_pixel_format out = (pixel_format_hint > natural) ? pixel_format_hint : natural;

    // ---- build the image (arena-owned) and fill it ----
    rc_image img = rc_image_make(rc_vec2i_make((int32_t)width, (int32_t)height), out, arena);
    rc_image_png_error fill_err = png_fill_image(img, inflated.data.view, 1u + (uint32_t)width_bytes,
                                                 width, height, color_type, bit_depth,
                                                 palette.view);
    if (fill_err != RC_IMAGE_PNG_OK)
        return png_fail(fill_err);

    return (rc_image_png_result) {.image = img};
}

rc_image_png_result rc_image_load_png(rc_str filename, rc_pixel_format pixel_format_hint,
                                      rc_arena *arena, rc_arena scratch)
{
    // Read the file into scratch (advancing the local copy), then decode; the
    // by-value scratch snapshot passed on sits after the file bytes, so they stay
    // valid for the decode and only the image survives in arena.
    rc_file_load_binary_result file = rc_file_load_binary(filename, 0, &scratch);
    if (file.error != RC_FILE_OK)
        return png_fail(RC_IMAGE_PNG_ERROR_IO);
    return rc_image_from_png(file.contents.view, pixel_format_hint, arena, scratch);
}
