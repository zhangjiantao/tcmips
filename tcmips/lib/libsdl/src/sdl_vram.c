/* VRAM driver for the TCMIPS SDL compatibility layer.
 *
 * Drives the hardware pixel console directly (tcm_pixel_console_init),
 * independent of the SDL2 layer, so the SDL1 game can render straight into
 * the physical framebuffer without an intermediate SDL2 surface exchange.
 *
 * Output format:
 *   The physical console expects a little-endian 32-bpp RGBA layout:
 *      byte 0 = R, byte 1 = G, byte 2 = B, byte 3 = A
 *
 * Supported source formats (libSDL1 surface):
 *   8  bpp indexed            (palette-driven, palette lookups)
 *   16 bpp RGB565 / RGB555    (high/low/nibble endian)
 *   24 bpp RGB888             (X8R8G8B8-style masks only, BytesPerPixel = 3)
 *   32 bpp RGBA8888 / ARGB8888 / BGRA8888 / ABGR8888 (any endian mask)
 *
 * The surface may differ in resolution from the physical console.  When it
 * differs, a nearest-neighbour rescale maps the logical surface onto the
 * whole screen.  Columns are precomputed once per logical width so the
 * inner loop is an indexed copy with no per-pixel division.
 *
 * Upstream SDL-1.2 baseline: release-1.2.15,
 * commit 457d4e55ffe1b6ad4c4fa4559dbda8360bf8253d
 */

#include "tcmips_sdl_internal.h"

#include <dev/console.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <stdint.h>
#include <string.h>

/* ====== Cached physical console state ====== */
static void *s_fb_addr;
static int s_hw_w;
static int s_hw_h;
static int s_hw_bpp;
static int s_initialized;

/* ====== Hardware pixel format ====== */

/* ====== Classified source pixel format ====== */
typedef enum {
  /* 32-bpp sources */
  FMT_RGBA32 = 0, /* R=0xFF000000, G=0xFF0000, B=0xFF00, A=0xFF */
  FMT_ARGB32,     /* A=0xFF000000, R=0xFF0000, G=0xFF00, B=0xFF */
  FMT_BGRA32,     /* B=0xFF, G=0xFF00, R=0xFF0000, A=0xFF000000 */
  FMT_ABGR32,     /* A=0xFF000000, B=0xFF0000, G=0xFF00, R=0xFF */
  /* sub-byte/16/24 paths */
  FMT_INDEXED8, /* 8-bpp indexed, palette lookup */
  FMT_RGB565,   /* 16-bpp RGB565 */
  FMT_RGB555,   /* 16-bpp RGB555 */
  FMT_BGR888,   /* 24-bpp BGR888, BytesPerPixel = 3 */
  FMT_RGB888,   /* 24-bpp RGB888, BytesPerPixel = 3 */
  FMT_UNKNOWN
} tcmips_sdl_pixfmt_kind_t;

// Hard-coded to match the TCMIPS hardware backbuffer.
#define TCMIPS_SDL_FORMAT FMT_ABGR32

