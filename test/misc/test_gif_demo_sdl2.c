// stb_image GIF playback demo (SDL2-based).
//
// This is a hands-on demo (NOT part of the AUTOTEST suite). It loads an
// animated GIF and loops it automatically (no key press needed). Press 'q' or
// close the window to exit. GIF metadata (width/height, frame count, channel
// count, per-frame delays, total duration) is logged via printf.
//
// The GIF frames are decoded with stb_image into a contiguous RGBA buffer and
// uploaded to the SDL2 renderer via SDL_UpdateTexture + SDL_RenderCopy, which
// stretches each frame to the whole window. This avoids touching the raw
// pixel-console VRAM directly (byte order / alpha pitfalls), relying on the
// SDL2 compatibility layer instead.
//
// Asset: test/misc/assets/test.gif (animated GIF, read via memfs FILE*).
//
// NOTE: test/misc/.gitignore excludes `*.gif`, so the asset is supplied by the
// developer, not committed to the repository.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "stb/stb_image.h"

#define SCREEN_W 640
#define SCREEN_H 480

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

// ---------------------------------------------------------------------------
// Upload one decoded RGBA frame and present it stretched to the window.
// ---------------------------------------------------------------------------
static void present_frame(SDL_Renderer *ren, SDL_Texture *tex,
                          const unsigned char *frame, int w, int h) {
  // SDL_PIXELFORMAT_RGBA32 == R,G,B,A byte order in memory on little-endian,
  // exactly matching stb_image's GIF frame layout (p[0]=R, p[1]=G, p[2]=B,
  // p[3]=A). No per-pixel conversion needed.
  SDL_UpdateTexture(tex, NULL, frame, w * 4);
  SDL_RenderClear(ren);
  SDL_RenderCopy(ren, tex, NULL, NULL); // stretch whole frame to window
  SDL_RenderPresent(ren);
}

int main(void) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("gif_demo_sdl2: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *win = SDL_CreateWindow("gif demo", SDL_WINDOWPOS_UNDEFINED,
                                     SDL_WINDOWPOS_UNDEFINED, SCREEN_W,
                                     SCREEN_H, SDL_WINDOW_SHOWN);
  SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, 0) : NULL;
  if (!win || !ren) {
    printf("gif_demo_sdl2: window/renderer setup failed: %s\n", SDL_GetError());
    return 1;
  }

  const char *path = "test.gif";
  int len = 0;
  unsigned char *data = read_file(path, &len);
  if (!data) {
    printf("gif_demo_sdl2: failed to read '%s' (place test.gif in test/misc/assets/)\n", path);
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
    printf("gif_demo_sdl2: failed to decode '%s' (reason: %s)\n", path,
           stbi_failure_reason());
    return 1;
  }

  // Streaming texture sized to one GIF frame; SDL_RenderCopy stretches it.
  SDL_Texture *tex =
      SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                        w, h);
  if (!tex) {
    printf("gif_demo_sdl2: SDL_CreateTexture: %s\n", SDL_GetError());
    stbi_image_free(pixels);
    if (delays)
      stbi_image_free(delays);
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
  printf("gif_demo_sdl2: file    = %s\n", path);
  printf("gif_demo_sdl2: size    = %dx%d\n", w, h);
  printf("gif_demo_sdl2: frames  = %d\n", frames);
  printf("gif_demo_sdl2: comp    = %d (decoded RGBA)\n", comp);
  printf("gif_demo_sdl2: delay   = min %dms, max %dms\n", min_delay, max_delay);
  printf("gif_demo_sdl2: total   = %ldms (%.2fs, %.1f fps)\n", total_ms,
         total_ms / 1000.0,
         total_ms > 0 ? (frames * 1000.0 / total_ms) : 0.0);
  printf("gif_demo_sdl2: looping. Q = exit\n");
  fflush(stdout);

  const size_t frame_bytes = (size_t)w * h * 4;
  int running = 1;

  while (running) {
    for (int f = 0; f < frames && running; f++) {
      const unsigned char *frame = pixels + (size_t)f * frame_bytes;
      present_frame(ren, tex, frame, w, h);

      // Frame delay is expressed in 1/1000 second by stb_image.
      int delay_ms = (delays && delays[f] > 0) ? delays[f] : 100;
      Uint32 deadline = SDL_GetTicks() + (Uint32)delay_ms;
      while (running && SDL_GetTicks() < deadline) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          if (e.type == SDL_QUIT)
            running = 0;
          else if (e.type == SDL_KEYDOWN)
            if (e.key.keysym.sym == SDLK_q)
              running = 0;
        }
      }
    }
    // Loop automatically — no key press needed to restart.
  }

  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();

  stbi_image_free(pixels);
  if (delays)
    stbi_image_free(delays);
  printf("gif_demo_sdl2: bye\n");
  return 0;
}
