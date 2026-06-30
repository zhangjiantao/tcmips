#include <stdio.h>
#include <stdint.h>

static int nr_draw = 0;
static uint32_t lastTime = 0;

void UpdateFPS(uint32_t now) {
  // if (now - lastTime > 1000) {
  //   int fps = nr_draw * 1000 / (now - lastTime);
  //   if (fps > 99) fps = 99;
  //   printf("\r(System time: %6ds) FPS = %2d%c", now / 1000, fps, fps == 99 ? '+' : ' ');
  //   fflush(stdout);
  //   nr_draw = 0;
  //   lastTime = now;
  // }
}

void IncreaseDraw() {
  nr_draw ++;
}