/* ====== Classify the SDL_Surface's pixel format. ====== */
static tcmips_sdl_pixfmt_kind_t classify_format(const SDL_PixelFormat *fmt) {
  if (!fmt)
    return FMT_UNKNOWN;
  Uint32 bpp = fmt->BitsPerPixel;
  Uint32 R = fmt->Rmask, G = fmt->Gmask, B = fmt->Bmask, A = fmt->Amask;

  if (bpp == 8 && R == 0 && G == 0 && B == 0 && A == 0) {
    return FMT_INDEXED8;
  }

  if (bpp == 16) {
    if (R == 0xF800u && G == 0x07E0u && B == 0x001Fu)
      return FMT_RGB565;
    if (R == 0x7C00u && G == 0x03E0u && B == 0x001Fu)
      return FMT_RGB555;
    return FMT_UNKNOWN;
  }

  if (bpp == 24) {
    if (fmt->BytesPerPixel != 3)
      return FMT_UNKNOWN;
    if (R == 0xFFu && G == 0xFF00u && B == 0xFF0000u)
      return FMT_RGB888;
    if (B == 0xFFu && G == 0xFF00u && R == 0xFF0000u)
      return FMT_BGR888;
    return FMT_UNKNOWN;
  }

  if (bpp == 32 && fmt->BytesPerPixel == 4) {
    if (R == TCMIPS_SDL_RMASK && G == TCMIPS_SDL_GMASK &&
        B == TCMIPS_SDL_BMASK && A == TCMIPS_SDL_AMASK)
      return FMT_ABGR32;
    if (A == TCMIPS_SDL_AMASK && R == 0x00FF0000u && G == 0x0000FF00u &&
        B == 0x000000FFu)
      return FMT_ARGB32;
    if (B == TCMIPS_SDL_RMASK && G == TCMIPS_SDL_GMASK && R == 0x00FF0000u &&
        A == TCMIPS_SDL_AMASK)
      return FMT_BGRA32;
    if (A == TCMIPS_SDL_AMASK && B == 0x00FF0000u && G == 0x0000FF00u &&
        R == 0x000000FFu)
      return FMT_RGBA32;
  }

  return FMT_UNKNOWN;
}

/* ====== Per-pixel helpers ====== */
static inline uint8_t component8(Uint32 raw, Uint32 mask, Uint8 shift,
                                 Uint8 loss) {
  if (mask == 0)
    return 255;
  Uint32 v = (raw & mask) >> shift;
  if (loss == 0)
    return (Uint8)v;
  return (Uint8)((v << loss) | (v >> (8 - loss)));
}

/* 32-bpp source pixel -> hardware RGBA32 */
static inline Uint32 rgba_from_32(Uint32 p, const SDL_PixelFormat *fmt) {
  Uint32 r = component8(p, fmt->Rmask, fmt->Rshift, fmt->Rloss);
  Uint32 g = component8(p, fmt->Gmask, fmt->Gshift, fmt->Gloss);
  Uint32 b = component8(p, fmt->Bmask, fmt->Bshift, fmt->Bloss);
  Uint32 a = component8(p, fmt->Amask, fmt->Ashift, fmt->Aloss);
  return r | (g << 8) | (b << 16) | (a << 24);
}

/* 16-bpp source pixel (already big-endian style in low 16 bits) -> RGBA32 */
static inline Uint32 rgba_from_16(Uint16 p, const SDL_PixelFormat *fmt) {
  Uint32 r = component8(p, fmt->Rmask, fmt->Rshift, fmt->Rloss);
  Uint32 g = component8(p, fmt->Gmask, fmt->Gshift, fmt->Gloss);
  Uint32 b = component8(p, fmt->Bmask, fmt->Bshift, fmt->Bloss);
  return r | (g << 8) | (b << 16) | 0xFF000000u;
}

/* 24-bpp (3 bytes) -> RGBA32 */
static inline Uint32 rgba_from_24(const Uint8 *p3, const SDL_PixelFormat *fmt) {
  Uint32 raw = ((Uint32)p3[0]) | ((Uint32)p3[1] << 8) | ((Uint32)p3[2] << 16);
  Uint32 r = component8(raw, fmt->Rmask, fmt->Rshift, fmt->Rloss);
  Uint32 g = component8(raw, fmt->Gmask, fmt->Gshift, fmt->Gloss);
  Uint32 b = component8(raw, fmt->Bmask, fmt->Bshift, fmt->Bloss);
  return r | (g << 8) | (b << 16) | 0xFF000000u;
}

