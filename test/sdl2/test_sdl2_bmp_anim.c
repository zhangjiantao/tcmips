/* SDL2 showcase/regression: embedded-memfs BMP asset animation.
 *
 *   TCMIPSEmulator.exe test_sdl2_bmp_anim.tcm -hold=0
 *
 * The assets/ directory is packed into the .tcm by the linker
 * (-tcm-memfs=<dir>) and loaded with the SDL2 file/render APIs:
 *
 *   SDL_LoadBMP("lena.bmp")       -> SDL_LoadBMP_RW over the RWops layer
 *   SDL_CreateTextureFromSurface  -> RGBA32 streaming into the renderer
 *
 * The asset is the user-provided assets/lena.bmp (dimensions are read from
 * the surface at runtime; no size is hard-coded).
 *
 * Phases:
 *   1. RenderCopy of the whole texture with dstrect == NULL fills the whole
 *      viewport (1024x768) — the asset is GPU-path stretched.  Hold 5 s.
 *   2. RenderCopy per 3x3 source tile (division remainder folded into the
 *      right column / bottom row) into 9 randomly moving destination rects
 *      for 20 s.
 *
 * The guest exits 0 after both phases.  Frame pacing uses SDL_GetTicks
 * busy-spins (SDL_Delay granularity is coarse on this platform).
 */

#include <SDL2/SDL.h>
#include <stdio.h>

#define SCREEN_W 1024
#define SCREEN_H 768
#define ASSET_NAME "lena.bmp"
#define TILE_COLS 3
#define TILE_ROWS 3
#define NUM_TILES (TILE_COLS * TILE_ROWS)
#define STRETCH_MS 5000
#define BOUNCE_MS 20000

static Uint8 rnd_byte(Uint32 *seed) {
  *seed = *seed * 1103515245u + 12345u;
  return (Uint8)((*seed >> 16) & 0xFF);
}

static void wait_until(Uint32 target) {
  while (SDL_GetTicks() < target) {
  }
}

int main(void) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("[FATAL] SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *win = SDL_CreateWindow("sdl2 bmp anim", SDL_WINDOWPOS_UNDEFINED,
                                     SDL_WINDOWPOS_UNDEFINED, SCREEN_W,
                                     SCREEN_H, SDL_WINDOW_SHOWN);
  SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
  SDL_Surface *screen = win ? SDL_GetWindowSurface(win) : NULL;
  if (!win || !ren || !screen || !screen->pixels) {
    printf("[FATAL] window/renderer setup failed\n");
    return 1;
  }
  printf("[PHASE] video %dx%d ok\n", screen->w, screen->h);

  SDL_Surface *img = SDL_LoadBMP(ASSET_NAME);
  if (!img || !img->pixels) {
    printf("[FATAL] SDL_LoadBMP(%s): %s\n", ASSET_NAME, SDL_GetError());
    return 1;
  }
  printf("[PHASE] asset loaded %dx%d %dbpp pitch=%d\n", img->w, img->h,
         img->format->BitsPerPixel, img->pitch);

  SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, img);
  if (!tex) {
    printf("[FATAL] SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    return 1;
  }

  /* ---- Phase 1: whole-texture stretch to viewport, hold 5 s ----------- */
  printf("[PHASE] 1: stretch to fullscreen, hold %ds\n", STRETCH_MS / 1000);
  Uint32 phase_end = SDL_GetTicks() + STRETCH_MS;
  while (SDL_GetTicks() < phase_end) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    /* dstrect == NULL: the whole texture is stretched over the viewport. */
    if (SDL_RenderCopy(ren, tex, NULL, NULL) != 0) {
      printf("[FATAL] RenderCopy stretch: %s\n", SDL_GetError());
      return 1;
    }
    SDL_RenderPresent(ren);
    wait_until(SDL_GetTicks() + 16);
  }

  /* ---- Phase 2: nine source tiles bounce for 20 s ---------------------
     Tile source rects include the division remainder: the right column /
     bottom row are widened/tallened so the tiles cover the whole asset.
     All motion math is pure integer (SDL_Rect + int velocities); the float
     RenderCopyF variant intentionally is not used. */
  printf("[PHASE] 2: nine tiles bounce, %ds\n", BOUNCE_MS / 1000);
  SDL_Rect src[NUM_TILES];
  SDL_Rect pos[NUM_TILES];
  int vx[NUM_TILES], vy[NUM_TILES];
  int tw[TILE_COLS], th[TILE_ROWS];
  for (int c = 0; c < TILE_COLS; ++c)
    tw[c] = img->w / TILE_COLS + (c == TILE_COLS - 1 ? img->w % TILE_COLS : 0);
  for (int r = 0; r < TILE_ROWS; ++r)
    th[r] = img->h / TILE_ROWS + (r == TILE_ROWS - 1 ? img->h % TILE_ROWS : 0);
  Uint32 seed = SDL_GetTicks() | 1u;
  for (int ty = 0; ty < TILE_ROWS; ++ty) {
    for (int tx = 0; tx < TILE_COLS; ++tx) {
      int i = ty * TILE_COLS + tx;
      src[i].x = tx * tw[0];
      src[i].y = ty * th[0];
      src[i].w = tw[tx];
      src[i].h = th[ty];
      pos[i].x = (int)(rnd_byte(&seed) % (SCREEN_W - 2 * tw[0]));
      pos[i].y = (int)(rnd_byte(&seed) % (SCREEN_H - 2 * th[0]));
      pos[i].w = tw[tx];
      pos[i].h = th[ty];
      vx[i] = (int)(rnd_byte(&seed) % 9) - 4;
      vy[i] = (int)(rnd_byte(&seed) % 7) - 3;
      if (vx[i] == 0)
        vx[i] = 3;
      if (vy[i] == 0)
        vy[i] = 2;
    }
  }

  phase_end = SDL_GetTicks() + BOUNCE_MS;
  int frames = 0;
  while (SDL_GetTicks() < phase_end) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    for (int i = 0; i < NUM_TILES; ++i) {
      int txc = i % TILE_COLS, tyc = i / TILE_COLS;
      pos[i].x += vx[i];
      pos[i].y += vy[i];
      if (pos[i].x < 0) {
        pos[i].x = 0;
        vx[i] = -vx[i];
      }
      if (pos[i].y < 0) {
        pos[i].y = 0;
        vy[i] = -vy[i];
      }
      if (pos[i].x > SCREEN_W - pos[i].w) {
        pos[i].x = SCREEN_W - pos[i].w;
        vx[i] = -vx[i];
      }
      if (pos[i].y > SCREEN_H - pos[i].h) {
        pos[i].y = SCREEN_H - pos[i].h;
        vy[i] = -vy[i];
      }
      if (SDL_RenderCopy(ren, tex, &src[i], &pos[i]) != 0) {
        printf("[FATAL] RenderCopy tile %d: %s\n", i, SDL_GetError());
        return 1;
      }
    }
    SDL_RenderPresent(ren);
    ++frames;
    wait_until(SDL_GetTicks() + 16);
  }
  printf("[PHASE] done, %d frames in %ds\n", frames, BOUNCE_MS / 1000);

  SDL_DestroyTexture(tex);
  SDL_FreeSurface(img);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);

  printf("[RESULT] SDL2 bmp anim PASS (%d frames)\n", frames);
  SDL_Quit();
  return 0;
}
