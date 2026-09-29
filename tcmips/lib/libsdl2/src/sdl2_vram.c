/* VRAM driver for the TCMIPS SDL2 compatibility layer.
 *
 * Drives the hardware pixel console directly (tcm_pixel_console_init),
 * independently of the SDL1 compatibility layer.  The interface and the
 * rescale core are kept IDENTICAL to the SDL1 layer
 * (tcmips/lib/libsdl/src/sdl_vram.c) so that both compatibility layers
 * behave consistently and can benefit from the same optimisations.
 *
 * Physics:
 *   The TCMIPS pixel console resolution is 4(n+1) x 3(n+1) for n derived from
 *   the requested width.  SDL2 keeps a logical window surface at the exact
 *   resolution the user requested; this module presents it to the hardware
 *   framebuffer returned at init time.
 *
 * Rescale core / 缩放核心:
 *   - identical dimensions  -> per-row memcpy (hot path, zero overhead);
 *   - differing dimensions  -> nearest-neighbour rescale that maps the whole
 *     logical buffer onto the whole physical screen.  A column source-index
 *     map is precomputed once per logical width, so the inner loop is a pure
 *     indexed copy with NO per-pixel integer division.  All index arithmetic
 *     is 32-bit (max dimension 1024 => products stay far below 2^32), which
 *     is much cheaper than the 64-bit helpers on the TCMIPS MIPS core.
 *
 * SDL2 upstream headers from branch SDL2,
 * commit b90ac95029d801c5abc59472ba8e2200dff31e1e
 */

#include "sdl2_vram.h"

#include <dev/console.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <SDL.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cache of the physical console state. */
static void *s_fb_addr;
static int s_hw_w;
static int s_hw_h;
static int s_initialized;

/* Rebuild-able column map: source x index for each physical pixel column.
 * Covers any supported physical width (<= TCM_SCREEN_MAX_W). */
#define TCMIPS_SDL_VRAM_MAX_W 1024
static uint32_t s_col_map[TCMIPS_SDL_VRAM_MAX_W];
static int s_col_map_width;
static int s_col_map_src_w;

static void build_col_map(int src_w, int dst_w) {
  for (int dx = 0; dx < dst_w; ++dx) {
    uint32_t sx =
        (uint32_t)(((uint32_t)dx * (uint32_t)src_w + (uint32_t)dst_w / 2) /
                   (uint32_t)dst_w);
    if (sx >= (uint32_t)src_w)
      sx = (uint32_t)src_w - 1;
    s_col_map[dx] = sx;
  }
  s_col_map_width = dst_w;
  s_col_map_src_w = src_w;
}

int tcmips_sdl2_vram_init(int logical_w, int logical_h) {
  // The logical surface can be up to 1920x1080 and is scaled down to the
  // physical console resolution on present.
  if (logical_w <= 0 || logical_h <= 0 ||
      logical_w > TCMIPS_SDL2_MAX_LOGICAL_WIDTH ||
      logical_h > TCMIPS_SDL2_MAX_LOGICAL_HEIGHT) {
    SDL_SetError("SDL2: invalid logical size %dx%d (max %dx%d)", logical_w,
                 logical_h, TCMIPS_SDL2_MAX_LOGICAL_WIDTH,
                 TCMIPS_SDL2_MAX_LOGICAL_HEIGHT);
    return -1;
  }

  /* Round the requested width up to a multiple of 4, as the TCMIPS console
     requires, then derive the 4:3 hardware resolution. */
  int req_w = (logical_w + 3) & ~3;
  if (req_w > TCM_SCREEN_MAX_W)
    req_w = TCM_SCREEN_MAX_W;
  int n = (req_w / 4) - 1;
  if (n < 0)
    n = 0;
  int hw_w = 4 * (n + 1);
  int hw_h = 3 * (n + 1);

  /* The platform returns the physical framebuffer base address; we trust
     that return value (not the compile-time macro) so the driver stays
     correct even if the runtime layout changes. */
  void *fb = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, (uint32_t)hw_w);
  if (!fb) {
    s_initialized = 0;
    return -1;
  }
  tcm_pixel_console_clear();

  s_fb_addr = fb;
  s_hw_w = hw_w;
  s_hw_h = hw_h;
  s_initialized = 1;
  return 0;
}

void tcmips_sdl2_vram_get_size(int *w, int *h) {
  if (w)
    *w = s_hw_w;
  if (h)
    *h = s_hw_h;
}

void tcmips_sdl2_vram_submit(const void *pixels, int logical_w, int logical_h,
                             int logical_pitch) {
  if (!s_initialized || !pixels || logical_w <= 0 || logical_h <= 0)
    return;

  uint8_t *fb = (uint8_t *)s_fb_addr;
  const uint8_t *src = (const uint8_t *)pixels;

  if (s_hw_w == logical_w && s_hw_h == logical_h) {
    /* Hot path: 1:1 row copy. */
    for (int y = 0; y < logical_h; ++y) {
      SDL_memcpy(fb + (size_t)y * (size_t)s_hw_w * 4u,
                 src + (size_t)y * (size_t)logical_pitch,
                 (size_t)logical_w * 4u);
    }
    return;
  }

  /* Nearest-neighbour rescale, whole logical buffer -> whole physical
     screen.  Column map is rebuilt only when the source width changes. */
  if (s_col_map_width != s_hw_w || s_col_map_src_w != logical_w) {
    build_col_map(logical_w, s_hw_w);
  }

  for (int dy = 0; dy < s_hw_h; ++dy) {
    uint32_t sy =
        (uint32_t)(((uint32_t)dy * (uint32_t)logical_h + (uint32_t)s_hw_h / 2) /
                   (uint32_t)s_hw_h);
    if (sy >= (uint32_t)logical_h)
      sy = (uint32_t)logical_h - 1;
    const uint8_t *r = src + (size_t)sy * (size_t)logical_pitch;
    uint8_t *d = fb + (size_t)dy * (size_t)s_hw_w * 4u;
    for (int dx = 0; dx < s_hw_w; ++dx) {
      uint32_t sx = s_col_map[dx];
      d[dx * 4 + 0] = r[sx * 4 + 0];
      d[dx * 4 + 1] = r[sx * 4 + 1];
      d[dx * 4 + 2] = r[sx * 4 + 2];
      d[dx * 4 + 3] = r[sx * 4 + 3];
    }
  }
}

void *tcmips_sdl2_vram_info(tcmips_sdl2_vram_format_t *fmt) {
  if (fmt) {
    SDL_memset(fmt, 0, sizeof(*fmt));
    fmt->width = s_hw_w;
    fmt->height = s_hw_h;
    /* The pixel console is always CONSOLE_MODE_PIXEL_32 here. */
    fmt->bits_per_pixel = 32;
    fmt->bytes_per_pixel = 4;
    /* RGBA order with R in the low byte (host little-endian). */
    fmt->rmask = 0x000000FFu;
    fmt->gmask = 0x0000FF00u;
    fmt->bmask = 0x00FF0000u;
    fmt->amask = 0xFF000000u;
  }
  return s_fb_addr;
}

void tcmips_sdl2_vram_quit(void) {
  s_fb_addr = NULL;
  s_hw_w = 0;
  s_hw_h = 0;
  s_initialized = 0;
  s_col_map_width = 0;
  s_col_map_src_w = 0;
}

#ifdef __cplusplus
}
#endif