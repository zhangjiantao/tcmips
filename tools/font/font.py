import struct

def psf1_extract_flat_c(psf_path: str, out_h: str = "font8x16.inc"):
    with open(psf_path, "rb") as f:
        header = f.read(4)
        if len(header) != 4:
            raise Exception("File too small, not valid PSF1")
        m0, m1, mode, char_h = struct.unpack("<BBBB", header)
        if m0 != 0x36 or m1 != 0x04:
            raise Exception("Not a PSF1 font file")
        char_count = 512 if (mode & 0x01) else 256
        bitmap_size = char_count * char_h
        bitmap_raw = f.read(bitmap_size)

    lines = [f"uint8_t font8x{char_h}[] = {{"]
    all_bytes = [f"0x{b:02X}" for b in bitmap_raw]
    for i in range(0, len(all_bytes), 16):
        chunk = all_bytes[i:i+16]
        lines.append("    " + ", ".join(chunk) + ",")
    lines.append("};")
    source = "\n".join(lines)

    with open(out_h, "w", encoding="utf-8") as f:
        f.write(source)
    print(f"Export complete -> {out_h}")
    print(f"Total chars: {char_count}, bytes per char: {char_h}")

if __name__ == "__main__":
    PSF_FILE = "iso01a-8x16.psf"
    psf1_extract_flat_c(PSF_FILE)