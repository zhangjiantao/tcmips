//
// Created by zhangjiantao on 2026/5/28.
//

#include "dev/keyboard.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_video.h"
#include "stdio.h"
#include "sys/unistd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

static uint8_t my_rgb332_palette[256];
static uint32_t my_rgb888_palette[256];

static uint32_t start_time;

#ifdef Q256_HARD_PALETTE

void DG_Init() {
  start_time = tcm_syscall_get_timestamp();
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 320);
  tcm_pixel_console_clear();
}

void DG_DrawFrame(void) {
  static uint32_t frame = 0;
  frame++;
  if (frame & 1)
    return;
  static uint32_t sfram = 0;
  static uint32_t stime = 0;
  if (!stime || (frame & 255) == 0) {
    stime = tcm_syscall_get_timestamp();
    sfram = 0;
  }
  uint32_t current_time = tcm_syscall_get_timestamp();
  if (current_time == stime)
    current_time++;
  printf("\rFPS %5.2f - %d       ", (double)sfram / (current_time - stime),
         frame);
  fflush(stdout);
  sfram++;

  uint64_t *src = (uint64_t *)DG_ScreenBuffer;
  uint64_t *dst = (uint64_t *)TCM_VRAM_PIXEL_ADDR;
  uint64_t *end_dst = dst + 64000 / 8;

  for (uint64_t *ptr = dst; ptr < end_dst; ptr++, src++) {
    tcm_syscall_palette_set_offset((uint32_t)ptr);

    uint64_t data = *src;
    tcm_syscall_palette_upload(data & 0xffffffff, data >> 32);
  }
}

void DG_SetPalette(unsigned char *palette) {
  uint8_t r, g, b;
  for (int i = 0; i < 256; i += 4) {
    r = ((*palette++) >> 5);
    g = ((*palette++) >> 5);
    b = ((*palette++) >> 6);
    uint8_t rgb0 = (r << 5) | (g << 2) | b;
    r = ((*palette++) >> 5);
    g = ((*palette++) >> 5);
    b = ((*palette++) >> 6);
    uint8_t rgb1 = (r << 5) | (g << 2) | b;
    r = ((*palette++) >> 5);
    g = ((*palette++) >> 5);
    b = ((*palette++) >> 6);
    uint8_t rgb2 = (r << 5) | (g << 2) | b;
    r = ((*palette++) >> 5);
    g = ((*palette++) >> 5);
    b = ((*palette++) >> 6);
    uint8_t rgb3 = (r << 5) | (g << 2) | b;

    uint32_t data = rgb3 << 24 | rgb2 << 16 | rgb1 << 8 | rgb0;
    tcm_syscall_palette_init(i, data);
  }
}

#endif

#ifdef Q256_OBD
void DG_Init() {
  start_time = tcm_syscall_get_timestamp();
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 320);
  tcm_pixel_console_clear();
}

uint8_t pal_m00[256];
uint8_t pal_m10[256];
uint8_t pal_m01[256];
uint8_t pal_m11[256];

