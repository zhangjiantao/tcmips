/*
This file presents all abstractions needed to port LiteNES.
  (The current working implementation uses allegro library.)

To port this project, replace the following functions by your own:
1) nes_hal_init()
    Do essential initialization work, including starting a FPS HZ timer.

2) nes_set_bg_color(c)
    Set the back ground color to be the NES internal color code c.

3) nes_flush_buf(*buf)
    Flush the entire pixel buf's data to frame buffer.

4) nes_flip_display()
    Fill the screen with previously set background color, and
    display all contents in the frame buffer.

5) wait_for_frame()
    Implement it to make the following code is executed FPS times a second:
        while (1) {
            wait_for_frame();
            do_something();
        }

6) int nes_key_state(int b)
    Query button b's state (1 to be pressed, otherwise 0).
    The correspondence of b and the buttons:
      0 - Power
      1 - A
      2 - B
      3 - SELECT
      4 - START
      5 - UP
      6 - DOWN
      7 - LEFT
      8 - RIGHT
*/
#include "hal.h"
#include "common.h"
#include "fce.h"

#define SCREEN_WIDTH_E ((SCREEN_HEIGHT / 3) * 4)

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <stdio.h>
#include <string.h>

static uint32_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
static uint32_t color_map_rgb888[64];
static int current_bg_color_idx = 0;

/* Wait until next allegro timer event is fired. */
void wait_for_frame() {}

/* Set background color. RGB value of c is defined in fce.h */
void nes_set_bg_color(int c) { current_bg_color_idx = c; }

/* Flush the pixel buffer */
void nes_flush_buf(PixelBuf *buf) {
  for (int i = 0; i < buf->size; i++) {
    Pixel *p = &buf->buf[i];
    if (p->x >= 0 && p->x < SCREEN_WIDTH && p->y >= 0 && p->y < SCREEN_HEIGHT) {
      // 直接写入 32 位颜色数据
      frame_buffer[p->y * SCREEN_WIDTH + p->x] = color_map_rgb888[p->c];
    }
  }
}

/* Initialization:
   (1) start a 1/FPS Hz timer.
   (2) register fce_timer handle on each timer event */
void nes_hal_init() {
  tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, SCREEN_WIDTH_E);

  for (int i = 0; i < 64; i++) {
    pal c = palette[i];
    color_map_rgb888[i] = (c.b << 16) | (c.g << 8) | c.r;
  }
}

/* Update screen at FPS rate by allegro's drawing function.
   Timer ensures this function is called FPS times a second. */
void nes_flip_display() {
  for (int x = 0; x < SCREEN_HEIGHT; x++) {
    uint32_t addr = TCM_VRAM_PIXEL_ADDR + x * SCREEN_WIDTH_E * 4 + 32 * 4;
    uint32_t *src = frame_buffer + x * SCREEN_WIDTH;
    memcpy((void *)addr, src, SCREEN_WIDTH * 4);
  }

  uint32_t bg_color = color_map_rgb888[current_bg_color_idx];
  for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
    frame_buffer[i] = bg_color;
  }
}

#define NES_MASK_A (1 << 0)
#define NES_MASK_B (1 << 1)
#define NES_MASK_SELECT (1 << 2)
#define NES_MASK_START (1 << 3)
#define NES_MASK_UP (1 << 4)
#define NES_MASK_DOWN (1 << 5)
#define NES_MASK_LEFT (1 << 6)
#define NES_MASK_RIGHT (1 << 7)

/* Query a button's state.
   Returns 1 if button #b is pressed. */
int nes_key_state(int b) {
  static uint8_t nes_joypad_state = 0;

  if (b == 1) {
    while (1) {
      uint32_t key_data = tcm_syscall_read_keyboard();

      if (key_data == 0)
        break;

      bool press = __TCM_KEYBOARD_PRESS(key_data);
      uint8_t kcode = __TCM_KEYBOARD_KCODE(key_data);
      uint8_t mask = 0;

      switch (kcode) {
      case 'z':
        mask = NES_MASK_A;
        break;
      case 'x':
        mask = NES_MASK_B;
        break;
      case __TCM_KEY_CODE_LSHIFT:
      case __TCM_KEY_CODE_RSHIFT:
        mask = NES_MASK_SELECT;
        break;
      case __TCM_KEY_CODE_ENTER:
        mask = NES_MASK_START;
        break;
      case __TCM_KEY_CODE_RIGHT:
        mask = NES_MASK_RIGHT;
        break;
      case __TCM_KEY_CODE_LEFT:
        mask = NES_MASK_LEFT;
        break;
      case __TCM_KEY_CODE_UP:
        mask = NES_MASK_UP;
        break;
      case __TCM_KEY_CODE_DOWN:
        mask = NES_MASK_DOWN;
        break;
      default:
        continue;
      }

      if (press) {
        nes_joypad_state |= mask;
      } else {
        nes_joypad_state &= ~mask;
      }
    }
  }

  switch (b) {
  case 0:
    return 1;
  case 1:
    return (nes_joypad_state & NES_MASK_A) ? 1 : 0;
  case 2:
    return (nes_joypad_state & NES_MASK_B) ? 1 : 0;
  case 3:
    return (nes_joypad_state & NES_MASK_SELECT) ? 1 : 0;
  case 4:
    return (nes_joypad_state & NES_MASK_START) ? 1 : 0;
  case 5:
    return (nes_joypad_state & NES_MASK_UP) ? 1 : 0;
  case 6:
    return (nes_joypad_state & NES_MASK_DOWN) ? 1 : 0;
  case 7:
    return (nes_joypad_state & NES_MASK_LEFT) ? 1 : 0;
  case 8:
    return (nes_joypad_state & NES_MASK_RIGHT) ? 1 : 0;
  default:
    return 0;
  }
}
