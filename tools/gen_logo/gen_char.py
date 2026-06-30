from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np

text = "TCMIPS"
width = 32
height = 32
edge_color = "black"
font_size = 20

char_styles = {
    'T': {"fg": (220, 20, 60), "bg": (140, 248, 215)},
    'C': {"fg": (30, 144, 255), "bg": (215, 215, 208)},
    'M': {"fg": (50, 205, 50), "bg": (126, 126, 126)},
    'I': {"fg": (255, 165, 0), "bg": (244, 144, 149)},
    'P': {"fg": (138, 43, 226), "bg": (155, 128, 125)},
    'S': {"fg": (255, 215, 0), "bg": (72, 61, 139)}
}


def rgb24_to_rgb332(r, g, b):
    r_3 = (r >> 5) & 0x07
    g_3 = (g >> 5) & 0x07
    b_2 = (b >> 6) & 0x03

    return (r_3 << 5) | (g_3 << 2) | b_2


def gen_char_assets():
    font_path = f"{Path(__file__).parent.resolve()}/geforce-bold.ttf"
    try:
        font = ImageFont.truetype(font_path, font_size)
    except IOError:
        font = ImageFont.load_default()

    c_file_content = "// Auto generated RGB332 letter asset file\n"
    c_file_content += f"// Image dimensions: {width}x{height}\n\n"

    for char in text:
        style = char_styles.get(char, {"fg": (0, 0, 0), "bg": (255, 255, 255)})
        current_bg = style["bg"]
        current_fg = style["fg"]

        img = Image.new("RGB", (width, height), current_bg)
        draw = ImageDraw.Draw(img)

        bbox = draw.textbbox((0, 0), char, font=font)
        char_w = bbox[2] - bbox[0]
        char_h = bbox[3] - bbox[1]
        x = (width - char_w) // 2 - bbox[0]
        y = (height - char_h) // 2 - bbox[1]

        offset = 2
        for dx in (-offset, 0, offset):
            for dy in (-offset, 0, offset):
                if dx != 0 or dy != 0:
                    draw.text((x + dx, y + dy), char, fill=edge_color, font=font)

        draw.text((x, y), char, fill=current_fg, font=font)

        bmp_filename = f"./CHAR_128x128_{char}.bmp"
        img.save(bmp_filename)
        print(f"bmp_filename: {bmp_filename}")

        arr = np.array(img)
        r = arr[:, :, 0]
        g = arr[:, :, 1]
        b = arr[:, :, 2]

        rgb332_arr = ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6)
        flat_arr = rgb332_arr.flatten()

        c_file_content += f"// Letter: {char}, Color: FG={current_fg} BG={current_bg}\n"
        c_file_content += f"const unsigned char CHAR_128x128_{char}_RGB332[{flat_arr.size}] = {{\n    "

        for i, val in enumerate(flat_arr):
            c_file_content += f"0x{val:02X}, "
            if (i + 1) % 12 == 0:
                c_file_content += "\n    "

        c_file_content = c_file_content.rstrip(", \n    ") + "\n};\n\n"

    inc_filename = "logo_chars.inc"
    with open(inc_filename, "w", encoding="utf-8") as f:
        f.write(c_file_content)
    print(f"inc_filename: {inc_filename}")


if __name__ == "__main__":
    gen_char_assets()