void DG_DrawFrame(void) {
  register uint8_t *src = (uint8_t *)DG_ScreenBuffer;
  register uint32_t *dst_words = (uint32_t *)TCM_VRAM_PIXEL_ADDR;

  register uint8_t *end_src = src + 64000;

  while (src < end_src) {
    int is_odd_row = ((src - (uint8_t *)DG_ScreenBuffer) >> 6) & 1;

    uint8_t c0, c1, c2, c3, c4, c5, c6, c7;
    uint8_t c8, c9, c10, c11, c12, c13, c14, c15;
    uint8_t c16, c17, c18, c19, c20, c21, c22, c23;
    uint8_t c24, c25, c26, c27, c28, c29, c30, c31;

    if (!is_odd_row) {
      c0 = pal_m00[*src++];
      c1 = pal_m10[*src++];
      c2 = pal_m00[*src++];
      c3 = pal_m10[*src++];
      c4 = pal_m00[*src++];
      c5 = pal_m10[*src++];
      c6 = pal_m00[*src++];
      c7 = pal_m10[*src++];

      c8 = pal_m00[*src++];
      c9 = pal_m10[*src++];
      c10 = pal_m00[*src++];
      c11 = pal_m10[*src++];
      c12 = pal_m00[*src++];
      c13 = pal_m10[*src++];
      c14 = pal_m00[*src++];
      c15 = pal_m10[*src++];

      c16 = pal_m00[*src++];
      c17 = pal_m10[*src++];
      c18 = pal_m00[*src++];
      c19 = pal_m10[*src++];
      c20 = pal_m00[*src++];
      c21 = pal_m10[*src++];
      c22 = pal_m00[*src++];
      c23 = pal_m10[*src++];

      c24 = pal_m00[*src++];
      c25 = pal_m10[*src++];
      c26 = pal_m00[*src++];
      c27 = pal_m10[*src++];
      c28 = pal_m00[*src++];
      c29 = pal_m10[*src++];
      c30 = pal_m00[*src++];
      c31 = pal_m10[*src++];
    } else {
      c0 = pal_m01[*src++];
      c1 = pal_m11[*src++];
      c2 = pal_m01[*src++];
      c3 = pal_m11[*src++];
      c4 = pal_m01[*src++];
      c5 = pal_m11[*src++];
      c6 = pal_m01[*src++];
      c7 = pal_m11[*src++];

      c8 = pal_m01[*src++];
      c9 = pal_m11[*src++];
      c10 = pal_m01[*src++];
      c11 = pal_m11[*src++];
      c12 = pal_m01[*src++];
      c13 = pal_m11[*src++];
      c14 = pal_m01[*src++];
      c15 = pal_m11[*src++];

      c16 = pal_m01[*src++];
      c17 = pal_m11[*src++];
      c18 = pal_m01[*src++];
      c19 = pal_m11[*src++];
      c20 = pal_m01[*src++];
      c21 = pal_m11[*src++];
      c22 = pal_m01[*src++];
      c23 = pal_m11[*src++];

      c24 = pal_m01[*src++];
      c25 = pal_m11[*src++];
      c26 = pal_m01[*src++];
      c27 = pal_m11[*src++];
      c28 = pal_m01[*src++];
      c29 = pal_m11[*src++];
      c30 = pal_m01[*src++];
      c31 = pal_m11[*src++];
    }

    uint32_t w0 = (c3 << 24) | (c2 << 16) | (c1 << 8) | c0;
    uint32_t w1 = (c7 << 24) | (c6 << 16) | (c5 << 8) | c4;
    uint32_t w2 = (c11 << 24) | (c10 << 16) | (c9 << 8) | c8;
    uint32_t w3 = (c15 << 24) | (c14 << 16) | (c13 << 8) | c12;
    uint32_t w4 = (c19 << 24) | (c18 << 16) | (c17 << 8) | c16;
    uint32_t w5 = (c23 << 24) | (c22 << 16) | (c21 << 8) | c20;
    uint32_t w6 = (c27 << 24) | (c26 << 16) | (c25 << 8) | c24;
    uint32_t w7 = (c31 << 24) | (c30 << 16) | (c29 << 8) | c28;

    *dst_words++ = w0;
    *dst_words++ = w1;
    *dst_words++ = w2;
    *dst_words++ = w3;
    *dst_words++ = w4;
    *dst_words++ = w5;
    *dst_words++ = w6;
    *dst_words++ = w7;
  }
}

