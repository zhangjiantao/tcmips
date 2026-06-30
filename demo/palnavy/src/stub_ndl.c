//
// Created by zhangjiantao on 2026/6/4.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include "NDL.h"
#include "util.h"
#include "video.h"

#include <stdio.h>
#include <string.h>

void NDL_Quit(void) {}

void NDL_OpenCanvas(int *w, int *h) {
  if (*w == 0 && *h == 0) {
    *w = 320;
    *h = 200;
  }
}

#define SCREEN_PHYS_WIDTH 320
#define SCREEN_PHYS_HEIGHT 200

#ifdef Q256_HARD_PALETTE

int NDL_Init(uint32_t flags) {
  UTIL_LogOutput(LOGLEVEL_INFO, "%s\n", __func__);

  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 320);
  tcm_pixel_console_clear();

  return 0;
}

void NDL_DrawRect(void *pixels, int x, int y, int w, int h) {
  if (!pixels)
    return;

  volatile uint32_t *vram_base = (volatile uint32_t *)TCM_VRAM_PIXEL_ADDR;
  uint64_t *src = (uint64_t *)pixels;
  uint64_t *dst = (uint64_t *)TCM_VRAM_PIXEL_ADDR;
  uint64_t *end_dst = dst + 64000 / 8;

  for (uint64_t *ptr = dst; ptr < end_dst; ptr++, src++) {
    tcm_syscall_palette_set_offset((uint32_t)ptr);

    uint64_t data = *src;
    tcm_syscall_palette_upload(data & 0xffffffff, data >> 32);
  }
}

#else

int NDL_Init(uint32_t flags) {
  UTIL_LogOutput(LOGLEVEL_INFO, "%s\n", __func__);

  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, 320);
  tcm_pixel_console_clear();

  return 0;
}

void NDL_DrawRect(void *pixels, int x, int y, int w, int h) {
  if (!pixels)
    return;

  volatile uint32_t *vram_base = (volatile uint32_t *)TCM_VRAM_PIXEL_ADDR;
  uint8_t *src_pixels = (uint8_t *)pixels;
  SDL_Color *colors = gpScreen->format->palette->colors;

  int total_pixels = 320 * 200;

  for (int i = 0; i < total_pixels; i += 8) {
    vram_base[i] = colors[src_pixels[i]].val;
    vram_base[i + 1] = colors[src_pixels[i + 1]].val;
    vram_base[i + 2] = colors[src_pixels[i + 2]].val;
    vram_base[i + 3] = colors[src_pixels[i + 3]].val;
    vram_base[i + 4] = colors[src_pixels[i + 4]].val;
    vram_base[i + 5] = colors[src_pixels[i + 5]].val;
    vram_base[i + 6] = colors[src_pixels[i + 6]].val;
    vram_base[i + 7] = colors[src_pixels[i + 7]].val;
  }
}

#endif
