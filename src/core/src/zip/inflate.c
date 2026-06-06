#include "richc/zip/inflate.h"

/*
 * DEFLATE decoder (RFC 1951) with a zlib wrapper (RFC 1950).
 *
 * The structure follows Mark Adler's puff.c reference decoder: code lengths are
 * turned into a canonical Huffman table held as per-length symbol counts plus a
 * length-sorted symbol list, and decode() walks the input one bit at a time
 * against that table.  The one departure is the output: instead of a fixed 32 KB
 * window this writes into an arena-backed rc_array_bytes that simply keeps
 * growing, so the "sliding window" is just the bytes already produced - a
 * back-reference copies from earlier in the same array.  All output reads/writes
 * go through the rc_array_bytes / rc_view_bytes accessors.
 *
 * Compressed data is untrusted: malformed input yields an rc_zip_error, and
 * reads past the end of the input set an overrun flag that the block loop checks
 * and reports as RC_ZIP_ERROR_TRUNCATED.  RC_ASSERT is reserved for the decoder's
 * own internal invariants.
 */

#define RC_ZIP_MAX_BITS    16    // exclusive bound on code length (codes are 1..15 bits)
#define RC_ZIP_MAX_LCODES  286   // maximum literal/length codes
#define RC_ZIP_MAX_DCODES  30    // maximum distance codes
#define RC_ZIP_FIX_LCODES  288   // fixed literal/length codes (incl. two unused)
#define RC_ZIP_CLCODES     19    // code-length codes (the HCLEN alphabet, symbols 0..18)

/* ---- bit reader ---- */

// Reads the input LSB-first.  bytepos is the next byte to pull into the bit
// accumulator; bitbuf/bitcnt hold the bits not yet consumed.  A read past the
// end of the input feeds zero bits and raises overrun, so decoding stays bounded
// and the caller can convert it to RC_ZIP_ERROR_TRUNCATED.
typedef struct bitstream {
    rc_view_bytes input;
    uint32_t      bytepos;
    uint32_t      bitbuf;
    uint32_t      bitcnt;
    bool          overrun;
} bitstream;