void DG_SetPalette(unsigned char *palette) {
  for (int i = 0; i < 256; i++) {
    uint8_t r = *palette++;
    uint8_t g = *palette++;
    uint8_t b = *palette++;

#define QUANTIZE_DITHER(offset_rg, offset_b)                                   \
  ({                                                                           \
    int r_d = r + offset_rg;                                                   \
    if (r_d > 255)                                                             \
      r_d = 255;                                                               \
    int g_d = g + offset_rg;                                                   \
    if (g_d > 255)                                                             \
      g_d = 255;                                                               \
    int b_d = b + offset_b;                                                    \
    if (b_d > 255)                                                             \
      b_d = 255;                                                               \
    ((r_d >> 5) << 5) | ((g_d >> 5) << 2) | (b_d >> 6);                        \
  })

    pal_m00[i] = QUANTIZE_DITHER(0, 0);
    pal_m10[i] = QUANTIZE_DITHER(16, 32);
    pal_m01[i] = QUANTIZE_DITHER(24, 48);
    pal_m11[i] = QUANTIZE_DITHER(8, 16);
  }
}

#endif

#ifdef Q256
void DG_Init() {
  start_time = tcm_syscall_get_timestamp();
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 320);
  tcm_pixel_console_clear();
}

void DG_DrawFrame(void) {
  register const uint8_t *const pal = my_rgb332_palette;

  register uint8_t *src = (uint8_t *)DG_ScreenBuffer;
  register uint32_t *dst = (uint32_t *)TCM_VRAM_PIXEL_ADDR;

  register uint8_t *end_src = src + 64000;

  while (src < end_src) {
    uint8_t c0 = pal[*src++];
    uint8_t c1 = pal[*src++];
    uint8_t c2 = pal[*src++];
    uint8_t c3 = pal[*src++];
    uint32_t w0 = (c3 << 24) | (c2 << 16) | (c1 << 8) | c0;

    uint8_t c4 = pal[*src++];
    uint8_t c5 = pal[*src++];
    uint8_t c6 = pal[*src++];
    uint8_t c7 = pal[*src++];
    uint32_t w1 = (c7 << 24) | (c6 << 16) | (c5 << 8) | c4;

    uint8_t c8 = pal[*src++];
    uint8_t c9 = pal[*src++];
    uint8_t c10 = pal[*src++];
    uint8_t c11 = pal[*src++];
    uint32_t w2 = (c11 << 24) | (c10 << 16) | (c9 << 8) | c8;

    uint8_t c12 = pal[*src++];
    uint8_t c13 = pal[*src++];
    uint8_t c14 = pal[*src++];
    uint8_t c15 = pal[*src++];
    uint32_t w3 = (c15 << 24) | (c14 << 16) | (c13 << 8) | c12;

    *dst++ = w0;
    *dst++ = w1;
    *dst++ = w2;
    *dst++ = w3;
  }
}

void DG_SetPalette(unsigned char *palette) {
  for (int i = 0; i < 256; i++) {
    uint8_t r = *palette++;
    uint8_t g = *palette++;
    uint8_t b = *palette++;

    uint8_t r_3bit = (r >> 5) & 0x07;
    uint8_t g_3bit = (g >> 5) & 0x07;
    uint8_t b_2bit = (b >> 6) & 0x03;
    uint8_t rgb332 = (r_3bit << 5) | (g_3bit << 2) | b_2bit;
    my_rgb332_palette[i] = rgb332;
  }
}
#endif

#if !defined(Q256) && !defined(Q256_OBD) && !defined(Q256_HARD_PALETTE)
void DG_Init() {
  start_time = tcm_syscall_get_timestamp();
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_PIXEL_32);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_DATA_OFFSET, TCM_VRAM_PIXEL_ADDR);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_RESOLUTION, 79);
}

