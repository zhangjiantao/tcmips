import os
import cv2
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np

width = 80
height = 48
quantize_sample_count = 50  # 色彩量化，采样帧数
quantize_color_count = 24  # 色彩量化，颜色数
target_fps = 12  # 目标fps
target_frame_count = 0  # 生成帧数上限，0则全量生成

tcmips_tps = 20000  # 游戏中的tps，每秒执行指令数，近似值即可
tcmips_cost_set_color = 4  # 更新调色板，平均代价
tcmips_cost_write_pixel = 2  # 更新像素，平均代价
tcmips_ticks_limit_per_frame = 1 / target_fps * tcmips_tps  # 重绘一帧ticks上限，超过阈值则当前帧绘制超时，视频卡顿

filename = "caixukun.mp4"

real_time_curve_plotting = False  # 实时重绘工作量曲线


class mp4_to_tcm_render_code(object):
    def __init__(self, filename, width, height, quantize_sample_count, quantize_color_count, target_frame_count,
                 target_fps):
        self.filename = filename
        self.output = open(f"tcm_{os.path.basename(filename).replace('.', '_')}_render_code.inc", "w")
        self.width = width
        self.height = height
        self.quantize_sample_count = quantize_sample_count if quantize_sample_count > target_frame_count else target_frame_count
        self.quantize_colors = quantize_color_count
        self.cap = cv2.VideoCapture(filename)
        self.source_frame_count = int(self.cap.get(cv2.CAP_PROP_FRAME_COUNT))
        self.source_fps = self.cap.get(cv2.CAP_PROP_FPS)
        self.sampling_step = self.source_fps / target_fps
        self.target_frame_count = target_frame_count if target_frame_count > 0 else int(
            self.source_frame_count / self.sampling_step)
        self.target_frame_interval = 1000 // target_fps
        self.global_palette = None
        self.redraw_costs = []
        self.fig = None
        self.redraw_frame_dirty_ax = None
        self.redraw_costs_line = None
        self.redraw_costs_ax = None
        self.redraw_frame_curr_ax = None
        self.init_redraw_graph()

    def generate_global_palette(self):
        print('generating global palette...')
        all_pixels = []
        step = max(1, self.source_frame_count // self.quantize_sample_count)
        for i in range(0, self.source_frame_count, step):
            self.cap.set(cv2.CAP_PROP_POS_FRAMES, i)
            ret, frame = self.cap.read()
            if not ret:
                break
            frame = cv2.resize(frame, (width, height))
            pixels = frame.reshape(-1, 3)
            all_pixels.append(pixels)
        all_pixels = np.vstack(all_pixels)
        all_pixels = np.float32(all_pixels)
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 100, 0.1)
        _, labels, palette = cv2.kmeans(all_pixels, self.quantize_colors, None, criteria, 10, cv2.KMEANS_PP_CENTERS)
        self.global_palette = np.uint8(palette)

    def generate_meta_code(self):
        self.output.write(f'uint32_t tcm_frame_count = {self.target_frame_count};\n')
        self.output.write(f'uint32_t tcm_frame_interval = {self.target_frame_interval};\n\n')
        for i in range(self.target_frame_count):
            self.output.write(f"void tcm_show_frame_{i}(void);\n")
        self.output.write('typedef void (*tcm_show_frame_fn_ty)(void);\n\n')
        self.output.write(f'tcm_show_frame_fn_ty tcm_frame_fn[{self.target_frame_count}] = {{\n')
        for i in range(self.target_frame_count):
            self.output.write(f"  tcm_show_frame_{i},\n")
        self.output.write("};\n\n")
        self.output.write(f'#define _C tcm_syscall_console_set_color\n')
        self.output.write(f'#define _W tcm_syscall_console_write\n')
        self.output.write(f'#define _V 0xdfdfdfdf\n')

    @staticmethod
    def image_reshape(image):
        upper = image[::2]
        lower = image[1::2]
        merged = np.concatenate([upper, lower], axis=-1)
        return merged

    def generate_frame_code(self, source_frame_index, frame_index, dirty, curr_img):
        print(
            f'generating frame from source {source_frame_index}/{self.source_frame_count} -> target {frame_index}/{self.target_frame_count}...',
            end='')
        self.output.write(f'void tcm_show_frame_{frame_index}(void) {{\n')
        # 统计颜色出现频率，优先绘制高频颜色
        flattened = curr_img.reshape(-1, curr_img.shape[-1])
        colors, cnts = np.unique(flattened, axis=0, return_counts=True)
        colors = colors[np.argsort(-cnts)]
        redraw_cost = 0  # 当前帧绘制代价
        for color in colors:
            redraw_code = ""
            for ln in range(self.height // 2):
                for col in range(self.width // 4):
                    bitmask = 0
                    # 与前一帧比较，按需绘制
                    if np.all(curr_img[ln][col * 4 + 0] == color) and not np.all(dirty[ln][col * 4 + 0]):
                        bitmask = (1 << 0) | bitmask
                    if np.all(curr_img[ln][col * 4 + 1] == color) and not np.all(dirty[ln][col * 4 + 1]):
                        bitmask = (1 << 1) | bitmask
                    if np.all(curr_img[ln][col * 4 + 2] == color) and not np.all(dirty[ln][col * 4 + 2]):
                        bitmask = (1 << 2) | bitmask
                    if np.all(curr_img[ln][col * 4 + 3] == color) and not np.all(dirty[ln][col * 4 + 3]):
                        bitmask = (1 << 3) | bitmask
                    if bitmask != 0:
                        redraw_cost += tcmips_cost_write_pixel
                        redraw_code += f"  _W({hex(bitmask)}0000 | {ln * 20 + col}, _V);\n"
            if redraw_code != "":
                # RGB翻译为显存控制器调色板格式
                bg = f"0x00{color[3]:02x}{color[4]:02x}{color[5]:02x}"
                fg = f"0x{color[0]:02x}{color[1]:02x}{color[2]:02x}00"
                self.output.write(f"  _C({bg}, {fg});\n")
                self.output.write(redraw_code)
                redraw_cost += tcmips_cost_set_color
        self.output.write('}\n\n')
        self.redraw_costs.append(redraw_cost)
        print(f'finished, redraw cost {redraw_cost}')

    def init_redraw_graph(self):
        if not real_time_curve_plotting:
            return
        plt.ion()
        self.fig = plt.figure(layout='constrained', figsize=(10, 5))
        gs = gridspec.GridSpec(2, 1, figure=self.fig, height_ratios=[1, 2])
        subfig_1 = self.fig.add_subfigure(gs[1, 0])
        subfig_2 = self.fig.add_subfigure(gs[0, 0])
        self.redraw_costs_ax = subfig_1.add_subplot()
        self.redraw_frame_prev_ax = subfig_2.add_subplot(1, 3, 1)
        self.redraw_frame_prev_ax.set_xticks([])
        self.redraw_frame_prev_ax.set_yticks([])
        self.redraw_frame_dirty_ax = subfig_2.add_subplot(1, 3, 2)
        self.redraw_frame_dirty_ax.set_title('dirty pixel', fontsize=12)
        self.redraw_frame_dirty_ax.set_xticks([])
        self.redraw_frame_dirty_ax.set_yticks([])
        self.redraw_frame_curr_ax = subfig_2.add_subplot(1, 3, 3)
        self.redraw_frame_curr_ax.set_xticks([])
        self.redraw_frame_curr_ax.set_yticks([])
        self.redraw_costs_line, = self.redraw_costs_ax.plot([], [], 'b-', linewidth=.5, label='redraw cost')
        self.redraw_costs_ax.set_title('redraw cost graph', fontsize=12)
        self.redraw_costs_ax.set_xlabel('frame index', fontsize=12)
        self.redraw_costs_ax.set_ylabel('redraw cost', fontsize=12)
        self.redraw_costs_ax.legend()
        self.redraw_costs_ax.grid(True, alpha=0.3)
        self.redraw_costs_ax.set_xlim(0, self.target_frame_count - 1)
        self.redraw_costs_ax.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
        self.redraw_costs_ax.axhline(y=tcmips_ticks_limit_per_frame, color='black', linestyle='--', linewidth=1,
                                     label=f'limit_per_frame_{tcmips_ticks_limit_per_frame}')

    def update_redraw_graph(self, index, dirty, prev, frame):
        if not real_time_curve_plotting:
            return
        x = np.arange(len(self.redraw_costs))
        y = np.array(self.redraw_costs)
        self.redraw_costs_line.set_data(x, y)
        for coll in self.redraw_costs_ax.collections:
            coll.remove()
        self.redraw_costs_ax.fill_between(x, 0, y, color='green', alpha=0.3,
                                          interpolate=True)
        self.redraw_costs_ax.fill_between(x, y, tcmips_ticks_limit_per_frame,
                                          where=(y >= tcmips_ticks_limit_per_frame), color='red', alpha=1,
                                          interpolate=True)
        self.redraw_costs_ax.relim()
        self.redraw_costs_ax.autoscale_view()
        if prev is not None:
            self.redraw_frame_prev_ax.set_title(f'frame {index - 1}', fontsize=12)
            self.redraw_frame_prev_ax.imshow(prev)
        self.redraw_frame_curr_ax.set_title(f'frame {index}', fontsize=12)
        temp = dirty.reshape(24, 80, 2, 3)
        temp = temp.transpose(0, 2, 1, 3)
        result = temp.reshape(48, 80, 3)
        W = [255, 255, 255]  # no change
        R = [255, 0, 0]  # redraw
        condition = np.all(result, axis=-1)  # 转为 (48, 80) 的布尔矩阵
        result = np.where(condition[..., None], W, R).astype(np.uint8)  # 转为脏像素矩阵
        self.redraw_frame_dirty_ax.imshow(result)
        self.redraw_frame_curr_ax.imshow(frame)
        self.fig.canvas.draw()
        self.fig.canvas.flush_events()

    def fini_redraw_graph(self):
        if real_time_curve_plotting:
            plt.ioff()
            plt.show()
        else:
            self.fig = plt.figure(layout='constrained', figsize=(10, 5))
            self.redraw_costs_ax = self.fig.add_subplot()
            self.redraw_costs_ax.set_title('redraw cost graph', fontsize=12)
            self.redraw_costs_ax.set_xlabel('frame index', fontsize=12)
            self.redraw_costs_ax.set_ylabel('redraw cost', fontsize=12)
            self.redraw_costs_ax.legend()
            self.redraw_costs_ax.grid(True, alpha=0.3)
            self.redraw_costs_line, = self.redraw_costs_ax.plot(np.array(list(range(self.target_frame_count))),
                                                                self.redraw_costs, 'b-', linewidth=.5,
                                                                label='redraw cost')
            self.redraw_costs_ax.xaxis.set_major_locator(plt.MaxNLocator(integer=True))
            self.redraw_costs_ax.axhline(y=tcmips_ticks_limit_per_frame, color='black', linestyle='--', linewidth=1,
                                         label=f'limit_per_frame_{tcmips_ticks_limit_per_frame}')
            plt.show()

    def process(self):
        self.generate_meta_code()
        if self.global_palette is None:
            self.generate_global_palette()

        frame_count = 0
        frame_index = 0
        prev_reshaped = None
        prev_rgbimage = None
        self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
        while True:
            ret, frame = self.cap.read()
            if not ret:
                break
            if frame_count >= self.sampling_step * frame_index and frame_index < self.target_frame_count:
                frame = cv2.resize(frame, (width, height))
                data = frame.reshape(-1, 3).astype(np.float32)
                dists = np.sqrt(np.sum((data[:, None] - self.global_palette.astype(np.float32)) ** 2, axis=2))
                best_idx = np.argmin(dists, axis=1)
                frame = self.global_palette[best_idx].reshape(self.height, self.width, 3)
                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                frame_reshaped = self.image_reshape(frame_rgb)
                dirty = frame_reshaped == prev_reshaped
                self.generate_frame_code(frame_count, frame_index, dirty, frame_reshaped)
                prev_reshaped = frame_reshaped
                self.update_redraw_graph(frame_index, dirty, prev_rgbimage, frame_rgb)
                frame_index += 1
                prev_rgbimage = frame_rgb
            frame_count += 1
        self.output.write(f'#undef _C\n')
        self.output.write(f'#undef _W\n')
        self.output.write(f'#undef _V\n')

    def release(self):
        self.output.close()
        self.cap.release()
        self.fini_redraw_graph()


if __name__ == '__main__':
    m = mp4_to_tcm_render_code(filename, width, height,
                               quantize_sample_count, quantize_color_count,
                               target_frame_count, target_fps)
    m.process()
    m.release()
