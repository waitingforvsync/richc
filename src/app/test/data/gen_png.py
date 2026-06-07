#!/usr/bin/env python3
# Generate the PNG test fixtures in ./png/ used by test_image_png.c.
# Stdlib only (zlib + struct).  Run once to (re)generate; the build does not.
#
# Pixel patterns are kept tiny and deterministic so the C tests can assert exact
# values.  filtered.png exercises all five scanline filters (one per row), with
# pixel bytes from the formula mirrored in the test.

import os
import struct
import zlib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "png")


def chunk(tag, data):
    out = struct.pack(">I", len(data)) + tag + data
    return out + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png(name, width, height, bit_depth, color_type, idat_raw, plte=None, trns=None):
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    out = bytearray(sig)
    out += chunk(b"IHDR", ihdr)
    if plte is not None:
        out += chunk(b"PLTE", plte)
    if trns is not None:
        out += chunk(b"tRNS", trns)
    out += chunk(b"IDAT", zlib.compress(idat_raw, 9))
    out += chunk(b"IEND", b"")
    with open(os.path.join(OUT, name), "wb") as f:
        f.write(out)


def scanlines(rows):
    # Prefix each row's data bytes with filter type 0 (None).
    raw = bytearray()
    for r in rows:
        raw += b"\x00" + bytes(r)
    return bytes(raw)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def filter_row(orig, prev, ftype, bpp):
    # Forward filter: reconstruction recovers the originals, so a/b/c reference
    # the original bytes of the current / previous row.
    n = len(orig)
    prev = prev if prev is not None else bytes(n)
    out = bytearray()
    for i in range(n):
        x = orig[i]
        a = orig[i - bpp] if i >= bpp else 0
        b = prev[i]
        c = prev[i - bpp] if i >= bpp else 0
        if ftype == 0:
            v = x
        elif ftype == 1:
            v = x - a
        elif ftype == 2:
            v = x - b
        elif ftype == 3:
            v = x - (a + b) // 2
        else:
            v = x - paeth(a, b, c)
        out.append(v & 0xFF)
    return bytes(out)


def main():
    os.makedirs(OUT, exist_ok=True)

    # grayscale 8-bit, 2x2
    write_png("gray8.png", 2, 2, 8, 0,
              scanlines([[0x00, 0x40], [0x80, 0xFF]]))

    # truecolour 8-bit, 2x2
    write_png("rgb8.png", 2, 2, 8, 2, scanlines([
        [10, 20, 30, 40, 50, 60],
        [70, 80, 90, 100, 110, 120],
    ]))

    # truecolour + alpha 8-bit, 2x2
    write_png("rgba8.png", 2, 2, 8, 6, scanlines([
        [11, 22, 33, 44, 55, 66, 77, 88],
        [99, 110, 121, 132, 143, 154, 165, 176],
    ]))

    # grayscale + alpha 8-bit, 2x2
    write_png("graya8.png", 2, 2, 8, 4, scanlines([
        [0x10, 0xFF, 0x20, 0x80],
        [0x30, 0x40, 0x40, 0x00],
    ]))

    # palette 8-bit with tRNS, 2x2; indices [[0,1],[2,3]]
    plte = bytes([255, 0, 0,  0, 255, 0,  0, 0, 255,  255, 255, 0])
    write_png("palette.png", 2, 2, 8, 3,
              scanlines([[0, 1], [2, 3]]), plte=plte, trns=bytes([0x80, 0x40]))

    # palette 8-bit without tRNS (natural RGB8), 2x2
    write_png("palette_opaque.png", 2, 2, 8, 3,
              scanlines([[0, 1], [2, 3]]), plte=plte)

    # 1-bit grayscale, 2x2; row0 pixels [1,0]=0x80, row1 [0,1]=0x40
    write_png("gray1.png", 2, 2, 1, 0,
              scanlines([[0x80], [0x40]]))

    # 4-bit palette, 2x2; row0 indices [0,1]=0x01, row1 [2,3]=0x23
    write_png("pal4.png", 2, 2, 4, 3,
              scanlines([[0x01], [0x23]]), plte=plte)

    # filtered RGB 4x5: one filter type per row, pixels from the shared formula.
    width, height, bpp = 4, 5, 3
    prev = None
    raw = bytearray()
    for y in range(height):
        row = []
        for x in range(width):
            row.append((x * 37 + y * 11) & 0xFF)   # r
            row.append((x * 17 + y * 29) & 0xFF)   # g
            row.append((x * 53 + y * 7) & 0xFF)    # b
        row = bytes(row)
        ftype = [0, 1, 2, 3, 4][y]
        raw += bytes([ftype]) + filter_row(row, prev, ftype, bpp)
        prev = row
    write_png("filtered.png", width, height, 8, 2, bytes(raw))

    print("wrote fixtures to", OUT)


if __name__ == "__main__":
    main()
