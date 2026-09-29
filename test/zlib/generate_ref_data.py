#!/usr/bin/env python3
"""Generate reference data and cross-language test files for zlib autotest.

All files are produced under test/zlib/assets/:

  assets/zlib_data/level_{N}_{TYPE}.zlib  (N = 0,3,6,9; TYPE = empty,text,repeat)
    Format: [u32 LE: original_len][u32 LE: compressed_len][compressed][original]
    These are Python-compressed streams used by test_inflate_files() in the
    guest to test cross-language inflate robustness.

  assets/raw/{TYPE}.raw (TYPE = empty,text,repeat)
    Raw original bytes used by test_gz_files() to exercise the gz* family
    (gzopen/gzread/gzseek/gzclose) against memfs.

The C test reads both data sets from tcmips_memfs (embedded via -tcm-memfs=assets).

Compression levels 0 (store), 3 (default), 6 (normal), 9 (maximum) are used
to span the full parameter range exposed by zlib.

Usage:
    python test/zlib/generate_ref_data.py
"""

import os
import struct
import zlib

BASE_DIR = os.path.dirname(__file__)
ASSETS_DIR = os.path.join(BASE_DIR, "assets")
ZLIB_DATA_DIR = os.path.join(ASSETS_DIR, "zlib_data")
RAW_DIR = os.path.join(ASSETS_DIR, "raw")

TEXT_DATA = (
    "The quick brown fox jumps over the lazy dog. "
    "Pack my box with five dozen liquor jugs. "
    "How vexingly quick daft zebras jump! "
    "Sphinx of black quartz, judge my vow. "
    "Two driven jocks help fax my big "
    "wax. A mad boxer shoots a quick, jabbed jive "
    "to a bag, a quick bad fox in a box."
).encode()

DATA_SPECS = [
    ("empty", 0, lambda size: b""),
    ("text", len(TEXT_DATA), lambda size: TEXT_DATA),
    ("repeat", 65536, lambda size: b"\xCD" * size),
]

COMPRESSION_LEVELS = [0, 3, 6, 9]


def generate_reference():
    """Print reference values for host-side cross-validation."""
    zlib_ver = zlib.ZLIB_VERSION
    print(f"ref_zlib_version:{zlib_ver}")

    crc_empty = zlib.crc32(b"") & 0xFFFFFFFF
    crc_a = zlib.crc32(b"a") & 0xFFFFFFFF
    crc_abc = zlib.crc32(b"abc") & 0xFFFFFFFF
    print(f"ref_crc32_empty:{crc_empty:08X}")
    print(f"ref_crc32_a:{crc_a:08X}")
    print(f"ref_crc32_abc:{crc_abc:08X}")

    adler_empty = zlib.adler32(b"") & 0xFFFFFFFF
    adler_a = zlib.adler32(b"a") & 0xFFFFFFFF
    adler_abc = zlib.adler32(b"abc") & 0xFFFFFFFF
    print(f"ref_adler32_empty:{adler_empty}")
    print(f"ref_adler32_a:{adler_a:08X}")
    print(f"ref_adler32_abc:{adler_abc:08X}")

    comp_empty = zlib.compress(b"", level=6)
    uncomp_empty = zlib.decompress(comp_empty)
    print(f"ref_compress_empty_len:{len(comp_empty)}")
    print(f"ref_uncompress_empty_len:{len(uncomp_empty)}")

    comp_text = zlib.compress(TEXT_DATA, level=6)
    uncomp_text = zlib.decompress(comp_text)
    print(f"ref_compress_text_len:{len(comp_text)}")
    print(f"ref_uncompress_text_len:{len(uncomp_text)}")
    print(f"ref_text_roundtrip_ok:{uncomp_text == TEXT_DATA}")

    repeat_data = b"\xCD" * 65536
    comp_rep = zlib.compress(repeat_data, level=6)
    uncomp_rep = zlib.decompress(comp_rep)
    print(f"ref_compress_repeat_len:{len(comp_rep)}")
    print(f"ref_compress_repeat_vs_src:{len(comp_rep) < 64}")
    print(f"ref_uncompress_repeat_len:{len(uncomp_rep)}")
    print(f"ref_repeat_roundtrip_ok:{uncomp_rep == repeat_data}")

    for level in COMPRESSION_LEVELS:
        for dtype_name, dtype_size, gen_fn in DATA_SPECS:
            data = gen_fn(dtype_size)
            if len(data) == 0 and dtype_name != "empty":
                continue
            if dtype_name == "empty" and level > 0:
                continue
            comp = zlib.compress(data, level=level)
            uncomp = zlib.decompress(comp)
            if uncomp != data:
                print(f"ref_inflate_level{level}_{dtype_name}:MISMATCH")
                return
            print(f"ref_inflate_level{level}_{dtype_name}:ok({len(data)}->{len(comp)}->{len(uncomp)})")

    import os
    garbage = os.urandom(16)
    try:
        zlib.decompress(garbage)
        print("ref_uncompress_garbage:unexpected_success")
    except zlib.error:
        print("ref_uncompress_garbage_error:zlib.error")


def generate_files():
    """Write all test files under assets/."""
    os.makedirs(ZLIB_DATA_DIR, exist_ok=True)
    os.makedirs(RAW_DIR, exist_ok=True)

    for data_name, size, gen_fn in DATA_SPECS:
        data = gen_fn(size)

        with open(os.path.join(RAW_DIR, f"{data_name}.raw"), "wb") as f:
            f.write(data)

        for level in COMPRESSION_LEVELS:
            compressed = zlib.compress(data, level=level)
            filename = f"level_{level}_{data_name}.zlib"
            filepath = os.path.join(ZLIB_DATA_DIR, filename)
            with open(filepath, "wb") as f:
                f.write(struct.pack("<II", len(data), len(compressed)))
                f.write(compressed)
                f.write(data)


def main():
    generate_reference()
    generate_files()

    print("\nGenerated files under assets/:")
    for root, _, files in os.walk(ASSETS_DIR):
        for f in sorted(files):
            path = os.path.join(root, f)
            rel = os.path.relpath(path, ASSETS_DIR)
            print(f"  {rel}: {os.path.getsize(path)} bytes")


if __name__ == "__main__":
    main()
