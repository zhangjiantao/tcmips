/* SDL1 showcase/regression: embedded-memfs BMP asset animation.
 *
 *   TCMIPSEmulator.exe test_sdl1_bmp_anim.tcm -hold=0
 *
 * The assets/ directory is packed into the .tcm by the linker
 * (-tcm-memfs=<dir>) and loaded with the SDL 1.2 file APIs:
 *
 *   SDL_LoadBMP("lena.bmp")  ->  SDL_LoadBMP_RW(SDL_RWFromFile(...), 1)
 *
 * The asset is the user-provided assets/lena.bmp (dimensions are read from
 * the surface at runtime; no size is hard-coded).
 *
 * Phases:
 *   1. Soft-stretch the image to the full 1024x768 screen, hold 5 s.
 *   2. Split it into a 3x3 grid of alias sub-surfaces
 *      (SDL_CreateRGBSurfaceFrom; the right/bottom tiles include the
 *      division remainder) and bounce them around the screen for 20 s.
 *
 * Everything runs on SDL1 APIs; the guest exits 0 after both phases.  Frame
 * pacing uses SDL_GetTicks busy-spins (SDL1 Delay granularity is coarse on
 * this platform).
 */

#include <SDL/SDL.h>
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

/* Busy-wait until SDL_GetTicks() reaches `target` (wall-clock accurate). */
static void wait_until(Uint32 target) {
  while (SDL_GetTicks() < target) {
  }
}

