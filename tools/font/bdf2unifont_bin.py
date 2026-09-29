#!/usr/bin/env python3
"""Convert a GNU Unifont BDF file into a binary font bundle asset.

The guest-side console (tcmips/dev/console_unicode.c) loads this bundle through
libmemfs and renders UTF-8 text with mixed cell widths: 8 px for narrow glyphs
(BBX 8 16) and 16 px for wide glyphs (BBX 16 16).

The bundle itself is a self-describing binary layout:

    header (16 bytes)
      +0  uint32 magic      = 0x554E4946 ("UNIF")
      +4  uint32 count      number of glyph entries
      +8  uint32 index_ofs  byte offset from bundle start to index[]
      +12 uint32 bitmap_ofs byte offset from bundle start to bitmap[]

    index[count] (8 bytes each), sorted ascending by codepoint
      +0  uint32 codepoint
      +4  uint16 width      glyph width in pixels (8 or 16)
      +6  uint16 height     glyph height in pixels (16)

    bitmap[count] (each glyph is a fixed 32-byte 16x16 bit row-major raster)
      32 bytes: 16 rows x 2 bytes; for an 8-px-wide glyph the glyph is stored
      left-aligned in the low-order 8 bits of each 16-bit row.

Both the codepoint index and the bitmap area are stored separately so the guest
can locate a glyph by binary search over `index` and then read exactly 32 bytes
from `bitmap`.

The on-disk asset wraps the bundle with a small file header carrying an exact
length and a compression flag; the raw bundle follows directly when
uncompressed, or the zlib deflate stream of it when compressed.

On-disk layout (whole file, all integers little-endian):

    file header (16 bytes)
      +0  uint32 magic             = 0x5A494E55 ("UNIZ" bytes 55 4E 49 5A)
      +4  uint32 original_length   bundle size in bytes
      +8  uint32 compressed_length zlib deflate stream size, or 0 if raw
      +12 uint32 flags             bit 0 = 1 compressed, 0 = raw (uncompressed)
    bundle bytes (from +16)          when flags.bit0 == 0
    zlib deflate stream (from +16)   when flags.bit0 == 1

Usage:
    bdf2unifont_bin.py <input.bdf> <output.bin> [--compress|--raw]

Default is --raw (uncompressed); use --compress to emit a zlib stream instead.
The guest console reads the flag and skips decompression when already raw.
"""

import struct
import sys
import zlib

MAGIC = 0x46494E55  # "UNIF" stored little-endian (bytes 55 4E 49 46 = U N I F)
WRAP_MAGIC = 0x5A494E55  # "UNIZ" stored little-endian (bytes 55 4E 49 5A = U N I Z)
INDEX_ENTRY = struct.Struct("<IHH")
HEADER = struct.Struct("<IIII")
WRAP_HEADER = struct.Struct("<IIII")

FLAG_COMPRESSED = 0x01


def parse_bdf(path):
    """Yield (codepoint, width, height, bitmap_rows) for every glyph."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        cp = None
        width = 16
        height = 16
        in_bitmap = False
        rows = []
        for raw in f:
            line = raw.rstrip("\n").strip()
            if line.startswith("ENCODING "):
                cp = int(line.split()[1])
                in_bitmap = False
            elif line.startswith("BBX "):
                parts = line.split()
                width = int(parts[1])
                height = int(parts[2])
            elif line == "BITMAP":
                in_bitmap = True
                rows = []
            elif line == "ENDCHAR":
                if cp is not None:
                    yield cp, width, height, rows
                cp = None
                in_bitmap = False
            elif in_bitmap and line:
                rows.append(line)


def main():
    if len(sys.argv) < 3:
        print("usage: bdf2unifont_bin.py <input.bdf> <output.bin> [--compress|--raw]")
        return 1

    src, dst = sys.argv[1], sys.argv[2]
    compress = False
    for arg in sys.argv[3:]:
        if arg == "--compress":
            compress = True
        elif arg == "--raw":
            compress = False
        else:
            print(f"warning: unknown argument '{arg}' ignored")
            print("usage: bdf2unifont_bin.py <input.bdf> <output.bin> "
                  "[--compress|--raw]")

    glyphs = []
    for cp, width, height, rows in parse_bdf(src):
        # Normalise every glyph to a 16x16 raster (rows are hex strings, one per
        # scanline; a 16-px row is 4 hex digits, an 8-px row is 2 hex digits).
        raster = []
        for r in rows:
            raster.append(int(r, 16))
        glyphs.append((cp, width, height, raster))

    glyphs.sort(key=lambda g: g[0])

    count = len(glyphs)
    header_size = HEADER.size
    index_size = count * INDEX_ENTRY.size
    bitmap_size = count * 32

    index_ofs = header_size
    bitmap_ofs = header_size + index_size

    out = bytearray()
    out += HEADER.pack(MAGIC, count, index_ofs, bitmap_ofs)

    # codepoint index
    for cp, width, height, _raster in glyphs:
        out += INDEX_ENTRY.pack(cp, width, height)

    # bitmaps: each glyph 16 rows x 2 bytes (16-bit row, left-aligned)
    missing = 0
    for cp, width, height, raster in glyphs:
        buf = bytearray(32)
        n_rows = min(len(raster), 16)
        for r in range(n_rows):
            v = raster[r]  # already left-aligned by BDF BBX width
            buf[r * 2 + 0] = (v >> 8) & 0xFF
            buf[r * 2 + 1] = v & 0xFF
        out += buf
        if n_rows != 16 or len(raster) != height:
            missing += 1

    bundle = bytes(out)
    if compress:
        payload = zlib.compress(bundle, level=9)
        flags = FLAG_COMPRESSED
        comp_len = len(payload)
    else:
        payload = bundle
        flags = 0
        comp_len = 0

    # Filesystem asset: file header (magic + lengths + compression flag), then
    # either the raw bundle or its zlib stream. The guest reads the flag and
    # skips decompression when the bundle is stored uncompressed.
    with open(dst, "wb") as f:
        f.write(WRAP_HEADER.pack(WRAP_MAGIC, len(bundle), comp_len, flags))
        f.write(payload)

    ratio = 100.0 * len(payload) / len(bundle) if bundle else 0
    mode = "compressed" if compress else "uncompressed"
    print(f"written {dst}: {count} glyphs, {len(bundle)} bytes "
          f"({mode}, payload {len(payload)}, {WRAP_HEADER.size}B header, "
          f"{ratio:.1f}%)")
    if missing:
        print(f"note: {missing} glyph(s) with non-16 rows (combining marks etc.)")


if __name__ == "__main__":
    sys.exit(main())
