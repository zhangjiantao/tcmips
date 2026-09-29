//
// Created by zhangjiantao on 2026/5/27.
//

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <string.h>

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 120

int main(void) {
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_PIXEL_32);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_DATA_OFFSET, TCM_VRAM_PIXEL_ADDR);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_RESOLUTION,
                            (SCREEN_WIDTH >> 2) - 1);

  for (int i = 0; i < 255; i += 32) {
    for (int j = 0; j < 255; j += 32) {
      for (int k = 0; k < 255; k += 32) {
        uint32_t c = i + (j << 8) + (k << 16);
        static uint32_t refresh = 0;
        for (int n = 0; n < SCREEN_WIDTH * SCREEN_HEIGHT; n += 8) {
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 0) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 1) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 2) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 3) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 4) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 5) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 6) = c;
          *((uint32_t *)TCM_VRAM_PIXEL_ADDR + n + 7) = c;
        }
        tcm_seven_segment_display_upper_hexadecimal(refresh++);
        tcm_seven_segment_display_lower_hexadecimal(c);
      }
    }
  }
  return 1;
}
