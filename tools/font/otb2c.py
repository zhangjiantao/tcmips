import os
import freetype


def otb_to_c_array_freetype(otb_path, output_path, target_width, target_height):
    if not os.path.exists(otb_path):
        return

    try:
        face = freetype.Face(otb_path)
    except Exception:
        return

    if face.num_fixed_sizes == 0:
        return

    face.select_size(0)

    bytes_per_row = (target_width + 7) // 8
    bytes_per_char = bytes_per_row * target_height
    ascii_font_table = [[0] * bytes_per_char for _ in range(256)]

    need_scale = (target_width == 16 and target_height == 32)

    for code in range(256):
        glyph_index = face.get_char_index(code)
        if glyph_index == 0 and code != 32:
            continue

        try:
            face.load_glyph(glyph_index, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_MONOCHROME)
            slot = face.glyph
            bitmap = slot.bitmap

            if bitmap.rows > 0 and bitmap.width > 0:
                pitch = bitmap.pitch
                buffer = bitmap.buffer

                src_h = bitmap.rows
                src_w = bitmap.width

                src_matrix = [[False] * 16 for _ in range(16)]
                top_offset = 16 - slot.bitmap_top - 2
                top_offset = max(0, min(top_offset, 16 - src_h))

                for y in range(src_h):
                    t_y = y + top_offset
                    if t_y >= 16:
                        break
                    for x in range(src_w):
                        byte_idx = x // 8
                        bit_idx = 7 - (x % 8)
                        if byte_idx < pitch:
                            if buffer[y * pitch + byte_idx] & (1 << bit_idx):
                                src_matrix[t_y][x] = True

                char_data = [0] * bytes_per_char

                if need_scale:
                    for dst_y in range(32):
                        src_y = dst_y // 2
                        for dst_x in range(16):
                            src_x = dst_x // 2
                            if src_matrix[src_y][src_x]:
                                d_byte = dst_x // 8
                                d_bit = 7 - (dst_x % 8)
                                char_data[dst_y * 2 + d_byte] |= (1 << d_bit)
                else:
                    for dst_y in range(16):
                        for dst_x in range(8):
                            if src_matrix[dst_y][dst_x]:
                                d_bit = 7 - dst_x
                                char_data[dst_y] |= (1 << d_bit)

                ascii_font_table[code] = char_data
        except Exception:
            pass

    c_code = []
    c_code.append(f"const unsigned char font_{target_width}x{target_height}[256][{bytes_per_char}] = {{")

    for i in range(256):
        bitmap = ascii_font_table[i]
        hex_str = ", ".join(f"0x{val:02X}" for val in bitmap)
        c_code.append(f"    {{ {hex_str} }},")

    c_code.append("};")

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(c_code))


if __name__ == "__main__":
    otb_to_c_array_freetype("Bm437_IBM_VGA_8x16.otb", "font_16x32_ibmvga.inc", target_width=16, target_height=32)
    otb_to_c_array_freetype("Bm437_IBM_VGA_8x16.otb", "font_8x16_ibmvga.inc", target_width=8, target_height=16)
