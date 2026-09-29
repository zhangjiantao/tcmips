//
// Created by zhangjiantao on 2026/4/30.
//

#include "sys/unistd.h"

#include <dev/console.h>
#include <dev/seven_segment_display.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/times.h>

#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#define COLOR332(r, g, b) (((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6))

int main(int argc, char *argv[]) {
  uint32_t screen_width = 320;
  uint32_t screen_height = 240;
  uint32_t vram =
      (uint32_t)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, screen_width);
  uint32_t vram_end = vram + screen_width * screen_height;
  tcm_pixel_console_clear();

  uint8_t red = COLOR332(255, 0, 0);
  uint8_t green = COLOR332(0, 255, 0);
  uint8_t blue = COLOR332(0, 0, 255);
  uint8_t white = COLOR332(255, 255, 255);
  uint8_t black = COLOR332(0, 0, 0);
  uint8_t gray = COLOR332(128, 128, 128);

  const char *colors[] = {"red", "green", "blue", "white", "black", "gray"};

  tcm_syscall_palette_init(0, red);
  tcm_syscall_palette_init(1, green);
  tcm_syscall_palette_init(2, blue);
  tcm_syscall_palette_init(3, white);
  tcm_syscall_palette_init(4, black);
  tcm_syscall_palette_init(5, gray);

  sleep(1);
  for (uint32_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
    const char *color = colors[i];
    printf("color %s\n", color);
    for (uint32_t j = vram; j < vram_end; j += 8) {
      tcm_syscall_palette_set_offset(j);
      uint32_t w = i << 24 | i << 16 | i << 8 | i;
      tcm_syscall_palette_upload(w, w);
    }
    sleep(1);
  }
}