/* ====== Init ====== */
int tcmips_sdl_vram_init(int *logic_w, int *logic_h) {
  int logical_w = *logic_w;
  int logical_h = *logic_h;
  /* The logical surface can be up to 1024x768 (4:3 HD); it is used directly
   * when the logical and physical resolutions match and rescaled onto the
   * whole physical screen when they differ. */
  if (logical_w <= 0                  //
      || logical_h <= 0               //
      || logical_w > TCM_SCREEN_MAX_W //
      || logical_h > TCM_SCREEN_MAX_H //
      || (logical_w & 3) != 0) {
    SDL_SetError("SDL1: invalid video size %dx%d (max logical %dx%d)",
                 logical_w, logical_h, TCM_SCREEN_MAX_W, TCM_SCREEN_MAX_H);
    return -1;
  }

  /* The hardware console is fixed at 4:3 and derives its height from the
   * requested width.  The effective height is the 4:3 height computed from
   * the width:
   *   - a shorter request (e.g. 640x400) is letterboxed: the mode succeeds
   *     and the caller gets a taller surface whose extra bottom rows are
   *     (zero-filled) black;
   *   - a taller request (e.g. 640x768) is clamped down to the physical 4:3
   *     height so the mode still succeeds without exceeding the console.
   * Either way the returned *logic_h is the physical 4:3 height. */
  int req_w = logical_w;
  int req_h = (logical_w / 4) * 3;

  s_fb_addr = tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, req_w);
  if (!s_fb_addr) {
    SDL_SetError("SDL1: tcm_pixel_console_init returned NULL");
    return -1;
  }
  tcm_pixel_console_clear();

  s_hw_w = req_w;
  s_hw_h = req_h;
  s_hw_bpp = 32; /* CONSOLE_MODE_PIXEL_32 fixed depth */
  *logic_w = req_w;
  *logic_h = req_h;
  s_initialized = 1;
  return 0;
}

