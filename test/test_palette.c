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
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 320);
  tcm_pixel_console_clear();

  uint8_t red = COLOR332(255, 0, 0);
  uint8_t green = COLOR332(0, 255, 0);
  uint8_t blue = COLOR332(0, 0, 255);
  uint8_t white = COLOR332(255, 255, 255);
  uint8_t black = COLOR332(0, 0, 0);
  uint8_t gray = COLOR332(128, 128, 128);
  tcm_syscall_palette_init(0, red);
  tcm_syscall_palette_init(1, green);
  tcm_syscall_palette_init(2, blue);
  tcm_syscall_palette_init(3, white);
  tcm_syscall_palette_init(4, black);
  tcm_syscall_palette_init(5, gray);

  uint32_t screen_width = 320;
  uint32_t screen_height = 240;
  uint32_t vram = TCM_VRAM_PIXEL_ADDR;
  uint32_t vram_end = vram + screen_width * screen_height;
  for (uint32_t i = vram; i < vram_end; i += 24) {
    tcm_syscall_palette_set_offset(i);
    tcm_syscall_palette_upload(0, 0x01010101);
    tcm_syscall_palette_set_offset(i + 8);
    tcm_syscall_palette_upload(0x02020202, 0x03030303);
    tcm_syscall_palette_set_offset(i + 16);
    tcm_syscall_palette_upload(0x04040404, 0x05050505);
  }
}
