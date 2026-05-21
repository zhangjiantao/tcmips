from pathlib import Path
from PIL import Image, ImageDraw
from PIL.Image import Resampling

width = 80
height = 48
text = "TCMIPS"
bg_color = "white"
edge_color = "black"
quantize_colors = 32
membase = 0
scale = 8

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

    font_size = 19
    from PIL import ImageFont

    font = ImageFont.truetype(f"{Path(__file__).parent.resolve()}/geforce-bold.ttf", font_size)

    # 整体居中排布
    text_total_w = sum([draw.textlength(c, font=font) for c in text])
    start_x = (width - text_total_w) // 2 - 4
    y = (height - font_size) // 2 - 8

    # 逐个绘制文字：先描黑边(1px)，再填充彩色
    offset = 1
    for idx, char in enumerate(text):
        # 1px 黑色描边
        for dx in (-offset, 0, offset):
            for dy in (-offset, 0, offset):
                if dx != 0 or dy != 0:
                    draw.text((start_x + dx, y + dy), char, fill=edge_color, font=font, antialias=False)
        # 主体彩色文字
        draw.text((start_x, y), char, fill=char_colors[idx], font=font, antialias=False)
        # 移动下个字符x
        start_x += draw.textlength(char, font=font)
        start_x += 2

    img = img.quantize(colors=quantize_colors, method=Image.Quantize.MAXCOVERAGE, kmeans=5)

    img.save(f"./LOGO_80x48_{text}.bmp")
    img_scale(trim_white(img), scale).save(f"./LOGO_{text}_trim.png")

    data = img.get_flattened_data()
    colors = dict()
    pixels = []
    for ln in range(height // 2):
        for col in range(width):
            pixel = (data[(ln * 2 + 0) * 80 + col], data[(ln * 2 + 1) * 80 + col])
            pixels.append(pixel)
            colors[pixel] = colors.get(pixel, 0) + 1
    colors = sorted(colors.items(), key=lambda x: x[1], reverse=True)

    palette = img.getpalette()
    with open("tcm_logo_render_code.inc", "w") as f:
        for citem in colors:
            c = citem[0]
            crgb = palette[c[0] * 3:c[0] * 3 + 3] + palette[c[1] * 3:c[1] * 3 + 3]
            if crgb == (0, 0, 0, 0, 0, 0):
                continue
            bg = f"0x00{crgb[3]:02x}{crgb[4]:02x}{crgb[5]:02x}"
            fg = f"0x{crgb[0]:02x}{crgb[1]:02x}{crgb[2]:02x}00"
            f.write(f"  tcm_syscall_console_set_color({bg}, {fg});\n")
            for i in range(len(pixels) // 4):
                bitmask = 0
                if pixels[i * 4] == c:
                    bitmask = 1
                if pixels[i * 4 + 1] == c:
                    bitmask = (1 << 1) | bitmask
                if pixels[i * 4 + 2] == c:
                    bitmask = (1 << 2) | bitmask
                if pixels[i * 4 + 3] == c:
                    bitmask = (1 << 3) | bitmask
                if bitmask != 0:
                    f.write(f"  tcm_syscall_console_write({hex(bitmask)}0000 | {i + membase}, 0xdfdfdfdf);\n")


if __name__ == "__main__":
    gen_logo()
