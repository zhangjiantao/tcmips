#include "common.h"
#include "util.h"

#include <NDL.h>
#include <assert.h>
#include <sdl-video.h>
#include <stdlib.h>
#include <string.h>

#include "dev/syscall.h"

void SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, uint32_t color) {
  // UTIL_LogOutput(LOGLEVEL_INFO, "%s\n", __func__);

  if (!dst)
    return;
  int w = dstrect ? dstrect->w : dst->w;
  int h = dstrect ? dstrect->h : dst->h;

  if (w == dst->w && h == dst->h) {
    memset(dst->pixels, (uint8_t)color, dst->pitch * dst->h);
    return;
  }

  int x = dstrect ? dstrect->x : 0;
  int y = dstrect ? dstrect->y : 0;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > dst->w)
    w = dst->w - x;
  if (y + h > dst->h)
    h = dst->h - y;
  if (w <= 0 || h <= 0)
    return;

  int bpp = dst->format->BytesPerPixel;
  uint8_t *dst_pixels = (uint8_t *)dst->pixels;

  for (int i = 0; i < h; i++) {
    uint8_t *d_row = dst_pixels + (y + i) * dst->pitch + x * bpp;
    if (bpp == 4) {
      uint32_t *d_row32 = (uint32_t *)d_row;
      for (int j = 0; j < w; j++)
        d_row32[j] = color;
    } else if (bpp == 1) {
      memset(d_row, (uint8_t)color, w);
    }
  }
}

void SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst,
                     SDL_Rect *dstrect) {
  assert(dst && src);

  int sw = srcrect ? srcrect->w : src->w;
  int sh = srcrect ? srcrect->h : src->h;

  int s_bpp = src->format->BytesPerPixel;
  int d_bpp = dst->format->BytesPerPixel;

  if (sw == 320 && sh == 200 && src->w == 320 && dst->w == 320 &&
      s_bpp == d_bpp) {
    memcpy(dst->pixels, src->pixels, 320 * 200 * s_bpp);
    return;
  }

  int sx = srcrect ? srcrect->x : 0;
  int sy = srcrect ? srcrect->y : 0;
  int dx = dstrect ? dstrect->x : 0;
  int dy = dstrect ? dstrect->y : 0;

  if (dx < 0) {
    sw += dx;
    sx -= dx;
    dx = 0;
  }
  if (dy < 0) {
    sh += dy;
    sy -= dy;
    dy = 0;
  }
  if (sx < 0) {
    sw += sx;
    dx -= sx;
    sx = 0;
  }
  if (sy < 0) {
    sh += sy;
    dy -= sy;
    sy = 0;
  }

  if (dx + sw > dst->w)
    sw = dst->w - dx;
  if (dy + sh > dst->h)
    sh = dst->h - dy;
  if (sx + sw > src->w)
    sw = src->w - sx;
  if (sy + sh > src->h)
    sh = src->h - sy;
  if (sw <= 0 || sh <= 0)
    return;

  uint8_t *src_pixels = (uint8_t *)src->pixels;
  uint8_t *dst_pixels = (uint8_t *)dst->pixels;

  if (s_bpp == 1 && d_bpp == 1) {
    for (int i = 0; i < sh; i++) {
      uint8_t *s_row = src_pixels + (sy + i) * src->pitch + sx;
      uint8_t *d_row = dst_pixels + (dy + i) * dst->pitch + dx;
      for (int j = 0; j < sw; j++) {
        if (s_row[j] != 0) {
          d_row[j] = s_row[j];
        }
      }
    }
  } else if (s_bpp == 4 && d_bpp == 4) {
    for (int i = 0; i < sh; i++) {
      uint32_t *s_row = (uint32_t *)(src_pixels + (sy + i) * src->pitch) + sx;
      uint32_t *d_row = (uint32_t *)(dst_pixels + (dy + i) * dst->pitch) + dx;
      for (int j = 0; j < sw; j++) {
        if ((s_row[j] & 0x00FFFFFF) != 0) {
          d_row[j] = s_row[j];
        }
      }
    }
  } else if (s_bpp == 1 && d_bpp == 4) {
    assert(src->format->palette);
    SDL_Color *colors = src->format->palette->colors;

    int is_background = (src->w >= 320 && src->h >= 200);

    for (int i = 0; i < sh; i++) {
      uint8_t *s_row = src_pixels + (sy + i) * src->pitch + sx;
      uint32_t *d_row = (uint32_t *)(dst_pixels + (dy + i) * dst->pitch) + dx;
      for (int j = 0; j < sw; j++) {
        uint8_t index = s_row[j];
        if (is_background || index != 0) {
          d_row[j] = colors[index].val;
        }
      }
    }
  }
}

