/*
 * image/image_png.h - decode a PNG into an rc_image.
 *
 * rc_image_from_png decompresses and decodes a PNG held in memory (the IDAT
 * stream is zlib-wrapped DEFLATE, decoded via richc core's rc_zip_inflate_zlib).
 * The decoded pixels are allocated from the supplied arena; a by-value scratch
 * arena holds all transient buffers (concatenated IDAT, inflated scanlines,
 * palette) and is discarded when the call returns.  rc_image_load_png is a thin
 * convenience that reads a file into scratch and decodes it.
 *
 * Supported: colour types grayscale, truecolour, palette, grayscale+alpha, and
 * truecolour+alpha at 8 bits per channel, plus 1/2/4-bit grayscale and palette
 * indices; all five scanline filters; multiple IDAT chunks.  Not supported:
 * 16-bit channels and Adam7 interlace (both report UNSUPPORTED).  Chunk CRC-32s
 * are not validated (the IDAT Adler-32 is checked during inflate).
 *
 * Pixel format
 * ------------
 * pixel_format_hint widens, never narrows: the output format is the wider (by
 * bytes per pixel) of the PNG's natural richc format and the hint.  Pass
 * RC_PIXEL_FORMAT_NONE to keep the PNG's natural format.  The natural format is
 * R8 for grayscale, RGB8 for truecolour, RGBA8 for grayscale+alpha and
 * truecolour+alpha, and RGB8 (or RGBA8 when a tRNS chunk is present) for palette.
 */

#ifndef RC_IMAGE_IMAGE_PNG_H_
#define RC_IMAGE_IMAGE_PNG_H_

#include "richc/arena.h"          // rc_arena passed by value (scratch)
#include "richc/image/image.h"

typedef enum rc_image_png_error {
    RC_IMAGE_PNG_OK = 0,
    RC_IMAGE_PNG_ERROR_NOT_PNG,      // missing or invalid 8-byte signature
    RC_IMAGE_PNG_ERROR_TRUNCATED,    // data ended mid-chunk / too short
    RC_IMAGE_PNG_ERROR_BAD_HEADER,   // invalid IHDR fields or chunk ordering
    RC_IMAGE_PNG_ERROR_UNSUPPORTED,  // 16-bit depth, Adam7 interlace, bad combo
    RC_IMAGE_PNG_ERROR_BAD_DATA,     // malformed chunks, inflate failure, size mismatch, missing PLTE
    RC_IMAGE_PNG_ERROR_IO            // the file could not be read (rc_image_load_png only)
} rc_image_png_error;

typedef struct rc_image_png_result {
    rc_image           image;        // the invalid (all-zero) image on error
    rc_image_png_error error;
} rc_image_png_result;

/*
 * Decode the PNG in png_data.  Pixels are allocated from arena; scratch (by
 * value) holds transient buffers and is released on return.  On error the
 * returned image is the invalid (all-zero) state.
 */
rc_image_png_result rc_image_from_png(rc_view_bytes png_data,
                                      rc_pixel_format pixel_format_hint,
                                      rc_arena *arena, rc_arena scratch);

/*
 * Load a PNG file and decode it (a convenience over rc_image_from_png).  The
 * file bytes are read into scratch, so only the decoded image survives in arena.
 * Returns RC_IMAGE_PNG_ERROR_IO if the file cannot be read.
 */
rc_image_png_result rc_image_load_png(rc_str filename,
                                      rc_pixel_format pixel_format_hint,
                                      rc_arena *arena, rc_arena scratch);

#endif /* RC_IMAGE_IMAGE_PNG_H_ */
