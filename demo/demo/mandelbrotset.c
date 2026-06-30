//
// Created by zhangjiantao on 2026/6/1.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <stdint.h>

#define WIDTH 320
#define HEIGHT 240
#define MAX_ITER 48

#define SHIFT_AMOUNT 16
#define FIXED_ONE (1 << SHIFT_AMOUNT)

const uint32_t palette1[8] = {0x000000, 0x330000, 0x660000, 0x992200,
                              0xCC4400, 0xFF6600, 0xFFAA00, 0xFFFFFF};

const uint32_t palette2[8] = {0x001100, 0x002200, 0x004411, 0x006622,
                              0x228833, 0x44AA55, 0x66CC77, 0xFFFFFF};

const uint32_t palette3[8] = {0x110022, 0x330055, 0x550088, 0x7722BB,
                              0x9933DD, 0xBB55EE, 0xDD77FF, 0xFFFFFF};

void draw_mandelbrot_fixed(const uint32_t palette[8], uint32_t *VRAM) {
  int32_t x_start = -2 * FIXED_ONE;
  int32_t x_end = (1 * FIXED_ONE) / 2;
  int32_t y_start = -5 * FIXED_ONE / 4;
  int32_t y_end = 5 * FIXED_ONE / 4;

  int32_t dx = (x_end - x_start) / WIDTH;
  int32_t dy = (y_end - y_start) / HEIGHT;

  int32_t ci = y_start;
  for (int y = 0; y < HEIGHT; y++) {
    int32_t cr = x_start;
    for (int x = 0; x < WIDTH; x++) {
      if (tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q')
        return;

      int32_t zr = 0;
      int32_t zi = 0;
      int iter = 0;

      while (iter < MAX_ITER) {
        int64_t zr2_64 = ((int64_t)zr * zr) >> SHIFT_AMOUNT;
        int64_t zi2_64 = ((int64_t)zi * zi) >> SHIFT_AMOUNT;

        if (zr2_64 + zi2_64 > (4 * FIXED_ONE)) {
          break;
        }

        int32_t zr_next = (int32_t)(zr2_64 - zi2_64) + cr;
        zi = (int32_t)(((int64_t)zr * zi) >> (SHIFT_AMOUNT - 1)) + ci;
        zr = zr_next;
        iter++;
      }

      uint32_t color = (iter == MAX_ITER) ? 0 : palette[iter % 8];
      VRAM[y * WIDTH + x] = color;

      cr += dx;
    }
    ci += dy;
  }
}

void draw_julia_fixed(const uint32_t palette[8], uint32_t *VRAM) {
  const int32_t cr_fixed = 0.285f * FIXED_ONE;
  const int32_t ci_fixed = 0.01f * FIXED_ONE;

  int32_t x_start = -1.5f * FIXED_ONE;
  int32_t x_end = 1.5f * FIXED_ONE;
  int32_t y_start = -1.2f * FIXED_ONE;
  int32_t y_end = 1.2f * FIXED_ONE;

  int32_t dx = (x_end - x_start) / WIDTH;
  int32_t dy = (y_end - y_start) / HEIGHT;

  int32_t cy = y_start;
  for (int y = 0; y < HEIGHT; y++) {
    int32_t cx = x_start;
    for (int x = 0; x < WIDTH; x++) {
      if (tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q')
        return;

      int32_t zr = cx;
      int32_t zi = cy;
      int iter = 0;

      while (iter < MAX_ITER) {
        int64_t zr2 = ((int64_t)zr * zr) >> SHIFT_AMOUNT;
        int64_t zi2 = ((int64_t)zi * zi) >> SHIFT_AMOUNT;
        if (zr2 + zi2 > 4 * FIXED_ONE)
          break;

        int32_t new_zr = (int32_t)(zr2 - zi2) + cr_fixed;
        int32_t new_zi =
            (int32_t)(((int64_t)zr * zi) << 1 >> SHIFT_AMOUNT) + ci_fixed;

        zr = new_zr;
        zi = new_zi;
        iter++;
      }

      uint32_t color = (iter == MAX_ITER) ? 0 : palette[iter % 8];
      VRAM[y * WIDTH + x] = color;
      cx += dx;
    }
    cy += dy;
  }
}

void draw_burning_ship_fixed(const uint32_t palette[8], uint32_t *VRAM) {
  int32_t x_start = -1.8f * FIXED_ONE;
  int32_t x_end = -1.7f * FIXED_ONE;
  int32_t y_start = -0.08f * FIXED_ONE;
  int32_t y_end = 0.02f * FIXED_ONE;

  int32_t dx = (x_end - x_start) / WIDTH;
  int32_t dy = (y_end - y_start) / HEIGHT;

  int32_t ci = y_start;
  for (int y = 0; y < HEIGHT; y++) {
    int32_t cr = x_start;
    for (int x = 0; x < WIDTH; x++) {
      if (tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q')
        return;

      int32_t zr = 0, zi = 0;
      int iter = 0;

      while (iter < MAX_ITER) {
        int64_t zr2 = ((int64_t)zr * zr) >> SHIFT_AMOUNT;
        int64_t zi2 = ((int64_t)zi * zi) >> SHIFT_AMOUNT;
        if (zr2 + zi2 > 4 * FIXED_ONE)
          break;

        int32_t new_zi = ((int32_t)(((int64_t)zr * zi) << 1 >> SHIFT_AMOUNT));
        if (new_zi < 0)
          new_zi = -new_zi;
        new_zi += ci;

        int32_t new_zr = (int32_t)(zr2 - zi2) + cr;
        if (new_zr < 0)
          new_zr = -new_zr;

        zr = new_zr;
        zi = new_zi;
        iter++;
      }

      uint32_t color = (iter == MAX_ITER) ? 0 : palette[iter % 8];
      VRAM[y * WIDTH + x] = color;
      cr += dx;
    }
    ci += dy;
  }
}

void mandelbrotset() {
  void *VRAM = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, WIDTH);
  tcm_pixel_console_clear();

  draw_mandelbrot_fixed(palette1, VRAM);
  draw_julia_fixed(palette2, VRAM);
}