void SDL_UpdateRect(SDL_Surface *s, int x, int y, int w, int h) {
  extern SDL_Surface *gpScreen;
  NDL_DrawRect(gpScreen->pixels, 0, 0, 320, 200);
}

// APIs below are already implemented.

static inline int maskToShift(uint32_t mask) {
  switch (mask) {
  case 0x000000ff:
    return 0;
  case 0x0000ff00:
    return 8;
  case 0x00ff0000:
    return 16;
  case 0xff000000:
    return 24;
  case 0x00000000:
    return 24; // hack
  default:
    assert(0);
  }
  __builtin_unreachable();
}

SDL_Surface *SDL_CreateRGBSurface(uint32_t flags, int width, int height,
                                  int depth, uint32_t Rmask, uint32_t Gmask,
                                  uint32_t Bmask, uint32_t Amask) {
  assert(depth == 8 || depth == 32);
  SDL_Surface *s = malloc(sizeof(SDL_Surface));
  assert(s);
  s->flags = flags;
  s->format = malloc(sizeof(SDL_PixelFormat));
  assert(s->format);
  if (depth == 8) {
    s->format->palette = malloc(sizeof(SDL_Palette));
    assert(s->format->palette);
    s->format->palette->colors = malloc(sizeof(SDL_Color) * 256);
    assert(s->format->palette->colors);
    memset(s->format->palette->colors, 0, sizeof(SDL_Color) * 256);
    s->format->palette->ncolors = 256;
  } else {
    s->format->palette = NULL;
    s->format->Rmask = Rmask;
    s->format->Rshift = maskToShift(Rmask);
    s->format->Rloss = 0;
    s->format->Gmask = Gmask;
    s->format->Gshift = maskToShift(Gmask);
    s->format->Gloss = 0;
    s->format->Bmask = Bmask;
    s->format->Bshift = maskToShift(Bmask);
    s->format->Bloss = 0;
    s->format->Amask = Amask;
    s->format->Ashift = maskToShift(Amask);
    s->format->Aloss = 0;
  }

  s->format->BitsPerPixel = depth;
  s->format->BytesPerPixel = depth / 8;

  s->w = width;
  s->h = height;
  s->pitch = width * depth / 8;
  assert(s->pitch == width * s->format->BytesPerPixel);

  if (!(flags & SDL_PREALLOC)) {
    s->pixels = malloc(s->pitch * height);
    assert(s->pixels);
  }

  return s;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height,
                                      int depth, int pitch, uint32_t Rmask,
                                      uint32_t Gmask, uint32_t Bmask,
                                      uint32_t Amask) {
  SDL_Surface *s = SDL_CreateRGBSurface(SDL_PREALLOC, width, height, depth,
                                        Rmask, Gmask, Bmask, Amask);
  assert(pitch == s->pitch);
  s->pixels = pixels;
  return s;
}

void SDL_FreeSurface(SDL_Surface *s) {
  if (s != NULL) {
    if (s->format != NULL) {
      if (s->format->palette != NULL) {
        if (s->format->palette->colors != NULL)
          free(s->format->palette->colors);
        free(s->format->palette);
      }
      free(s->format);
    }
    if (s->pixels != NULL && !(s->flags & SDL_PREALLOC))
      free(s->pixels);
    free(s);
  }
}

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags) {
  if (flags & SDL_HWSURFACE)
    NDL_OpenCanvas(&width, &height);
  return SDL_CreateRGBSurface(flags, width, height, bpp, DEFAULT_RMASK,
                              DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);
}

void SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst,
                     SDL_Rect *dstrect) {
  assert(src && dst);
  assert(dst->format->BitsPerPixel == src->format->BitsPerPixel);
  assert(dst->format->BitsPerPixel == 8);

  int x = (srcrect == NULL ? 0 : srcrect->x);
  int y = (srcrect == NULL ? 0 : srcrect->y);
  int w = (srcrect == NULL ? src->w : srcrect->w);
  int h = (srcrect == NULL ? src->h : srcrect->h);

  assert(dstrect);
  if (w == dstrect->w && h == dstrect->h) {
    /* The source rectangle and the destination rectangle
     * are of the same size. If that is the case, there
     * is no need to stretch, just copy. */
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_BlitSurface(src, &rect, dst, dstrect);
  } else {
    // assert(0);
  }
}

void SDL_SetPalette(SDL_Surface *s, int flags, SDL_Color *colors,
                    int firstcolor, int ncolors) {
  assert(s);
  assert(s->format);
  assert(s->format->palette);
  assert(firstcolor == 0);

#ifdef Q256_HARD_PALETTE
  for (int i = 0; i < ncolors; i++) {
    uint8_t r = colors[i].r >> 5;
    uint8_t g = colors[i].g >> 5;
    uint8_t b = colors[i].b >> 6;
    uint8_t rgb332 = (r << 5) | (g << 2) | b;
    tcm_syscall_palette_init(i, rgb332);
  }
#endif

  s->format->palette->ncolors = ncolors;
  memcpy(s->format->palette->colors, colors, sizeof(SDL_Color) * ncolors);

  if (s->flags & SDL_HWSURFACE) {
    assert(ncolors == 256);
    for (int i = 0; i < ncolors; i++) {
      uint8_t r = colors[i].r;
      uint8_t g = colors[i].g;
      uint8_t b = colors[i].b;
    }
    SDL_UpdateRect(s, 0, 0, 0, 0);
  }
}

static void ConvertPixelsARGB_ABGR(void *dst, void *src, int len) {
  int i;
  uint8_t (*pdst)[4] = dst;
  uint8_t (*psrc)[4] = src;
  union {
    uint8_t val8[4];
    uint32_t val32;
  } tmp;
  int first = len & ~0xf;
  for (i = 0; i < first; i += 16) {
#define macro(i)                                                               \
  tmp.val32 = *((uint32_t *)psrc[i]);                                          \
  *((uint32_t *)pdst[i]) = tmp.val32;                                          \
  pdst[i][0] = tmp.val8[2];                                                    \
  pdst[i][2] = tmp.val8[0];

    macro(i + 0);
    macro(i + 1);
    macro(i + 2);
    macro(i + 3);
    macro(i + 4);
    macro(i + 5);
    macro(i + 6);
    macro(i + 7);
    macro(i + 8);
    macro(i + 9);
    macro(i + 10);
    macro(i + 11);
    macro(i + 12);
    macro(i + 13);
    macro(i + 14);
    macro(i + 15);
  }

  for (; i < len; i++) {
    macro(i);
  }
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt,
                                uint32_t flags) {
  assert(src->format->BitsPerPixel == 32);
  assert(src->w * src->format->BytesPerPixel == src->pitch);
  assert(src->format->BitsPerPixel == fmt->BitsPerPixel);

  SDL_Surface *ret =
      SDL_CreateRGBSurface(flags, src->w, src->h, fmt->BitsPerPixel, fmt->Rmask,
                           fmt->Gmask, fmt->Bmask, fmt->Amask);

  assert(fmt->Gmask == src->format->Gmask);
  assert(fmt->Amask == 0 || src->format->Amask == 0 ||
         (fmt->Amask == src->format->Amask));
  ConvertPixelsARGB_ABGR(ret->pixels, src->pixels, src->w * src->h);

  return ret;
}

uint32_t SDL_MapRGBA(SDL_PixelFormat *fmt, uint8_t r, uint8_t g, uint8_t b,
                     uint8_t a) {
  assert(fmt->BytesPerPixel == 4);
  uint32_t p = (r << fmt->Rshift) | (g << fmt->Gshift) | (b << fmt->Bshift);
  if (fmt->Amask)
    p |= (a << fmt->Ashift);
  return p;
}

int SDL_LockSurface(SDL_Surface *s) { return 0; }

void SDL_UnlockSurface(SDL_Surface *s) {}