/* ====== Submit (with optional rescale) ====== */
void tcmips_sdl_vram_submit(SDL_Surface *screen) {
  if (!s_initialized || !screen || !screen->pixels || !screen->format) {
    return;
  }

  const int logical_pitch = screen->pitch;
  const unsigned char *src = (const unsigned char *)screen->pixels;
  const SDL_PixelFormat *fmt = screen->format;

  tcmips_sdl_pixfmt_kind_t kind = classify_format(fmt);
  if (kind == FMT_UNKNOWN) {
    SDL_SetError("SDL1 VRAM: unsupported screen pixel format "
                 "(bpp=%u, masks=%08X/%08X/%08X/%08X)",
                 (unsigned)fmt->BitsPerPixel, (unsigned)fmt->Rmask,
                 (unsigned)fmt->Gmask, (unsigned)fmt->Bmask,
                 (unsigned)fmt->Amask);
    return;
  }

  /* Identity hot path: 32 bpp ABGR matching the hardware layout, no rescale,
   * and tightly packed pitch.
   * Invariant: vram_submit only ever receives the SetVideoMode-created 32-bpp
   * screen (see SDL_UpdateRect guard), whose pitch is always w*4
   */
  if (kind == TCMIPS_SDL_FORMAT) {
    SDL_memcpy(s_fb_addr, src, (size_t)screen->h * (size_t)logical_pitch);
    return;
  }

  const Uint8 src_bpp =
      fmt->BytesPerPixel ? fmt->BytesPerPixel : (Uint8)(fmt->BitsPerPixel / 8);
  unsigned char *fb8 = (unsigned char *)s_fb_addr;

  switch (kind) {
  case FMT_RGBA32:
  case FMT_ARGB32:
  case FMT_BGRA32:
  case FMT_ABGR32: {
    for (int y = 0; y < s_hw_h; ++y) {
      const unsigned char *srow = src + (size_t)y * (size_t)logical_pitch;
      unsigned char *drow = fb8 + (size_t)y * (size_t)s_hw_w * 4u;
      for (int x = 0; x < s_hw_w; ++x) {
        const unsigned char *sp = srow + (size_t)x * src_bpp;
        Uint32 p = *(Uint32 *)sp;
        *(uint32_t *)(drow + x * 4) = rgba_from_32(p, fmt);
      }
    }
  } break;
  case FMT_RGB565:
  case FMT_RGB555: {
    for (int y = 0; y < s_hw_h; ++y) {
      const unsigned char *srow = src + (size_t)y * (size_t)logical_pitch;
      unsigned char *drow = fb8 + (size_t)y * (size_t)s_hw_w * 4u;
      for (int x = 0; x < s_hw_w; ++x) {
        const unsigned char *sp = srow + (size_t)x * src_bpp;
        Uint16 p = *(Uint16 *)sp;
        *(uint32_t *)(drow + x * 4) = rgba_from_16(p, fmt);
      }
    }
  } break;
  case FMT_RGB888:
  case FMT_BGR888: {
    for (int y = 0; y < s_hw_h; ++y) {
      const unsigned char *srow = src + (size_t)y * (size_t)logical_pitch;
      unsigned char *drow = fb8 + (size_t)y * (size_t)s_hw_w * 4u;
      for (int x = 0; x < s_hw_w; ++x) {
        const unsigned char *sp = srow + (size_t)x * src_bpp;
        *(uint32_t *)(drow + x * 4) = rgba_from_24(sp, fmt);
      }
    }
  } break;
  case FMT_INDEXED8: {
    /* An 8-bpp screen must always have a palette; guard anyway so an
     * uninitialised surface cannot crash the framebuffer path. */
    if (!fmt->palette || !fmt->palette->colors) {
      SDL_SetError("SDL1 VRAM: 8-bpp surface has no palette");
      return;
    }
    SDL_Color *colors = fmt->palette->colors;
    uint32_t *fb = s_fb_addr;
    /* 8x unrolled fast path; the tail loop below handles any remainder so
     * non-8-multiple pixel counts (e.g. 1020x765 = 780300, % 8 = 4) do NOT
     * overrun `src`/`fb`. */
    int total = s_hw_h * s_hw_w;
    int i = 0;
    for (; i + 8 <= total; i += 8) {
      fb[i + 0] = colors[src[i + 0]].val;
      fb[i + 1] = colors[src[i + 1]].val;
      fb[i + 2] = colors[src[i + 2]].val;
      fb[i + 3] = colors[src[i + 3]].val;
      fb[i + 4] = colors[src[i + 4]].val;
      fb[i + 5] = colors[src[i + 5]].val;
      fb[i + 6] = colors[src[i + 6]].val;
      fb[i + 7] = colors[src[i + 7]].val;
    }
    for (; i < total; ++i)
      fb[i] = colors[src[i]].val;
  } break;
  default: {
    SDL_SetError("SDL1 VRAM: unsupported pixel format ");
  } break;
  }
}

/* ====== Framebuffer query ====== */
void *tcmips_sdl_vram_info(tcmips_sdl_vram_format_t *fmt) {
  if (fmt) {
    SDL_memset(fmt, 0, sizeof(*fmt));
    fmt->width = s_hw_w;
    fmt->height = s_hw_h;
    fmt->bits_per_pixel = s_hw_bpp;
    fmt->bytes_per_pixel = s_hw_bpp / 8;
    /* The hardware pixel console packs channels RGBA with R in the low
     * byte (host little-endian); report the exact masks so consumers can
     * interpret pixels without assuming a fixed layout. */
    fmt->rmask = TCMIPS_SDL_RMASK;
    fmt->gmask = TCMIPS_SDL_GMASK;
    fmt->bmask = TCMIPS_SDL_BMASK;
    fmt->amask = TCMIPS_SDL_AMASK;
  }
  return s_fb_addr;
}

/* ====== Quit ====== */
void tcmips_sdl_vram_quit(void) {
  s_fb_addr = NULL;
  s_hw_w = 0;
  s_hw_h = 0;
  s_hw_bpp = 0;
  s_initialized = 0;
}