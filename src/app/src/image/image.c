#include "richc/image/image.h"

rc_image rc_image_make(rc_vec2i size, rc_pixel_format format,
                       const uint8_t *fill_pixel, rc_arena *arena)
{
    RC_ASSERT(format != RC_PIXEL_FORMAT_NONE);
    RC_ASSERT(size.x >= 0 && size.y >= 0);

    uint32_t bpp    = rc_pixel_format_bytes_per_pixel(format);
    uint32_t stride = (uint32_t)size.x * bpp;
    uint32_t total  = (uint32_t)size.y * stride;

    // Size the buffer to exactly `total` zeroed bytes through the array API.
    rc_array_bytes arr = rc_array_bytes_make(total, arena);
    rc_array_bytes_push_n_zero(&arr, total, arena);

    // A non-NULL fill_pixel is a bpp-byte template repeated across every pixel;
    // NULL leaves the already-zeroed bytes in place.  A freshly made image has no
    // row padding (stride == size.x * bpp), so the pixels are contiguous.
    if (fill_pixel) {
        for (uint32_t off = 0; off < total; off += bpp)
            for (uint32_t b = 0; b < bpp; b++)
                rc_span_bytes_set(arr.span, off + b, fill_pixel[b]);
    }

    return (rc_image) {
        .data   = arr.span,
        .size   = size,
        .stride = stride,
        .format = format
    };
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

bool rc_image_blit(rc_image dst, rc_vec2i dst_pos, rc_image src)
{
    // Cannot narrow a wider source into a narrower destination.
    if (src.format > dst.format)
        return false;

    // Clip the source rectangle to dst's bounds, shifting the source origin to
    // account for a negative destination position.
    int32_t sx = 0,         sy = 0;
    int32_t dx = dst_pos.x, dy = dst_pos.y;
    int32_t w  = src.size.x, h = src.size.y;

    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dst.size.x) w = dst.size.x - dx;
    if (dy + h > dst.size.y) h = dst.size.y - dy;

    if (w <= 0 || h <= 0)
        return true;   // fully clipped; a no-op, not an error

    uint32_t src_bpp = rc_pixel_format_bytes_per_pixel(src.format);
    uint32_t dst_bpp = rc_pixel_format_bytes_per_pixel(dst.format);

    for (int32_t y = 0; y < h; y++) {
        uint32_t s_row = (uint32_t)(sy + y) * src.stride + (uint32_t)sx * src_bpp;
        uint32_t d_row = (uint32_t)(dy + y) * dst.stride + (uint32_t)dx * dst_bpp;

        for (int32_t x = 0; x < w; x++) {
            uint32_t s = s_row + (uint32_t)x * src_bpp;
            uint32_t d = d_row + (uint32_t)x * dst_bpp;

            if (src.format == dst.format) {
                for (uint32_t b = 0; b < src_bpp; b++)
                    rc_span_bytes_set(dst.data, d + b, rc_span_bytes_get(src.data, s + b));
            } else if (src.format == RC_PIXEL_FORMAT_R8 && dst.format == RC_PIXEL_FORMAT_RGB8) {
                uint8_t v = rc_span_bytes_get(src.data, s);
                rc_span_bytes_set(dst.data, d,     v);
                rc_span_bytes_set(dst.data, d + 1, v);
                rc_span_bytes_set(dst.data, d + 2, v);
            } else if (src.format == RC_PIXEL_FORMAT_R8 && dst.format == RC_PIXEL_FORMAT_RGBA8) {
                uint8_t v = rc_span_bytes_get(src.data, s);
                rc_span_bytes_set(dst.data, d,     v);
                rc_span_bytes_set(dst.data, d + 1, v);
                rc_span_bytes_set(dst.data, d + 2, v);
                rc_span_bytes_set(dst.data, d + 3, 255);
            } else if (src.format == RC_PIXEL_FORMAT_RGB8 && dst.format == RC_PIXEL_FORMAT_RGBA8) {
                rc_span_bytes_set(dst.data, d,     rc_span_bytes_get(src.data, s));
                rc_span_bytes_set(dst.data, d + 1, rc_span_bytes_get(src.data, s + 1));
                rc_span_bytes_set(dst.data, d + 2, rc_span_bytes_get(src.data, s + 2));
                rc_span_bytes_set(dst.data, d + 3, 255);
            }
        }
    }
    return true;
}
