#include "richc/file.h"
#include "richc/test.h"
#include "richc/zip/inflate.h"

// Inflate tests run with the working directory set to the test binary's
// directory (see test/CMakeLists.txt), so the pre-compressed fixtures under
// data/zip/ (produced by the reference zlib) are reached by a relative path.

RC_TEST_GROUP_DATA(zip_inflate) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(zip_inflate, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(zip_inflate, fix)
{
    rc_arena_deinit(&fix->a);
}

// Load a bundled fixture as raw bytes; fails the test if it cannot be read.
static rc_view_bytes load(rc_arena *a, rc_str path)
{
    rc_file_load_binary_result r = rc_file_load_binary(path, 0, a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    return r.contents.view;
}

/* ---- hand-built streams: exercise each path without a fixture ---- */

RC_TEST_STEP(zip_inflate, stored_block, fix)
{
    // One final stored block: BFINAL=1, BTYPE=00 (byte 0x01), then LEN, ~LEN, data.
    uint8_t stream[] = {0x01, 0x05, 0x00, 0xFA, 0xFF, 'h', 'e', 'l', 'l', 'o'};
    rc_zip_inflate_result r = rc_zip_inflate(rc_view_bytes_make(stream, (uint32_t)sizeof(stream)), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, RC_STR("hello"));
}

RC_TEST_STEP(zip_inflate, stored_bad_nlen, fix)
{
    // NLEN is not the one's-complement of LEN -> corrupt stored block.
    uint8_t stream[] = {0x01, 0x05, 0x00, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o'};
    rc_zip_inflate_result r = rc_zip_inflate(rc_view_bytes_make(stream, (uint32_t)sizeof(stream)), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_ERROR_BAD_DATA);
    RC_CHECK_FALSE(rc_array_bytes_is_valid(&r.data));
}

RC_TEST_STEP(zip_inflate, reserved_block_type, fix)
{
    // BFINAL=1, BTYPE=11 (reserved): low three bits 111 -> byte 0x07.
    uint8_t stream[] = {0x07};
    rc_zip_inflate_result r = rc_zip_inflate(rc_view_bytes_make(stream, (uint32_t)sizeof(stream)), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_ERROR_BAD_DATA);
}

RC_TEST_STEP(zip_inflate, truncated_stored, fix)
{
    // Claims five bytes but only three follow.
    uint8_t stream[] = {0x01, 0x05, 0x00, 0xFA, 0xFF, 'h', 'e', 'l'};
    rc_zip_inflate_result r = rc_zip_inflate(rc_view_bytes_make(stream, (uint32_t)sizeof(stream)), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_ERROR_TRUNCATED);
}

RC_TEST_STEP(zip_inflate, empty_input, fix)
{
    rc_zip_inflate_result r = rc_zip_inflate(rc_view_bytes_make(NULL, 0), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_ERROR_TRUNCATED);
}

/* ---- fixtures: round-trip pre-compressed data against known contents ---- */

RC_TEST_STEP(zip_inflate, raw_dynamic, fix)
{
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/lorem.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/lorem.deflate"));
    rc_zip_inflate_result r = rc_zip_inflate(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, raw_fixed, fix)
{
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/lorem.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/lorem.fixed.deflate"));
    rc_zip_inflate_result r = rc_zip_inflate(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, overlapping_backref, fix)
{
    // Highly repetitive: short distances with runs longer than the distance,
    // i.e. the byte-by-byte overlapping copy path.
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/repeat.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/repeat.deflate"));
    rc_zip_inflate_result r = rc_zip_inflate(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, zlib_wrapped, fix)
{
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/lorem.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/lorem.zlib"));
    rc_zip_inflate_result r = rc_zip_inflate_zlib(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, zlib_empty, fix)
{
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/empty.zlib"));
    rc_zip_inflate_result r = rc_zip_inflate_zlib(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, 0u);
}

RC_TEST_STEP(zip_inflate, zlib_bad_header, fix)
{
    // Corrupt the first header byte so the method/checksum check fails.
    rc_array_bytes bytes = rc_array_bytes_make_copy(load(&fix->a, RC_STR("data/zip/lorem.zlib")), 0, &fix->a);
    rc_array_bytes_set(&bytes, 0, 0x00);
    rc_zip_inflate_result r = rc_zip_inflate_zlib(bytes.view, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_ERROR_BAD_HEADER);
}

RC_TEST_STEP(zip_inflate, zlib_bad_checksum, fix)
{
    // Flip a bit in the trailing Adler-32 so verification fails.
    rc_array_bytes bytes = rc_array_bytes_make_copy(load(&fix->a, RC_STR("data/zip/lorem.zlib")), 0, &fix->a);
    uint32_t last = bytes.num - 1;
    rc_array_bytes_set(&bytes, last, (uint8_t)(rc_array_bytes_get(&bytes, last) ^ 0xFF));
    rc_zip_inflate_result r = rc_zip_inflate_zlib(bytes.view, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_ERROR_CHECKSUM);
}

/* ---- larger inputs and decoder edge cases ---- */

RC_TEST_STEP(zip_inflate, raw_large, fix)
{
    // ~100 KB of varied text: many dynamic blocks and a broad spread of
    // back-reference distances, decoded through the raw path.
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/large.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/large.deflate"));
    rc_zip_inflate_result r = rc_zip_inflate(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, zlib_large, fix)
{
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/large.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/large.zlib"));
    rc_zip_inflate_result r = rc_zip_inflate_zlib(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, full_alphabet, fix)
{
    // Every one of the 256 byte values appears, so the whole literal alphabet is
    // exercised through the Huffman path (the rest become distance-256 matches).
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/alphabet.bin"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/alphabet.zlib"));
    rc_zip_inflate_result r = rc_zip_inflate_zlib(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, multi_stored_blocks, fix)
{
    // 70 KB of incompressible data: larger than the 64 KB stored-block cap, so it
    // decodes as several chained stored blocks across byte boundaries, covering
    // the full 0..255 byte range.
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/random.bin"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/random.zlib"));
    rc_zip_inflate_result r = rc_zip_inflate_zlib(compressed, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(rc_view_bytes_as_str(r.data.view), ==, rc_view_bytes_as_str(expect));
}

RC_TEST_STEP(zip_inflate, minimum_capacity_exact, fix)
{
    // Passing the exact decompressed size pre-sizes the output: it fills without
    // growing past the reservation.
    rc_view_bytes expect     = load(&fix->a, RC_STR("data/zip/lorem.txt"));
    rc_view_bytes compressed = load(&fix->a, RC_STR("data/zip/lorem.deflate"));
    rc_zip_inflate_result r = rc_zip_inflate(compressed, expect.num, &fix->a);
    RC_CHECK_TRUE(r.error == RC_ZIP_OK);
    RC_CHECK(r.data.num, ==, expect.num);
    RC_CHECK(r.data.cap, ==, expect.num);
}
