from PIL import Image, ImageFont, ImageDraw

WIDTH, HEIGHT = 96, 40
TEXT = "TCMIPS"
FONT_SIZE = 22
LETTER_GAP = 3  # 紧凑间距

# ===================== CP850 灰度字符映射（你指定的） =====================
# CP850 编码对应字符：
# 0x20 = 空格 (纯黑, 0% 亮度)
# 0xB0 = ░ (浅灰, 25% 亮度)
# 0xB1 = ▒ (中灰, 50% 亮度)
# 0xB2 = ▓ (深灰, 75% 亮度)
# 0xDB = █ (纯白, 100% 亮度)
GRAY_MAP = [0x20, 0xB0, 0xB1, 0xB2, 0xDB]

THRESHOLDS = [64, 128, 192, 255]

img = Image.new('L', (WIDTH, HEIGHT), color=0)
draw = ImageDraw.Draw(img)

try:
    font = ImageFont.truetype('geforce-bold.ttf', FONT_SIZE)
except:
    try:
        font = ImageFont.truetype('arialbd.ttf', FONT_SIZE)
    except:
        font = ImageFont.load_default()

char_widths = [font.getbbox(c)[2] - font.getbbox(c)[0] for c in TEXT]
total_text_width = sum(char_widths)
total_spacing = (len(TEXT) - 1) * LETTER_GAP
total_width = total_text_width + total_spacing

available_width = WIDTH - 2
if total_width > available_width:
    LETTER_GAP = max(1, (available_width - total_text_width) // (len(TEXT) - 1))
    total_spacing = (len(TEXT) - 1) * LETTER_GAP
    total_width = total_text_width + total_spacing

full_bbox = font.getbbox(TEXT)
text_h = full_bbox[3] - full_bbox[1]
baseline_offset = full_bbox[1]
y = (HEIGHT - text_h) // 2 - baseline_offset

x = 1 + (available_width - total_width) // 2

for i, char in enumerate(TEXT):
    draw.text((x, y), char, font=font, fill=255)
    x += char_widths[i] + LETTER_GAP

pixels = img.load()
c_array = []

for y_line in range(HEIGHT):
    row = []
    for x_line in range(WIDTH):
        gray = pixels[x_line, y_line]

        if gray < THRESHOLDS[0]:
            val = GRAY_MAP[0]
        elif gray < THRESHOLDS[1]:
            val = GRAY_MAP[1]
        elif gray < THRESHOLDS[2]:
            val = GRAY_MAP[2]
        elif gray < THRESHOLDS[3]:
            val = GRAY_MAP[3]
        else:
            val = GRAY_MAP[4]

        row.append(f"0x{val:02X}")
    c_array.append("    {" + ", ".join(row) + "}")

header = f"""#ifndef TCMIPS_LOGO_ARRAY_H
#define TCMIPS_LOGO_ARRAY_H

#include <stdint.h>

#define LOGO_WIDTH   {WIDTH}
#define LOGO_HEIGHT  {HEIGHT}

const uint8_t TCMipsLogoArray[LOGO_HEIGHT][LOGO_WIDTH] = {{
{',\n'.join(c_array)}
}};

#endif // TCMIPS_LOGO_ARRAY_H
"""

with open("TCMipsLogoArray.inc", "w", encoding="utf-8") as f:
    f.write(header)

