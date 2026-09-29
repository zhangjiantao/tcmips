#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define W 800
#define H 600

static void draw_circle(SDL_Surface *surf, int cx, int cy, int r,
                        Uint32 color) {
  for (int y = -r; y <= r; y++) {
    for (int x = -r; x <= r; x++) {
      if (x * x + y * y <= r * r) {
        int px = cx + x;
        int py = cy + y;
        if (px >= 0 && px < W && py >= 0 && py < H) {
          Uint8 *p = (Uint8 *)surf->pixels + py * surf->pitch + px * 4;
          *(Uint32 *)p = color;
        }
      }
    }
  }
}

static void draw_line(SDL_Surface *surf, int x0, int y0, int x1, int y1,
                      Uint32 color) {
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  int x = x0, y = y0;
  while (1) {
    if (x >= 0 && x < W && y >= 0 && y < H) {
      Uint8 *p = (Uint8 *)surf->pixels + y * surf->pitch + x * 4;
      *(Uint32 *)p = color;
    }
    if (x == x1 && y == y1)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
}

int main(int argc, char **argv) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init fail: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *win =
      SDL_CreateWindow("SDL2 Stability Test", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
  if (!win) {
    fprintf(stderr, "SDL_CreateWindow fail: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Surface *screen = SDL_GetWindowSurface(win);
  int running = 1;
  SDL_Event e;
  Uint32 frame = 0;

  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = 0;
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        running = 0;
    }

    // 清屏，深蓝底色
    Uint32 bg = SDL_MapRGB(screen->format, 10, 15, 30);
    SDL_FillRect(screen, NULL, bg);

    double t = frame * 0.02;
    int cx = W / 2;
    int cy = H / 2;

    // 旋转放射线条
    for (int i = 0; i < 36; i++) {
      double ang = t + i * (M_PI * 2 / 36.0);
      int x2 = cx + (int)(220 * cos(ang));
      int y2 = cy + (int)(220 * sin(ang));
      Uint32 col = SDL_MapRGB(screen->format, (i * 7) & 0xFF,
                              (128 + i * 5) & 0xFF, (200 + i * 3) & 0xFF);
      draw_line(screen, cx, cy, x2, y2, col);
    }

    // 旋转移动的同心圆
    for (int r = 30; r <= 180; r += 30) {
      int offX = (int)(60 * cos(t * 1.3 + r * 0.05));
      int offY = (int)(60 * sin(t * 0.9 + r * 0.05));
      Uint32 c = SDL_MapRGB(screen->format, (r) & 0xFF, (255 - r) & 0xFF, 120);
      draw_circle(screen, cx + offX, cy + offY, r / 4, c);
    }

    // 移动小方块阵列
    for (int j = 0; j < 6; j++) {
      for (int i = 0; i < 8; i++) {
        int sx = 40 + i * 90 + (int)(12 * sin(t * 2 + i + j));
        int sy = 40 + j * 90 + (int)(12 * cos(t * 2 + i + j));
        SDL_Rect rc = {sx, sy, 16, 16};
        Uint32 rcCol = SDL_MapRGB(screen->format, 255, 220, 80);
        SDL_FillRect(screen, &rc, rcCol);
      }
    }

    SDL_UpdateWindowSurface(win);
    frame++;
  }

  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