void DG_DrawFrame() {
  uint8_t *doom_pixels = (uint8_t *)DG_ScreenBuffer;
  uint32_t *dst_words = (uint32_t *)TCM_VRAM_PIXEL_ADDR;

  for (int i = 0; i < 64000 * 4; i += 16) {
    uint32_t c0 = my_rgb888_palette[doom_pixels[i + 0x0]];
    uint32_t c1 = my_rgb888_palette[doom_pixels[i + 0x1]];
    uint32_t c2 = my_rgb888_palette[doom_pixels[i + 0x2]];
    uint32_t c3 = my_rgb888_palette[doom_pixels[i + 0x3]];
    uint32_t c4 = my_rgb888_palette[doom_pixels[i + 0x4]];
    uint32_t c5 = my_rgb888_palette[doom_pixels[i + 0x5]];
    uint32_t c6 = my_rgb888_palette[doom_pixels[i + 0x6]];
    uint32_t c7 = my_rgb888_palette[doom_pixels[i + 0x7]];
    uint32_t c8 = my_rgb888_palette[doom_pixels[i + 0x8]];
    uint32_t c9 = my_rgb888_palette[doom_pixels[i + 0x9]];
    uint32_t ca = my_rgb888_palette[doom_pixels[i + 0xa]];
    uint32_t cb = my_rgb888_palette[doom_pixels[i + 0xb]];
    uint32_t cc = my_rgb888_palette[doom_pixels[i + 0xc]];
    uint32_t cd = my_rgb888_palette[doom_pixels[i + 0xd]];
    uint32_t ce = my_rgb888_palette[doom_pixels[i + 0xe]];
    uint32_t cf = my_rgb888_palette[doom_pixels[i + 0xf]];

    *dst_words++ = c0;
    *dst_words++ = c1;
    *dst_words++ = c2;
    *dst_words++ = c3;
    *dst_words++ = c4;
    *dst_words++ = c5;
    *dst_words++ = c6;
    *dst_words++ = c7;
    *dst_words++ = c8;
    *dst_words++ = c9;
    *dst_words++ = ca;
    *dst_words++ = cb;
    *dst_words++ = cc;
    *dst_words++ = cd;
    *dst_words++ = ce;
    *dst_words++ = cf;
  }
}

void DG_SetPalette(unsigned char *palette) {
  for (int i = 0; i < 256; i++) {
    uint8_t r = *palette++;
    uint8_t g = *palette++;
    uint8_t b = *palette++;

    uint32_t rgb = (b << 16) | (g << 8) | r;
    my_rgb888_palette[i] = rgb;
  }
}

#endif

void DG_SleepMs(uint32_t ms) {
  // uint32_t t = tcm_syscall_get_timestamp_milli();
  // while (tcm_syscall_get_timestamp_milli() - t < ms)
  //   ;
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
  uint32_t k = tcm_syscall_read_keyboard();
  if (k) {
    *pressed = __TCM_KEYBOARD_PRESS(k);

    uint8_t kcode = __TCM_KEYBOARD_KCODE(k);
    switch (kcode) {
    case __TCM_KEY_CODE_UP:
      *doomKey = KEY_UPARROW;
      break;
    case __TCM_KEY_CODE_RIGHT:
      *doomKey = KEY_RIGHTARROW;
      break;
    case __TCM_KEY_CODE_DOWN:
      *doomKey = KEY_DOWNARROW;
      break;
    case __TCM_KEY_CODE_LEFT:
      *doomKey = KEY_LEFTARROW;
      break;

    case __TCM_KEY_CODE_BACKSPACE:
      *doomKey = KEY_BACKSPACE;
      break;

    case __TCM_KEY_CODE_ENTER:
      *doomKey = KEY_ENTER;
      break;
    case __TCM_KEY_CODE_TAB:
      *doomKey = KEY_TAB;
      break;
    case __TCM_KEY_CODE_CTRL:
      *doomKey = KEY_USE;
      break;
    case 0x20:
      *doomKey = KEY_FIRE;
      break;
    default:
      *doomKey = kcode;
      break;
    }
    return 1;
  }

  return 0;
}

uint32_t DG_GetTicksMs(void) {
  return (tcm_syscall_get_timestamp() - start_time) * 1000 +
         tcm_syscall_get_timestamp_milli();
}

void DG_SetWindowTitle(const char *title) {}

int main(int _argc, char **_argv) {
  char *argv[] = {_argv[0], "-iwad", "doom1.wad", NULL};
  doomgeneric_Create(3, argv);

  while (1) {
    doomgeneric_Tick();
  }

  return 0;
}