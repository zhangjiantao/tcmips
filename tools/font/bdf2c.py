import os


def bdf_to_c_array_universal(bdf_path, output_path):
    if not os.path.exists(bdf_path):
        print(f"错误: 找不到文件 {bdf_path}")
        return

    # 第一步：快速扫描文件，自动获取字体宽高 (FONTBOUNDINGBOX)
    font_width, font_height = 8, 16  # 默认值
    with open(bdf_path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            if line.startswith("FONTBOUNDINGBOX"):
                parts = line.split()
                if len(parts) >= 3:
                    font_width = int(parts[1])
                    font_height = int(parts[2])
                    break

    print(f"检测到字体尺寸: {font_width}x{font_height}")

    # 计算每行需要几个字节 (8位宽=1字节，16位宽=2字节)
    bytes_per_row = (font_width + 7) // 8

    # 初始化 256 个 ASCII 槽位
    # 每个字符占据的总字节数 = 每一行的字节数 * 总行数
    bytes_per_char = bytes_per_row * font_height
    ascii_font_table = [[0] * bytes_per_char for _ in range(256)]
    char_names = ["UNKNOWN"] * 256

    # 第二步：解析字模数据
    with open(bdf_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    in_bitmap = False
    current_bitmap_bytes = []
    current_char = ""
    current_encoding = -1

    for line in lines:
        line = line.strip()

        if line.startswith("STARTCHAR"):
            parts = line.split()
            current_char = parts[1] if len(parts) > 1 else ""
            continue

        if line.startswith("ENCODING"):
            try:
                current_encoding = int(line.split()[1])
            except (IndexError, ValueError):
                current_encoding = -1
            continue

        if line.startswith("BITMAP"):
            in_bitmap = True
            current_bitmap_bytes = []
            continue

        if line.startswith("ENDCHAR"):
            in_bitmap = False
            # 校验解析出来的总字节数是否完整
            if 0 <= current_encoding <= 255 and len(current_bitmap_bytes) == bytes_per_char:
                ascii_font_table[current_encoding] = current_bitmap_bytes
                char_names[current_encoding] = current_char
            continue

        if in_bitmap:
            # 核心修改：将十六进制字符串转换为对应的字节列表
            # 8x16 是一行 2 码（如 "FF" ->）
            # 16x32 是一行 4 码（如 "FFFF" ->）
            row_bytes = bytes.fromhex(line)
            current_bitmap_bytes.extend(row_bytes)

    # 第三步：导出 C 语言数组
    c_code = []
    c_code.append(f"const unsigned char font_{font_width}x{font_height}[256][{bytes_per_char}] = {{")

    for i in range(256):
        bitmap = ascii_font_table[i]
        name = char_names[i]
        hex_str = ", ".join(f"0x{val:02X}" for val in bitmap)

        if 32 <= i <= 126:
            char_vis = f"'{chr(i)}'" if chr(i) != '\\' else "'\\\\'"
            c_code.append(f"    {{ {hex_str} }}, // [{i:3d}] {char_vis}")
        else:
            c_code.append(f"    {{ {hex_str} }}, // [{i:3d}] {name}")

    c_code.append("};")

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(c_code))

    print(f"转换成功！已生成标准 256 字符二维数组：font_{font_width}x{font_height}")


if __name__ == "__main__":
    bdf_to_c_array_universal("spleen-16x32.bdf", "font_16x32_spleen.inc")
