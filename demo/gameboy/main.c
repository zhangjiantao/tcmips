//
// Created by zhangjiantao on 2026/6/1.
//

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdio.h>

unsigned global_frame_counter = 0;

#define ENABLE_SOUND 0
// #define PEANUT_GB_HEADER_ONLY
#include "peanut_gb.h"

extern uint8_t gb_rom_read(struct gb_s *gb, uint32_t addr);
extern uint8_t gb_cart_ram_read(struct gb_s *gb, uint32_t addr);
extern void gb_cart_ram_write(struct gb_s *gb, uint32_t addr, uint8_t val);
extern void gb_error(struct gb_s *gb, enum gb_error_e gb_err, uint16_t val);
extern void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, uint8_t line);
extern void poll_hardware_input(struct gb_s *gb);

int main() {
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_8, 192);
  tcm_pixel_console_clear();

  struct gb_s gb;

  int init_res = gb_init(&gb, gb_rom_read, gb_cart_ram_read, gb_cart_ram_write,
                         gb_error, NULL);
  printf("gb_init %d\n", init_res);

  if (init_res != GB_INIT_NO_ERROR) {
    printf("Error: gb_init %d\n", init_res);
    exit(init_res);
  }

  printf("\nKeyMap:\n");
  printf("JOYCON_A       ---  z\n");
  printf("JOYCON_B       ---  x\n");
  printf("JOYCON_START   ---  Enter\n");
  printf("JOYCON_SELECT  ---  Shift\n");

  gb_init_lcd(&gb, lcd_draw_line);

  gb.display.bg_palette[0] = 0;
  gb.display.bg_palette[1] = 1;
  gb.display.bg_palette[2] = 2;
  gb.display.bg_palette[3] = 3;

  gb.direct.joypad = 0xFF;

  gb.direct.interlace = 0;
  gb.direct.frame_skip = 1;

  while (1) {
    printf("global_frame_counter %d\r", global_frame_counter);
    // tcm_seven_segment_display_upper_hexadecimal(global_frame_counter);
    fflush(stdout);
    poll_hardware_input(&gb);
    gb_run_frame(&gb);
    global_frame_counter++;
  }
  return 0;
}