int main(void) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("[FATAL] SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Surface *screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_SWSURFACE);
  if (!screen || !screen->pixels) {
    printf("[FATAL] SDL_SetVideoMode: %s\n", SDL_GetError());
    return 1;
  }
  printf("[PHASE] video %dx%d %dbpp ok\n", screen->w, screen->h,
         screen->format->BitsPerPixel);

  SDL_Surface *img = SDL_LoadBMP(ASSET_NAME);
  if (!img || !img->pixels) {
    printf("[FATAL] SDL_LoadBMP(%s): %s\n", ASSET_NAME, SDL_GetError());
    return 1;
  }
  printf("[PHASE] asset loaded %dx%d %dbpp pitch=%d\n", img->w, img->h,
         img->format->BitsPerPixel, (int)img->pitch);

  /* ---- Phase 1: stretch to full screen, hold 5 s ---------------------- */
  printf("[PHASE] 1: stretch to fullscreen, hold %ds\n", STRETCH_MS / 1000);
  Uint32 phase_end = SDL_GetTicks() + STRETCH_MS;
  while (SDL_GetTicks() < phase_end) {
    if (SDL_SoftStretch(img, NULL, screen, NULL) != 0) {
      printf("[FATAL] SDL_SoftStretch: %s\n", SDL_GetError());
      SDL_FreeSurface(img);
      SDL_Quit();
      return 1;
    }
    SDL_Flip(screen);
    wait_until(SDL_GetTicks() + 16);
  }

  /* ---- Phase 2: split 3x3 and bounce 20 s ------------------------------
     Tile sizes include the division remainder: the right column / bottom row
     are widened/tallened so the tiles cover the whole asset exactly. */
  printf("[PHASE] 2: nine tiles bounce, %ds\n", BOUNCE_MS / 1000);
  int tw[TILE_COLS], th[TILE_ROWS];
  for (int c = 0; c < TILE_COLS; ++c)
    tw[c] = img->w / TILE_COLS + (c == TILE_COLS - 1 ? img->w % TILE_COLS : 0);
  for (int r = 0; r < TILE_ROWS; ++r)
    th[r] = img->h / TILE_ROWS + (r == TILE_ROWS - 1 ? img->h % TILE_ROWS : 0);
  if (tw[0] <= 0 || th[0] <= 0) {
    printf("[FATAL] asset too small to split\n");
    SDL_FreeSurface(img);
    SDL_Quit();
    return 1;
  }

  SDL_Surface *tiles[TILE_ROWS][TILE_COLS];
  for (int ty = 0; ty < TILE_ROWS; ++ty) {
    for (int tx = 0; tx < TILE_COLS; ++tx) {
      Uint8 *tile_base = (Uint8 *)img->pixels +
                         (size_t)ty * th[0] * (size_t)img->pitch +
                         (size_t)tx * tw[0] * (size_t)img->format->BytesPerPixel;
      tiles[ty][tx] = SDL_CreateRGBSurfaceFrom(
          tile_base, tw[tx], th[ty], img->format->BitsPerPixel,
          (int)img->pitch, img->format->Rmask, img->format->Gmask,
          img->format->Bmask, img->format->Amask);
      if (!tiles[ty][tx]) {
        printf("[FATAL] SDL_CreateRGBSurfaceFrom(tile %d,%d)\n", tx, ty);
        SDL_FreeSurface(img);
        SDL_Quit();
        return 1;
      }
    }
  }

  /* 9 movers: per-tile rect (own width/height) with random start position and
     velocity (LCG seeded from ticks), bouncing inside the screen. */
  SDL_Rect pos[TILE_ROWS][TILE_COLS];
  int vx[TILE_ROWS][TILE_COLS], vy[TILE_ROWS][TILE_COLS];
  Uint32 seed = SDL_GetTicks() | 1u;
  for (int ty = 0; ty < TILE_ROWS; ++ty) {
    for (int tx = 0; tx < TILE_COLS; ++tx) {
      pos[ty][tx].x = (Sint16)(rnd_byte(&seed) % (SCREEN_W - 2 * tw[0]));
      pos[ty][tx].y = (Sint16)(rnd_byte(&seed) % (SCREEN_H - 2 * th[0]));
      vx[ty][tx] = (rnd_byte(&seed) % 9) - 4;
      vy[ty][tx] = (rnd_byte(&seed) % 7) - 3;
      if (vx[ty][tx] == 0)
        vx[ty][tx] = 3;
      if (vy[ty][tx] == 0)
        vy[ty][tx] = 2;
    }
  }

  phase_end = SDL_GetTicks() + BOUNCE_MS;
  int frames = 0;
  while (SDL_GetTicks() < phase_end) {
    SDL_FillRect(screen, NULL, 0); /* black background */
    for (int ty = 0; ty < TILE_ROWS; ++ty) {
      for (int tx = 0; tx < TILE_COLS; ++tx) {
        pos[ty][tx].x = (Sint16)(pos[ty][tx].x + vx[ty][tx]);
        pos[ty][tx].y = (Sint16)(pos[ty][tx].y + vy[ty][tx]);
        if (pos[ty][tx].x < 0) {
          pos[ty][tx].x = 0;
          vx[ty][tx] = -vx[ty][tx];
        }
        if (pos[ty][tx].y < 0) {
          pos[ty][tx].y = 0;
          vy[ty][tx] = -vy[ty][tx];
        }
        if (pos[ty][tx].x > SCREEN_W - tw[tx]) {
          pos[ty][tx].x = (Sint16)(SCREEN_W - tw[tx]);
          vx[ty][tx] = -vx[ty][tx];
        }
        if (pos[ty][tx].y > SCREEN_H - th[ty]) {
          pos[ty][tx].y = (Sint16)(SCREEN_H - th[ty]);
          vy[ty][tx] = -vy[ty][tx];
        }
        if (SDL_BlitSurface(tiles[ty][tx], NULL, screen, &pos[ty][tx]) != 0) {
          printf("[FATAL] blit tile %d,%d: %s\n", tx, ty, SDL_GetError());
          return 1;
        }
      }
    }
    SDL_Flip(screen);
    ++frames;
    wait_until(SDL_GetTicks() + 16);
  }
  printf("[PHASE] done, %d frames in %ds\n", frames, BOUNCE_MS / 1000);

  for (int ty = 0; ty < TILE_ROWS; ++ty)
    for (int tx = 0; tx < TILE_COLS; ++tx)
      SDL_FreeSurface(tiles[ty][tx]);
  SDL_FreeSurface(img);

  printf("[RESULT] SDL1 bmp anim PASS (%d frames)\n", frames);
  SDL_Quit();
  return 0;
}
