/* SDL2 interactive demo: SDL_RenderSetLogicalSize visual check.
 *
 * Purpose: show that a circle drawn in *logical* coordinates keeps the same
 * on-screen size and position regardless of the logical resolution, because
 * SDL_RenderSetLogicalSize maps the logical coordinate system onto the fixed
 * output window.  Only the colour is changed each round, and the colour name
 * is printed so the change can be matched to the eye.
 *
 * The window is fixed at 640x480.  Each round:
 *   1. sets a new (4:3) logical size,
 *   2. clears, then draws a filled circle centred in logical coordinates
 *      whose diameter is a fixed fraction of the logical width,
 *   3. presents and waits ~1 s before the next round.
 *
 * Because the circle always occupies the same *relative* position/size in
 * logical space, it must appear at the same size/position on screen every
 * round — only the colour should change.  (All logical sizes are 4:3 so the
 * simplified non-aspect-preserving scale is uniform and the circle stays
 * round.)
 *
 * Run (auto-exit after the fixed rounds):
 *   TCMIPSEmulator test_sdl2_logicalsize.tcm -hold=0
 */

#include <SDL2/SDL.h>
#include <stdio.h>

#define WIN_W 640
#define WIN_H 480

/* 10 rounds of 4:3 logical sizes (sorted by logical width) with an easily
 * distinguishable colour and its name for the log line. */
static const struct {
  int w;
  int h;
  Uint8 r;
  Uint8 g;
  Uint8 b;
  const char *name;
} rounds[] = {
    {40, 30, 255, 0, 0, "red"},         {100, 75, 0, 255, 0, "green"},
    {160, 120, 0, 0, 255, "blue"},      {200, 150, 255, 255, 0, "yellow"},
    {320, 240, 0, 255, 255, "cyan"},    {400, 300, 255, 0, 255, "magenta"},
    {480, 360, 255, 165, 0, "orange"},  {560, 420, 128, 0, 128, "purple"},
    {640, 480, 255, 255, 255, "white"}, {320, 240, 0, 0, 0, "black"},
};

#define N_ROUNDS ((int)(sizeof(rounds) / sizeof(rounds[0])))

/* Fill a circle in *logical* coordinates, row by row.  Each logical row is a
 * single horizontal SDL_RenderFillRect spanning the circle's width at that
 * row, so the interior is one continuous band (no per-pixel rect tiling, which
 * would expose the renderer's rounding at non-integer scales as a grid). */
static void draw_circle(SDL_Renderer *ren, int cx, int cy, int r) {
  for (int y = -r; y <= r; ++y) {
    /* Half-width of the circle at this row. */
    int half = 0;
    while (half <= r && half * half + y * y <= r * r)
      ++half;
    if (half == 0)
      continue;
    SDL_Rect row = {cx - (half - 1), cy + y, 2 * (half - 1) + 1, 1};
    SDL_RenderFillRect(ren, &row);
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *win =
      SDL_CreateWindow("SDL_RenderSetLogicalSize check", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
  if (!win) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
  if (!ren) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  for (int i = 0; i < N_ROUNDS; ++i) {
    int lw = rounds[i].w;
    int lh = rounds[i].h;

    /* Establish the logical coordinate system for this round. */
    if (SDL_RenderSetLogicalSize(ren, lw, lh) != 0) {
      fprintf(stderr, "SDL_RenderSetLogicalSize(%dx%d): %s\n", lw, lh,
              SDL_GetError());
      continue;
    }

    /* Clear in a dark background (also in logical space). */
    SDL_SetRenderDrawColor(ren, 20, 20, 28, 255);
    SDL_RenderClear(ren);

    /* Centred filled circle whose diameter is 3/4 of the logical width. */
    SDL_SetRenderDrawColor(ren, rounds[i].r, rounds[i].g, rounds[i].b, 255);
    int radius = (lw * 3) / 8;
    draw_circle(ren, lw / 2, lh / 2, radius);

    SDL_RenderPresent(ren);
    printf("[round %d] logical %d x %d, diameter %d, colour %s\n", i, lw, lh,
           radius * 2, rounds[i].name);
    fflush(stdout);
    SDL_Delay(1000);
  }

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}