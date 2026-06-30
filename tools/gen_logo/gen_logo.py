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
    arr_rgba = np.array(img_rgba)
    arr_rgba[:, :, 3] = 0
    result_array = arr_rgba.view(np.int32).squeeze(-1)
    flat_array = result_array.flatten()
    c_code = f"// size: {width}x{height}\n"
    c_code += f"const unsigned int LOGO_{width}x{height}_{text}_data[{flat_array.size}] = {{\n    "

    for i, val in enumerate(flat_array):
        c_code += f"0x{val:08X}, "
        if (i + 1) % 8 == 0:
            c_code += "\n    "

    c_code += "\n};"

    with open(f"logo.inc", "w") as f:
        f.write(c_code)


if __name__ == "__main__":
    gen_logo()