// Consume `need` bits and return them.  DEFLATE delivers integers LSB-first, so
// the bits come out in input order.  need == 0 is valid and common - a length or
// distance code with zero extra bits reads no bits and contributes 0 - and need
// never approaches the type width, so the mask below is always well-defined.
static uint32_t get_bits(bitstream *s, uint32_t need)
{
    uint32_t val = s->bitbuf;
    while (s->bitcnt < need) {
        uint32_t next = 0;
        if (s->bytepos < s->input.num) {
            next = rc_view_bytes_get(s->input, s->bytepos++);
        }
        else {
            s->overrun = true;        // ran out of input; feed zero bits
        }
        val |= next << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = val >> need;
    s->bitcnt -= need;
    return val & (((uint32_t)1 << need) - 1);   // need == 0 -> mask 0 -> 0
}

/* ---- Huffman tables ---- */

// Canonical Huffman table: counts[len] is the number of codes of length len,
// and symbols[] lists the coded symbols sorted by (length, symbol).
typedef struct huffman {
    uint16_t counts[RC_ZIP_MAX_BITS];
    uint16_t symbols[RC_ZIP_FIX_LCODES];
} huffman;

// The result of constructing a Huffman table from a set of code lengths.
typedef enum huffman_status {
    HUFFMAN_COMPLETE,        // the code lengths exactly fill the code space
    HUFFMAN_INCOMPLETE,      // some code space is left unused
    HUFFMAN_OVERSUBSCRIBED   // the code lengths demand more than the code space
} huffman_status;

// Build a canonical Huffman table from the per-symbol code lengths (one entry
// per symbol; lengths.num is the symbol count).
static huffman_status huffman_construct(huffman *h, rc_span_bytes lengths)
{
    for (uint32_t len = 0; len < RC_ZIP_MAX_BITS; len++) {
        h->counts[len] = 0;
    }
    for (uint32_t sym = 0; sym < lengths.num; sym++) {
        h->counts[rc_span_bytes_get(lengths, sym)]++;
    }

    // Kraft inequality: each extra bit doubles the codes available at a length;
    // the codes of that length consume some and the rest carry down.  Needing
    // more codes than are available is over-subscribed; codes left over at the
    // end is incomplete; using exactly all of them is complete.
    uint32_t available = 1;
    for (uint32_t len = 1; len < RC_ZIP_MAX_BITS; len++) {
        available *= 2;
        if (h->counts[len] > available) {
            return HUFFMAN_OVERSUBSCRIBED;
        }
        available -= h->counts[len];
    }

    // Place each symbol into its length's slot, in symbol order.
    uint32_t offsets[RC_ZIP_MAX_BITS];
    offsets[1] = 0;
    for (uint32_t len = 2; len < RC_ZIP_MAX_BITS; len++) {
        offsets[len] = offsets[len - 1] + h->counts[len - 1];
    }
    for (uint32_t sym = 0; sym < lengths.num; sym++) {
        uint8_t length = rc_span_bytes_get(lengths, sym);
        if (length != 0) {
            h->symbols[offsets[length]++] = (uint16_t)sym;
        }
    }
    return available == 0 ? HUFFMAN_COMPLETE : HUFFMAN_INCOMPLETE;
}

// Whether a constructed code is usable for decoding: a complete code always is;
// an incomplete one only in the degenerate case of a single one-bit code (every
// length 0 or 1), matching the reference decoder; an over-subscribed one never.
static bool huffman_usable(huffman_status status, const huffman *h, uint32_t n)
{
    if (status == HUFFMAN_COMPLETE) {
        return true;
    }
    if (status == HUFFMAN_INCOMPLETE) {
        return n == (uint32_t)(h->counts[0] + h->counts[1]);
    }
    return false;
}

// Decode one symbol by walking the table a bit at a time.  Returns the symbol,
// or RC_INDEX_NONE when the bits do not form a valid code.
static uint32_t huffman_decode(bitstream *s, const huffman *h)
{
    uint32_t code  = 0;    // bits read so far, as an integer
    uint32_t first = 0;    // first code of the current length
    uint32_t index = 0;    // index of the first symbol of the current length
    for (uint32_t len = 1; len < RC_ZIP_MAX_BITS; len++) {
        code |= get_bits(s, 1);
        uint32_t count = h->counts[len];
        if (code < first + count) {   // code lies in this length's range (code >= first)
            return h->symbols[index + (code - first)];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return RC_INDEX_NONE;
}

/* ---- length / distance tables (RFC 1951 section 3.2.5) ---- */

static const uint16_t length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16_t dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

/* ---- block decoders ---- */

// Decode the literal/length and distance codes of one compressed block, writing
// literals and back-reference copies into out, until the end-of-block symbol.
static rc_zip_error inflate_codes(bitstream *s, const huffman *lencode,
                                  const huffman *distcode, rc_array_bytes *out, rc_arena *arena)
{
    for (;;) {
        if (s->overrun) {
            return RC_ZIP_ERROR_TRUNCATED;
        }
        uint32_t sym = huffman_decode(s, lencode);
        if (s->overrun) {
            return RC_ZIP_ERROR_TRUNCATED;
        }
        if (sym == RC_INDEX_NONE) {
            return RC_ZIP_ERROR_BAD_DATA;
        }
        if (sym == 256) {
            return RC_ZIP_OK;             // end of block
        }
        if (sym < 256) {
            rc_array_bytes_push(out, (uint8_t)sym, arena);
            continue;
        }

        // A length/distance pair: decode the length, then the distance, then
        // copy `length` bytes from `dist` bytes back in the output.
        sym -= 257;
        if (sym >= 29) {
            return RC_ZIP_ERROR_BAD_DATA; // symbols 286/287 are not valid lengths
        }
        uint32_t length = length_base[sym] + get_bits(s, length_extra[sym]);

        uint32_t dsym = huffman_decode(s, distcode);
        if (s->overrun) {
            return RC_ZIP_ERROR_TRUNCATED;
        }
        if (dsym >= RC_ZIP_MAX_DCODES) {   // also catches RC_INDEX_NONE (an invalid code)
            return RC_ZIP_ERROR_BAD_DATA;
        }
        uint32_t dist = dist_base[dsym] + get_bits(s, dist_extra[dsym]);
        if (dist > out->num) {
            return RC_ZIP_ERROR_BAD_DATA; // refers to before the start of the output
        }

        // Copy byte by byte: the run may overlap (dist < length, e.g. dist 1
        // repeats the last byte), so each copied byte may feed a later one.
        uint32_t from  = out->num - dist;
        uint32_t start = rc_array_bytes_push_n(out, length, arena);
        for (uint32_t i = 0; i < length; i++) {
            rc_array_bytes_set(out, start + i, rc_array_bytes_get(out, from + i));
        }
    }
}

// Copy a stored (uncompressed) block: skip to the byte boundary, read the 16-bit
// length and its one's-complement check, then append that many raw bytes.
static rc_zip_error inflate_stored(bitstream *s, rc_array_bytes *out, rc_arena *arena)
{
    // The unused bits in the accumulator are the high bits of bytes already read,
    // so discarding them aligns the stream to the next whole byte.
    s->bitbuf = 0;
    s->bitcnt = 0;

    if ((uint64_t)s->bytepos + 4 > s->input.num) {
        return RC_ZIP_ERROR_TRUNCATED;
    }
    uint32_t len  = rc_view_bytes_get(s->input, s->bytepos) |
                    ((uint32_t)rc_view_bytes_get(s->input, s->bytepos + 1) << 8);
    uint32_t nlen = rc_view_bytes_get(s->input, s->bytepos + 2) |
                    ((uint32_t)rc_view_bytes_get(s->input, s->bytepos + 3) << 8);
    s->bytepos += 4;
    if ((len ^ 0xFFFFu) != nlen) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    if ((uint64_t)s->bytepos + len > s->input.num) {
        return RC_ZIP_ERROR_TRUNCATED;
    }
    rc_array_bytes_append(out, rc_view_bytes_get_subview(s->input, s->bytepos, s->bytepos + len), arena);
    s->bytepos += len;
    return RC_ZIP_OK;
}

// Build the fixed literal/length and distance Huffman tables (RFC 1951 3.2.6).
static void inflate_build_fixed(huffman *lencode, huffman *distcode)
{
    uint8_t lengths[RC_ZIP_FIX_LCODES];
    uint32_t i = 0;
    for (; i < 144; i++) lengths[i] = 8;
    for (; i < 256; i++) lengths[i] = 9;
    for (; i < 280; i++) lengths[i] = 7;
    for (; i < 288; i++) lengths[i] = 8;
    rc_span_bytes span = RC_SPAN(lengths);
    huffman_construct(lencode, span);

    for (i = 0; i < RC_ZIP_MAX_DCODES; i++) lengths[i] = 5;
    huffman_construct(distcode, rc_span_bytes_get_head(span, RC_ZIP_MAX_DCODES));
}

// The number of times a code-length repeat symbol repeats a value: 16 repeats
// the previous length 3..6 times, 17 a run of zeros 3..10, 18 a run of zeros
// 11..138.  The repeat counts carry, respectively, 2, 3, and 7 extra bits.
static uint32_t repeat_count(bitstream *s, uint32_t sym)
{
    if (sym == 16) {
        return 3 + get_bits(s, 2);
    }
    if (sym == 17) {
        return 3 + get_bits(s, 3);
    }
    RC_ASSERT(sym == 18);
    return 11 + get_bits(s, 7);
}

// Read a dynamic block's code-length, literal/length and distance code lengths
// and build the two Huffman tables (RFC 1951 3.2.7).
static rc_zip_error inflate_build_dynamic(bitstream *s, huffman *lencode, huffman *distcode)
{
    // The code-length code lengths are themselves sent in this permuted order.
    static const uint8_t order[RC_ZIP_CLCODES] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    uint32_t nlen  = get_bits(s, 5) + 257;   // 257..286 literal/length codes
    uint32_t ndist = get_bits(s, 5) + 1;     // 1..30 distance codes
    uint32_t ncode = get_bits(s, 4) + 4;     // 4..19 code-length codes
    if (s->overrun) {
        return RC_ZIP_ERROR_TRUNCATED;       // header ran past the end of the input
    }
    if (nlen > RC_ZIP_MAX_LCODES || ndist > RC_ZIP_MAX_DCODES) {
        return RC_ZIP_ERROR_BAD_DATA;
    }

    // Read the code-length code lengths and build their (complete) Huffman table.
    uint8_t lengths[RC_ZIP_MAX_LCODES + RC_ZIP_MAX_DCODES];
    rc_span_bytes span = RC_SPAN(lengths);
    for (uint32_t i = 0; i < ncode; i++) {
        lengths[order[i]] = (uint8_t)get_bits(s, 3);
    }
    for (uint32_t i = ncode; i < RC_ZIP_CLCODES; i++) {
        lengths[order[i]] = 0;
    }
    huffman clcode;
    if (huffman_construct(&clcode, rc_span_bytes_get_head(span, RC_ZIP_CLCODES)) != HUFFMAN_COMPLETE) {
        return RC_ZIP_ERROR_BAD_DATA;        // the code-length code must be complete
    }

    // Decode the literal/length and distance code lengths into one array, with
    // the run-length repeat codes 16 (copy previous), 17 and 18 (runs of zeros).
    uint32_t index = 0;
    while (index < nlen + ndist) {
        if (s->overrun) {
            return RC_ZIP_ERROR_TRUNCATED;
        }
        uint32_t sym = huffman_decode(s, &clcode);
        if (sym == RC_INDEX_NONE) {
            return RC_ZIP_ERROR_BAD_DATA;
        }
        if (sym < 16) {
            lengths[index++] = (uint8_t)sym;
            continue;
        }
        if (sym == 16 && index == 0) {
            return RC_ZIP_ERROR_BAD_DATA;       // nothing to repeat
        }
        uint8_t  value  = (sym == 16) ? lengths[index - 1] : 0;
        uint32_t repeat = repeat_count(s, sym);
        if (index + repeat > nlen + ndist) {
            return RC_ZIP_ERROR_BAD_DATA;       // run overflows the code-length list
        }
        for (uint32_t r = 0; r < repeat; r++) {
            lengths[index++] = value;
        }
    }
    if (s->overrun) {
        return RC_ZIP_ERROR_TRUNCATED;
    }
    if (lengths[256] == 0) {
        return RC_ZIP_ERROR_BAD_DATA;           // block has no end-of-block code
    }

    // Build the literal/length and distance tables; each must be usable.  The
    // distance code lengths follow the nlen literal/length ones in the buffer.
    if (!huffman_usable(huffman_construct(lencode, rc_span_bytes_get_head(span, nlen)), lencode, nlen)) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    if (!huffman_usable(huffman_construct(distcode, rc_span_bytes_get_subspan(span, nlen, nlen + ndist)), distcode, ndist)) {
        return RC_ZIP_ERROR_BAD_DATA;
    }
    return RC_ZIP_OK;
}

// Decode one block: read its type and dispatch to the matching block decoder.
// The block's final-bit (BFINAL) has already been consumed by the caller.
static rc_zip_error inflate_block(bitstream *s, rc_array_bytes *out, rc_arena *arena)
{
    uint32_t type = get_bits(s, 2);
    if (s->overrun) {
        return RC_ZIP_ERROR_TRUNCATED;
    }
    if (type == 0) {
        return inflate_stored(s, out, arena);
    }
    if (type == 1) {
        huffman lencode, distcode;
        inflate_build_fixed(&lencode, &distcode);
        return inflate_codes(s, &lencode, &distcode, out, arena);
    }
    if (type == 2) {
        huffman lencode, distcode;
        rc_zip_error err = inflate_build_dynamic(s, &lencode, &distcode);
        if (err != RC_ZIP_OK) {
            return err;
        }
        return inflate_codes(s, &lencode, &distcode, out, arena);
    }
    return RC_ZIP_ERROR_BAD_DATA;    // reserved block type 3
}

// Decode a whole raw DEFLATE stream, block by block, into out.  Each block opens
// with a final-block flag; decode non-final blocks in the loop, then the final
// one (whose flag the condition consumed) after it.
static rc_zip_error inflate_raw(rc_view_bytes compressed, rc_array_bytes *out, rc_arena *arena)
{
    bitstream s = {.input = compressed};
    while (!get_bits(&s, 1)) {
        rc_zip_error err = inflate_block(&s, out, arena);
        if (err != RC_ZIP_OK) {
            return err;
        }
    }
    return inflate_block(&s, out, arena);
}

// Adler-32 of the output (RFC 1950): two running sums modulo 65521.
static uint32_t adler32(rc_view_bytes data)
{
    const uint32_t mod = 65521;
    uint32_t a = 1, b = 0;
    for (uint32_t i = 0; i < data.num; i++) {
        a = (a + rc_view_bytes_get(data, i)) % mod;
        b = (b + a) % mod;
    }
    return (b << 16) | a;
}

/* ---- public API ---- */

rc_zip_inflate_result rc_zip_inflate(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena)
{
    uint32_t capacity = minimum_capacity > 256 ? minimum_capacity : 256;
    rc_array_bytes out = rc_array_bytes_make(capacity, arena);
    rc_zip_error err = inflate_raw(compressed, &out, arena);
    if (err != RC_ZIP_OK) {
        rc_array_bytes_deinit(&out, arena);
        return (rc_zip_inflate_result) {.error = err};   // .data zero-filled: the invalid state
    }
    return (rc_zip_inflate_result) {.data = out, .error = RC_ZIP_OK};
}

rc_zip_inflate_result rc_zip_inflate_zlib(rc_view_bytes compressed, uint32_t minimum_capacity, rc_arena *arena)
{
    // A zlib stream is at least a 2-byte header plus a 4-byte Adler-32 trailer.
    if (compressed.num < 6) {
        return (rc_zip_inflate_result) {.error = RC_ZIP_ERROR_BAD_HEADER};
    }
    uint32_t cmf = rc_view_bytes_get(compressed, 0);
    uint32_t flg = rc_view_bytes_get(compressed, 1);
    if ((cmf & 0x0F) != 8 ||              // compression method must be DEFLATE
        ((cmf << 8) | flg) % 31 != 0 ||   // header checksum
        (flg & 0x20) != 0) {              // FDICT: preset dictionary unsupported
        return (rc_zip_inflate_result) {.error = RC_ZIP_ERROR_BAD_HEADER};
    }

    // The DEFLATE data sits between the 2-byte header and the 4-byte trailer;
    // decode it with the raw entry point, then verify the trailing checksum.
    rc_view_bytes body = rc_view_bytes_get_subview(compressed, 2, compressed.num - 4);
    rc_zip_inflate_result result = rc_zip_inflate(body, minimum_capacity, arena);
    if (result.error != RC_ZIP_OK) {
        return result;
    }

    uint32_t want = ((uint32_t)rc_view_bytes_get(compressed, compressed.num - 4) << 24) |
                    ((uint32_t)rc_view_bytes_get(compressed, compressed.num - 3) << 16) |
                    ((uint32_t)rc_view_bytes_get(compressed, compressed.num - 2) << 8)  |
                     (uint32_t)rc_view_bytes_get(compressed, compressed.num - 1);
    if (adler32(result.data.view) != want) {
        rc_array_bytes_deinit(&result.data, arena);
        return (rc_zip_inflate_result) {.error = RC_ZIP_ERROR_CHECKSUM};
    }
    return result;
}
