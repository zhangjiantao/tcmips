#include "dev/keyboard.h"
#include "wl_def.h"

#include <dev/console.h>
#include <dev/syscall.h>

#define FIXME abort

void TimerInit(void) {}

void Quit(const char *error) {
  if (error) {
    printf("\n!!! GAME FATAL QUIT !!!\n Reason: %s\n", error);
  } else {
    printf("\n!!! GAME QUIT TO DOS ATTEMPTED !!!\n");
  }

  exit(error ? 1 : 0);
}

/* Input.  */

void INL_Update() {
  uint32_t k = tcm_syscall_read_keyboard();
  bool press = __TCM_KEYBOARD_PRESS(k);
  uint32_t scancode = __TCM_KEYBOARD_KCODE(k);

  while (scancode != 0) {
    switch (scancode) {
    case 0x20:
      scancode = sc_Space;
      break;
    case __TCM_KEY_CODE_CTRL:
      scancode = sc_Control;
      break;
    case __TCM_KEY_CODE_UP:
      scancode = sc_UpArrow;
      break;
    case __TCM_KEY_CODE_DOWN:
      scancode = sc_DownArrow;
      break;
    case __TCM_KEY_CODE_LEFT:
      scancode = sc_LeftArrow;
      break;
    case __TCM_KEY_CODE_RIGHT:
      scancode = sc_RightArrow;
      break;
    default:
      scancode = 0;
      break;
    }
    if (scancode == 0)
      continue;
    keyboard_handler(scancode, press);

    k = tcm_syscall_read_keyboard();
    press = __TCM_KEYBOARD_PRESS(k);
    scancode = __TCM_KEYBOARD_KCODE(k);
  }

#ifdef INTEGRATOR
  int scancode;
  while (kmi0[1] & 0x10) {
    int press = 1;
    scancode = kmi0[2];
#if 0
	printf ("scancode %x\n", scancode);
#else
    if (scancode == 0xe0) {
      scancode = kmi0[2] | 0x100;
    }
    if ((scancode & 0xff) == 0xf0) {
      press = 0;
      scancode = (scancode & 0x100) | kmi0[2];
    }
    switch (scancode) {
    case 0x29:
      scancode = sc_Space;
      break;
    case 0x14:
      scancode = sc_Control;
      break;
    case 0x175:
      scancode = sc_UpArrow;
      break;
    case 0x172:
      scancode = sc_DownArrow;
      break;
    case 0x16b:
      scancode = sc_LeftArrow;
      break;
    case 0x174:
      scancode = sc_RightArrow;
      break;
    default:
      scancode = 0;
      break;
    }
    if (scancode == 0)
      continue;
    keyboard_handler(scancode, press);
#endif
  }
#endif
#ifdef LUMINARY
  int buttons = 0;
  static int oldbuttons = 0;
  int i;
  static const byte scancodes[5] = {sc_UpArrow, sc_DownArrow, sc_LeftArrow,
                                    sc_RightArrow, sc_Space};

  buttons = read_buttons();
  buttons ^= oldbuttons;
  oldbuttons ^= buttons;
  for (i = 0; i < 5; i++) {
    int mask = 1 << i;
    if (buttons & mask) {
      keyboard_handler(scancodes[i], (oldbuttons & mask) != 0);
    }
  }

#endif
}

/* Graphics bits.  */

byte framebuffer[128 * 64 / 2];

static uint32_t tc_palette16[16];

void VL_Startup() {
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, 128);
  tcm_pixel_console_clear();

  for (int i = 0; i < 256; i++) {
    uint8_t target_16_idx = pal4bit[i] & 0x0F;

    uint8_t r = gamepal[i * 3] << 2;
    uint8_t g = gamepal[i * 3 + 1] << 2;
    uint8_t b = gamepal[i * 3 + 2] << 2;

    tc_palette16[target_16_idx] = (b << 16) | (g << 8) | r;
  }
}

void VW_UpdateScreen() {
  volatile uint32_t *vram = (volatile uint32_t *)TCM_VRAM_PIXEL_ADDR;

  for (int y = 0; y < 64; y++) {
    // 假设沙盒彩色屏幕每行硬件物理宽度是 256 个像素
    // 像素纵向复制 2 次：当前行对应的物理显存第 1 行和第 2 行的起始位置指针
    volatile uint32_t *vram_row1 = &vram[(y * 2) * 256];
    volatile uint32_t *vram_row2 = &vram[(y * 2 + 1) * 256];

    for (int x_byte = 0; x_byte < 64; x_byte++) {
      uint8_t packed_byte = framebuffer[y * 64 + x_byte];

      // 极限移位解包：高 4 位为左像素，低 4 位为右像素（范围都是 0~15）
      uint8_t pixel_left = (packed_byte >> 4) & 0x0F;
      uint8_t pixel_right = packed_byte & 0x0F;

      // 查阅我们刚刚在开机时融合好的、完全对齐 0x00BBGGRR 格式的高速表
      uint32_t rgb_left = tc_palette16[pixel_left];
      uint32_t rgb_right = tc_palette16[pixel_right];

      int p_idx = x_byte * 4;

      // 【2x2 狂暴无脑刷入显存 MMIO】
      // 1. 写入左边的游戏像素（横向连续写 2 格，且上下行同步写，拼出 2x2
      // 硬件放大方块）
      vram_row1[p_idx] = rgb_left;
      vram_row1[p_idx + 1] = rgb_left;
      vram_row2[p_idx] = rgb_left;
      vram_row2[p_idx + 1] = rgb_left;

      // 2. 写入右边的游戏像素（紧接着向右偏移 2 格写入）
      vram_row1[p_idx + 2] = rgb_right;
      vram_row1[p_idx + 3] = rgb_right;
      vram_row2[p_idx + 2] = rgb_right;
      vram_row2[p_idx + 3] = rgb_right;
    }
  }
}

static void sys_init() {}

int main(int argc, char **argv) {
  // #ifndef LUMINARY
  //     vwidth = 128;
  //     vheight = 96;
  // #endif
  sys_init();
  TimerInit();
  return WolfMain(argc, argv);
}
