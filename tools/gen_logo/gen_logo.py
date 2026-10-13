from pathlib import Path
from PIL import Image, ImageDraw
from PIL.Image import Resampling
import numpy as np

width = 160
height = 120
text = "TCMIPS"
bg_color = "white"
edge_color = "black"
scale = 4

def img_scale(img, s):
    new_width = img.width * s
    new_height = img.height * s
    return img.resize((new_width, new_height), resample=Resampling.NEAREST)

def trim_white(img, thresh=250):
    gray = img.convert("L")
    coords = [
        (x, y)
        for x in range(gray.width)
        for y in range(gray.height)
        if gray.getpixel((x, y)) < thresh
    ]
    if not coords:
        return img
    xs, ys = zip(*coords)
    return img.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))

def gen_logo():
    img = Image.new("RGB", (width, height), bg_color)
    draw = ImageDraw.Draw(img)
    # draw.fontmode = "1"

    char_colors = [
        (220, 20, 60),  # T 猩红
        (30, 144, 255),  # C 深蓝
        (50, 205, 50),  # M 翠绿
        (255, 165, 0),  # I 橙色
        (138, 43, 226),  # P 紫蓝
        (255, 215, 0)  # S 金黄
    ]

    font_size = 40
    from PIL import ImageFont

    font = ImageFont.truetype(f"{Path(__file__).parent.resolve()}/geforce-bold.ttf", font_size)

    # 整体居中排布
    text_total_w = sum([draw.textlength(c, font=font) for c in text])
    start_x = (width - text_total_w) // 2 - 4
    y = (height - font_size) // 2 - 8

    offset = 1
    for idx, char in enumerate(text):
        for dx in (-offset, 0, offset):
            for dy in (-offset, 0, offset):
                if dx != 0 or dy != 0:
                    draw.text((start_x + dx, y + dy), char, fill=edge_color, font=font, antialias=False)
        draw.text((start_x, y), char, fill=char_colors[idx], font=font, antialias=False)
        start_x += draw.textlength(char, font=font)
        start_x += 2

    img.save(f"./LOGO_{width}x{height}_{text}.bmp")
    img_scale(trim_white(img), scale).save(f"./LOGO_{width}x{height}_{text}_trim.png")

    img_rgba = img.convert('RGBA')
    arr_rgba = np.array(img_rgba)          # shape (H, W, 4), channel order R,G,B,A
    arr_rgba[:, :, 3] = 0                  # alpha = 0 (opaque)

    # Raw pixel bytes in VRAM-ready layout: per pixel R,G,B,A, top-down, packed
    # (no per-row padding). tobytes() serialises the array in C order, i.e.
    # row-major R,G,B,A — exactly what the TCMIPS 32bpp pixel console expects,
    # and independent of the host's endianness (unlike the previous .view(np.int32)
    # trick). The firmware loads this at startup via memfs_map() and memcpy()s it
    # straight into VRAM (see tcmips/bsp/start.c).
    flat_bytes = arr_rgba.tobytes()

    with open(f"logo.bin", "wb") as f:
        f.write(flat_bytes)


if __name__ == "__main__":
    gen_logo()
