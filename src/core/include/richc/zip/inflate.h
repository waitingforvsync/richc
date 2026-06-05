/*
 * zip/inflate.h - DEFLATE decompression (inflate).
 *
 * Decompresses a DEFLATE stream (RFC 1951) or a zlib-wrapped DEFLATE stream
 * (RFC 1950, the form PNG IDAT uses) into a growable byte array.  Only the
 * decompression direction is provided; there is no compressor.
 *
 * Two entry points
 * ----------------
 *   rc_zip_inflate       raw DEFLATE (RFC 1951) - the reusable engine.
 *   rc_zip_inflate_zlib  zlib wrapper: a 2-byte header (and optional preset
 *                        dictionary id) followed by the DEFLATE data and a
 *                        trailing big-endian Adler-32 of the output.  Strips the
 *                        wrapper, delegates to the engine, then verifies the
 *                        checksum.
 *
 * Result and errors
 * -----------------
 * Both return rc_zip_inflate_result { rc_array_bytes data; rc_zip_error error; }.
 * RC_ZIP_OK (0) is success; on any error the returned data is the invalid
 * (empty) state.  Compressed input is untrusted, so a malformed stream returns
 * an error rather than asserting.
 *
 * Capacity
 * --------
 * minimum_capacity is a floor on the output array's capacity (like
 * rc_file_load_binary).  Neither DEFLATE nor the zlib wrapper records the
 * decompressed size, so in general it cannot be known ahead of time and the
 * array grows geometrically; a caller that does know it (PNG knows it exactly:
 * height * (1 + width * bytes_per_pixel)) passes it to avoid reallocation.  Pass
 * 0 when it is unknown.
 *
 * Memory
 * ------
 * The output is allocated from arena, which owns it; the caller scopes its
 * lifetime by arena rather than deinit-ing it (as with the file loaders).
 */

#ifndef RC_ZIP_INFLATE_H_
#define RC_ZIP_INFLATE_H_

#include "richc/bytes.h"

/* ---- error codes ---- */

typedef enum {
    RC_ZIP_OK = 0,
    RC_ZIP_ERROR_BAD_DATA,     // malformed DEFLATE: bad block type, Huffman code, NLEN, or symbol
    RC_ZIP_ERROR_TRUNCATED,    // input ended mid-stream
    RC_ZIP_ERROR_BAD_HEADER,   // zlib: invalid CMF/FLG, or an unsupported preset dictionary
    RC_ZIP_ERROR_CHECKSUM      // zlib: the trailing Adler-32 did not match the output
} rc_zip_error;

/* ---- result type ---- */

typedef struct rc_zip_inflate_result {
    rc_array_bytes data;       // the invalid (empty) state on error
    rc_zip_error   error;
} rc_zip_inflate_result;

/* ---- functions ---- */

rc_zip_inflate_result rc_zip_inflate(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena);
rc_zip_inflate_result rc_zip_inflate_zlib(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena);

#endif /* RC_ZIP_INFLATE_H_ */
