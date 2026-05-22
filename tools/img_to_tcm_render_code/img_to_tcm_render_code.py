import cv2
import os
import numpy as np

quantize_colors = 64
root_dir = "./"  # image path

IMAGE_EXTENSIONS = ('.jpg', '.jpeg', '.png', '.gif', '.bmp', '.tiff', '.webp')


def color_quantization(image, k=8):
    data = image.reshape((-1, 3)).astype(np.float32)
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 100, 0.1)
    _, labels, palette = cv2.kmeans(data, k, None, criteria, 10, cv2.KMEANS_RANDOM_CENTERS)
    quant_img = palette[labels.flatten()].reshape(image.shape).astype(np.uint8)
    return quant_img


def process(img_path):
    img = cv2.imread(img_path)
    if img is None:
        raise Exception("load failed!")

    img_resize = cv2.resize(img, (160, 96), interpolation=cv2.INTER_AREA)

    h, w = img_resize.shape[:2]
    mid_h, mid_w = h // 2, w // 2

    top_left = img_resize[0:mid_h, 0:mid_w]
    top_right = img_resize[0:mid_h, mid_w:w]
    bottom_left = img_resize[mid_h:h, 0:mid_w]
    bottom_right = img_resize[mid_h:h, mid_w:w]

    merged = cv2.vconcat([top_left, top_right, bottom_left, bottom_right])

    quant_img = color_quantization(merged, k=quantize_colors)

    upper = quant_img[::2]
    lower = quant_img[1::2]
    merged = np.concatenate([upper, lower], axis=-1)

    flattened = merged.reshape(-1, merged.shape[-1])
    colors, cnts = np.unique(flattened, axis=0, return_counts=True)
    colors = colors[np.argsort(-cnts)]

    name = os.path.basename(img_path).replace('.', '_')
    output = open(f"tcm_{name}_code.inc", "w")
    output.write(f'#define _C tcm_syscall_console_set_color\n')
    output.write(f'#define _W tcm_syscall_console_write\n')
    output.write(f'#define _V 0xdfdfdfdf\n')
    output.write(f'void tcm_draw_image_{name}(void) {{\n')
    for color in colors:
        bg = f"0x00{color[5]:02x}{color[4]:02x}{color[3]:02x}"
        fg = f"0x{color[2]:02x}{color[1]:02x}{color[0]:02x}00"
        output.write(f"  _C({bg}, {fg});\n")
        for ln in range(96):
            for col in range(20):
                bitmask = 0
                if np.all(merged[ln][col * 4 + 0] == color):
                    bitmask = (1 << 0) | bitmask
                if np.all(merged[ln][col * 4 + 1] == color):
                    bitmask = (1 << 1) | bitmask
                if np.all(merged[ln][col * 4 + 2] == color):
                    bitmask = (1 << 2) | bitmask
                if np.all(merged[ln][col * 4 + 3] == color):
                    bitmask = (1 << 3) | bitmask
                if bitmask != 0:
                    output.write(f"  _W({hex(bitmask)}0000 | {ln * 20 + col}, _V);\n")
    output.write('}\n\n')
    output.write(f'#undef _C\n')
    output.write(f'#undef _W\n')
    output.write(f'#undef _V\n')
    output.close()


if __name__ == "__main__":
    if os.path.isdir(root_dir):
        for dir_path, dir_names, filenames in os.walk(root_dir):
            for filename in filenames:
                if filename.lower().endswith(IMAGE_EXTENSIONS):
                    img_path = os.path.join(dir_path, filename)
                    print('processing ', img_path)
                    process(img_path)
    if os.path.isfile(root_dir):
        print('processing ', root_dir)
        process(root_dir)
