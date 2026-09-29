//
// Created by zhangjiantao on 2026/6/1.
//

#include "time.h"

#include <dev/console.h>
#include <dev/syscall.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 80
#define HEIGHT 60
uint8_t world[WIDTH * HEIGHT];
uint8_t next_world[WIDTH * HEIGHT];

static uint32_t * FRAMEBUFFER = NULL;

void init_world_acorn() {
  memset(world, 0, WIDTH * HEIGHT);

  int cx = WIDTH / 2;
  int cy = HEIGHT / 2;

  // . X . . . . .
  // . . . X . . .
  // X X . . X X X

  world[(cy - 1) * WIDTH + (cx - 2)] = 1;

  world[cy * WIDTH + (cx)] = 1;

  world[(cy + 1) * WIDTH + (cx - 3)] = 1;
  world[(cy + 1) * WIDTH + (cx - 2)] = 1;
  world[(cy + 1) * WIDTH + (cx + 1)] = 1;
  world[(cy + 1) * WIDTH + (cx + 2)] = 1;
  world[(cy + 1) * WIDTH + (cx + 3)] = 1;
}

void update_life_torus() {
  for (int y = 0; y < HEIGHT; y++) {
    int y_up = (y == 0) ? (HEIGHT - 1) : (y - 1);
    int y_down = (y == HEIGHT - 1) ? 0 : (y + 1);

    for (int x = 0; x < WIDTH; x++) {
      int x_left = (x == 0) ? (WIDTH - 1) : (x - 1);
      int x_right = (x == WIDTH - 1) ? 0 : (x + 1);

      int neighbors =
          world[y_up * WIDTH + x_left] + world[y_up * WIDTH + x] +
          world[y_up * WIDTH + x_right] + world[y * WIDTH + x_left] +
          world[y * WIDTH + x_right] + world[y_down * WIDTH + x_left] +
          world[y_down * WIDTH + x] + world[y_down * WIDTH + x_right];

      int idx = y * WIDTH + x;
      if (world[idx] == 1) {
        next_world[idx] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
      } else {
        next_world[idx] = (neighbors == 3) ? 1 : 0;
      }

      FRAMEBUFFER[idx] = next_world[idx] ? 0xFFFFFF : 0x000000;
    }
  }

  memcpy(world, next_world, WIDTH * HEIGHT);
}

int main() {
  FRAMEBUFFER = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, WIDTH);
  tcm_pixel_console_clear();

  init_world_acorn();
  uint32_t frame = 5026;
  while (frame--) {
    // printf("%d\n", frame);
    update_life_torus();
  }
}