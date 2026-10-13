// stb_image GIF playback demo (SDL1-based).
//
// This is a hands-on demo (NOT part of the AUTOTEST suite). It loads an
// animated GIF and loops it automatically (no key press needed). Press 'q' to
// exit. GIF metadata (width/height, frame count, channel count, per-frame
// delays, total duration) is logged via printf.
//
// The GIF frames are decoded with stb_image into a contiguous RGBA buffer,
// wrapped as an SDL_Surface via SDL_CreateRGBSurfaceFrom, and soft-stretched
// to the screen each frame (SDL_SoftStretch + SDL_Flip). This avoids touching
// the raw pixel-console VRAM directly, relying on the SDL1 compatibility
// layer instead.
//
// Asset: test/misc/assets/test.gif (animated GIF, read via memfs FILE*).
//
// NOTE: test/misc/.gitignore excludes `*.gif`, so the asset is supplied by the
// developer, not committed to the repository.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>

#include "stb/stb_image.h"

#define SCREEN_W 640
#define SCREEN_H 480

// 32-bit RGBA masks, R in the low byte (matches stb_image's RGBA byte order
// and TCMIPS_SDL_RMASK 0x000000FF). See tcmips_sdl_internal.h.
#define RGBA_RMASK 0x000000FFu
#define RGBA_GMASK 0x0000FF00u
#define RGBA_BMASK 0x00FF0000u
#define RGBA_AMASK 0xFF000000u

// ---------------------------------------------------------------------------
// Read an entire memfs file into a malloc'd buffer.
// ---------------------------------------------------------------------------
static unsigned char *read_file(const char *filename, int *out_len) {
  FILE *f = fopen(filename, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len <= 0) {
    fclose(f);
    return NULL;
  }
  unsigned char *buf = (unsigned char *)malloc((size_t)len);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t rd = fread(buf, 1, (size_t)len, f);
  fclose(f);
  if (rd != (size_t)len) {
    free(buf);
    return NULL;
  }
  *out_len = (int)len;
  return buf;
}

int main(void) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("gif_demo_sdl1: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Surface *screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 32, SDL_SWSURFACE);
  if (!screen || !screen->pixels) {
    printf("gif_demo_sdl1: SDL_SetVideoMode: %s\n", SDL_GetError());
    return 1;
  }

  const char *path = "test.gif";
  int len = 0;
  unsigned char *data = read_file(path, &len);
  if (!data) {
    printf("gif_demo_sdl1: failed to read '%s' (place test.gif in test/misc/assets/)\n", path);
    return 1;
  }

  int w = 0, h = 0, frames = 0, comp = 0;
  int *delays = NULL;
  // req_comp = 4: every frame is RGBA, all frames concatenated into one
  // contiguous buffer of `frames * w * h * 4` bytes.
  unsigned char *pixels = stbi_load_gif_from_memory(
      data, len, &delays, &w, &h, &frames, &comp, 4);
  free(data);

  if (!pixels) {
    printf("gif_demo_sdl1: failed to decode '%s' (reason: %s)\n", path,
           stbi_failure_reason());
    return 1;
  }

  // ------------------------------------------------------------------
  // Log GIF metadata.
  // ------------------------------------------------------------------
  int min_delay = 0x7fffffff, max_delay = 0;
  long total_ms = 0;
  if (delays) {
    for (int f = 0; f < frames; f++) {
      int d = delays[f] > 0 ? delays[f] : 100; // stb: 1/1000 s
      if (d < min_delay)
        min_delay = d;
      if (d > max_delay)
        max_delay = d;
      total_ms += d;
    }
  }
  printf("gif_demo_sdl1: file    = %s\n", path);
  printf("gif_demo_sdl1: size    = %dx%d\n", w, h);
  printf("gif_demo_sdl1: frames  = %d\n", frames);
  printf("gif_demo_sdl1: comp    = %d (decoded RGBA)\n", comp);
  printf("gif_demo_sdl1: delay   = min %dms, max %dms\n", min_delay, max_delay);
  printf("gif_demo_sdl1: total   = %ldms (%.2fs, %.1f fps)\n", total_ms,
         total_ms / 1000.0,
         total_ms > 0 ? (frames * 1000.0 / total_ms) : 0.0);
  printf("gif_demo_sdl1: looping. Q = exit\n");
  fflush(stdout);

  const size_t frame_bytes = (size_t)w * h * 4;

  // Wrap the whole decoded animation (or each frame) as a surface. We build
  // one surface per frame view by re-wrapping the base pointer offset; the
  // surface does not own the pixels, so no per-frame free is needed.
  int running = 1;

  while (running) {
    for (int f = 0; f < frames && running; f++) {
      unsigned char *frame = pixels + (size_t)f * frame_bytes;
      SDL_Surface *fs =
          SDL_CreateRGBSurfaceFrom(frame, w, h, 32, w * 4, RGBA_RMASK,
                                   RGBA_GMASK, RGBA_BMASK, RGBA_AMASK);
      if (!fs) {
        printf("gif_demo_sdl1: SDL_CreateRGBSurfaceFrom: %s\n", SDL_GetError());
        running = 0;
        break;
      }

      if (SDL_SoftStretch(fs, NULL, screen, NULL) != 0) {
        printf("gif_demo_sdl1: SDL_SoftStretch: %s\n", SDL_GetError());
        SDL_FreeSurface(fs);
        running = 0;
        break;
      }
      SDL_Flip(screen);
      SDL_FreeSurface(fs);

      // Frame delay is expressed in 1/1000 second by stb_image.
      int delay_ms = (delays && delays[f] > 0) ? delays[f] : 100;
      Uint32 deadline = SDL_GetTicks() + (Uint32)delay_ms;
      while (running && SDL_GetTicks() < deadline) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT)
            running = 0;
          else if (e.type == SDL_KEYDOWN)
            if (e.key.keysym.sym == 'q' || e.key.keysym.sym == 'Q')
              running = 0;
        }
      }
    }
    // Loop automatically — no key press needed to restart.
  }

  SDL_Quit();

  stbi_image_free(pixels);
  if (delays)
    stbi_image_free(delays);
  printf("gif_demo_sdl1: bye\n");
  return 0;
}
