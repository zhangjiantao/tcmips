/* SDL compatibility layer: video / surface operations. */

#include "tcmips_sdl_internal.h"

#include <stdlib.h>
#include <string.h>

#define SDL_ALPHA_OPAQUE 255

/* Track whether the SDL1 video subsystem was initialized by this layer. */
static int tcmips_sdl_video_initialized = 0;

/* Count the number of trailing zero bits of mask. */
static int tcmips_mask_shift(Uint32 mask) {
  if (mask == 0)
    return 0;
  int s = 0;
  while ((mask & 1u) == 0u) {
    mask >>= 1;
    ++s;
  }
  return s;
}

/* Return the precision loss when promoting a channel of "mask" to 8 bits
 * (i.e. 8 - popcount(mask), clamped at 8 when mask == 0). */
static int tcmips_mask_loss(Uint32 mask) {
  if (mask == 0)
    return 8;
  int bits = 0;
  while (mask) {
    bits += (mask & 1u);
    mask >>= 1;
  }
  if (bits >= 8)
    return 0;
  return 8 - bits;
}

SDL_PixelFormat *tcmips_sdl_alloc_format(int bpp, Uint32 rmask, Uint32 gmask,
                                         Uint32 bmask, Uint32 amask) {
  SDL_PixelFormat *fmt = (SDL_PixelFormat *)SDL_malloc(sizeof(SDL_PixelFormat));
  if (!fmt)
    return NULL;
  SDL_memset(fmt, 0, sizeof(*fmt));
  fmt->palette = NULL;
  fmt->BitsPerPixel = bpp;
  fmt->BytesPerPixel = (bpp + 7) / 8;
  fmt->Rmask = rmask;
  fmt->Gmask = gmask;
  fmt->Bmask = bmask;
  fmt->Amask = amask;
  fmt->Rshift = (Uint8)tcmips_mask_shift(rmask);
  fmt->Gshift = (Uint8)tcmips_mask_shift(gmask);
  fmt->Bshift = (Uint8)tcmips_mask_shift(bmask);
  fmt->Ashift = (Uint8)tcmips_mask_shift(amask);
  fmt->Rloss = (Uint8)tcmips_mask_loss(rmask);
  fmt->Gloss = (Uint8)tcmips_mask_loss(gmask);
  fmt->Bloss = (Uint8)tcmips_mask_loss(bmask);
  fmt->Aloss = (Uint8)tcmips_mask_loss(amask);
  fmt->colorkey = 0;
  fmt->alpha = SDL_ALPHA_OPAQUE;

  /* 8-bpp palette surfaces always carry a 256-entry RGB332 palette; games
   * may override it via SDL_SetPalette / SDL_SetColors. */
  if (bpp == 8) {
    SDL_Palette *pal = (SDL_Palette *)SDL_malloc(sizeof(SDL_Palette));
    if (!pal) {
      SDL_free(fmt);
      SDL_SetError("tcmips_sdl_alloc_format: out of memory");
      return NULL;
    }
    pal->ncolors = 256;
    pal->colors = (SDL_Color *)SDL_malloc(256 * sizeof(SDL_Color));
    if (!pal->colors) {
      SDL_free(pal);
      SDL_free(fmt);
      SDL_SetError("tcmips_sdl_alloc_format: out of memory");
      return NULL;
    }
    for (int i = 0; i < 256; ++i) {
      pal->colors[i].r = (Uint8)(((i >> 5) & 0x07) * 255 / 7);
      pal->colors[i].g = (Uint8)(((i >> 2) & 0x07) * 255 / 7);
      pal->colors[i].b = (Uint8)((i & 0x03) * 255 / 3);
      pal->colors[i].unused = 0;
    }
    fmt->palette = pal;
  }
  return fmt;
}

void tcmips_sdl_free_format(SDL_PixelFormat *fmt) {
  if (!fmt)
    return;
  if (fmt->palette) {
    SDL_free(fmt->palette->colors);
    SDL_free(fmt->palette);
  }
  SDL_free(fmt);
}

/* == WM functions == */
void SDL_WM_SetCaption(const char *title, const char *icon) {
  (void)icon;
  if (g_tcmips_sdl.wm_title)
    SDL_free(g_tcmips_sdl.wm_title);
  g_tcmips_sdl.wm_title = title ? SDL_strdup(title) : NULL;
}

void SDL_WM_GetCaption(char **title, char **icon) {
  if (title)
    *title = g_tcmips_sdl.wm_title;
  if (icon)
    *icon = NULL;
}

void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask) {
  (void)icon;
  (void)mask;
}

int SDL_WM_IconifyWindow(void) { return 0; }

int SDL_WM_ToggleFullScreen(SDL_Surface *surface) {
  (void)surface;
  return 0;
}

SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode) {
  if (mode == SDL_GRAB_QUERY)
    return SDL_GRAB_OFF;
  return mode;
}

/* == Video init / quit == */
int SDL_VideoInit(const char *driver_name, Uint32 flags) {
  (void)driver_name;
  return SDL_InitSubSystem(SDL_INIT_VIDEO | flags);
}

void SDL_VideoQuit(void) {
  tcmips_sdl_vram_quit();
  tcmips_sdl_video_initialized = 0;
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

char *SDL_VideoDriverName(char *namebuf, int maxlen) {
  if (namebuf && maxlen > 0) {
    SDL_strlcpy(namebuf, "tcmips", maxlen);
  }
  return namebuf;
}

/* == Video modes == */
SDL_Surface *SDL_GetVideoSurface(void) { return g_tcmips_sdl.screen; }

/* Pick the standard RGB channel masks and bytes-per-pixel for a supported
 * logical depth.  Returns 0 on success, -1 if the depth is unsupported.
 * For 8 bpp the masks are zero and the format should be paired with a
 * software palette via the format's palette field. */
static int tcmips_sdl_pick_masks(int bpp, Uint32 *rmask, Uint32 *gmask,
                                 Uint32 *bmask, Uint32 *amask,
                                 Uint32 *bytes_per_pixel) {
  switch (bpp) {
  case 8:
    *rmask = 0u;
    *gmask = 0u;
    *bmask = 0u;
    *amask = 0u;
    *bytes_per_pixel = 1u;
    return 0;
  case 16:
    /* SDL 1.2 default: RGB565. */
    *rmask = 0xF800u;
    *gmask = 0x07E0u;
    *bmask = 0x001Fu;
    *amask = 0u;
    *bytes_per_pixel = 2u;
    return 0;
  case 24:
    /* XRGB8888-style masks over 3 bytes per pixel. */
    *rmask = 0x0000FFu;
    *gmask = 0x00FF00u;
    *bmask = 0xFF0000u;
    *amask = 0u;
    *bytes_per_pixel = 3u;
    return 0;
  case 32:
    /* RGBA8888 (host-endian on this little-endian MIPS core). */
    *rmask = 0x000000FFu;
    *gmask = 0x0000FF00u;
    *bmask = 0x00FF0000u;
    *amask = 0xFF000000u;
    *bytes_per_pixel = 4u;
    return 0;
  default:
    return -1;
  }
}

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) {
  if (width <= 0 || height <= 0) {
    SDL_SetError("SDL_SetVideoMode: invalid size %dx%d", width, height);
    return NULL;
  }

  /* The pixel console is fixed at 32 bpp RGBA.  SDL_HWPALETTE is accepted
   * for API compatibility but honoured as a software-managed palette, since
   * the underlying VRAM has no palette hardware.  bpp == 0 means "default
   * depth"* and is mapped to 32.
   */
  int logical_bpp = bpp;
  if (logical_bpp == 0)
    logical_bpp = 32;
  Uint32 rmask, gmask, bmask, amask, bpp_bytes;
  if (tcmips_sdl_pick_masks(logical_bpp, &rmask, &gmask, &bmask, &amask,
                            &bpp_bytes) != 0) {
    SDL_SetError("SDL_SetVideoMode: unsupported colour depth %d", bpp);
    return NULL;
  }

  /* Make sure the video subsystem is initialized. */
  if (!tcmips_sdl_video_initialized) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
      return NULL;
    tcmips_sdl_video_initialized = 1;
  }

  /* Drop any previous screen surface. */
  if (g_tcmips_sdl.screen) {
    if (!(g_tcmips_sdl.screen->flags & SDL_PREALLOC) &&
        g_tcmips_sdl.screen->pixels) {
      SDL_free(g_tcmips_sdl.screen->pixels);
    }
    tcmips_sdl_free_format(g_tcmips_sdl.screen->format);
    SDL_free(g_tcmips_sdl.screen);
    g_tcmips_sdl.screen = NULL;
  }

  /* Initialize the physical pixel console (directly on the TCMIPS hardware
     console, no SDL2 window involved).  Its resolution is hardware-rounded
     to the 4:3 step and is NOT what we expose to the SDL1 caller. */
  if (tcmips_sdl_vram_init(&width, &height) != 0) {
    return NULL;
  }

  /* The SDL1 game draws into a logically 8/16/24/32-bpp surface whose
     dimensions match what SDL_SetVideoMode was called with; the VRAM driver
     expands each logical pixel to the hardware 32-bpp RGBA framebuffer at
     submit time. */
  Uint64 pitch = ((Uint64)width * (Uint64)bpp_bytes + 3u) & ~(Uint64)3u;
  if (pitch == 0 || pitch > 0xFFFFu) {
    SDL_SetError("SDL_SetVideoMode: computed pitch out of range");
    tcmips_sdl_vram_quit();
    return NULL;
  }
  Uint64 buf_size = pitch * (Uint64)height;
  if (buf_size == 0 || buf_size > ((Uint64)1 << 26)) {
    SDL_SetError("SDL_SetVideoMode: logical surface size out of range");
    tcmips_sdl_vram_quit();
    return NULL;
  }

  void *pixels = SDL_malloc((size_t)buf_size);
  if (!pixels) {
    SDL_SetError("SDL_SetVideoMode: out of memory");
    tcmips_sdl_vram_quit();
    return NULL;
  }
  SDL_memset(pixels, 0, (size_t)buf_size);

  SDL_Surface *surf = (SDL_Surface *)SDL_malloc(sizeof(SDL_Surface));
  if (!surf) {
    SDL_SetError("SDL_SetVideoMode: out of memory");
    SDL_free(pixels);
    tcmips_sdl_vram_quit();
    return NULL;
  }
  SDL_memset(surf, 0, sizeof(*surf));

  surf->flags = flags;
  surf->format =
      tcmips_sdl_alloc_format((int)bpp_bytes * 8, rmask, gmask, bmask, amask);
  if (!surf->format) {
    SDL_SetError("SDL_SetVideoMode: %s", SDL_GetError());
    SDL_free(pixels);
    SDL_free(surf);
    tcmips_sdl_vram_quit();
    return NULL;
  }
  surf->w = width;
  surf->h = height;
  surf->pitch = (Uint16)pitch;
  surf->pixels = pixels;
  surf->clip_rect.x = 0;
  surf->clip_rect.y = 0;
  surf->clip_rect.w = (Uint16)width;
  surf->clip_rect.h = (Uint16)height;
  surf->refcount = 1;
  surf->locked = 0;

  g_tcmips_sdl.screen = surf;
  return surf;
}

int SDL_VideoModeOK(int width, int height, int bpp, Uint32 flags) {
  (void)flags;
  if (width <= 0 || height <= 0)
    return 0;
  Uint32 rmask, gmask, bmask, amask, bpp_bytes;
  if (tcmips_sdl_pick_masks(bpp == 0 ? 32 : bpp, &rmask, &gmask, &bmask, &amask,
                            &bpp_bytes) != 0) {
    return 0;
  }
  return (int)bpp_bytes * 8;
}

/* SDL_ListModes returns a NULL-terminated list. We report only one mode. */
static SDL_Rect tcmips_sdl_mode;
static SDL_Rect *tcmips_sdl_modelist[2];

SDL_Rect **SDL_ListModes(SDL_PixelFormat *format, Uint32 flags) {
  (void)format;
  (void)flags;
  if (!g_tcmips_sdl.screen)
    return (SDL_Rect **)-1;
  tcmips_sdl_mode.x = 0;
  tcmips_sdl_mode.y = 0;
  tcmips_sdl_mode.w = (Uint16)g_tcmips_sdl.screen->w;
  tcmips_sdl_mode.h = (Uint16)g_tcmips_sdl.screen->h;
  tcmips_sdl_modelist[0] = &tcmips_sdl_mode;
  tcmips_sdl_modelist[1] = NULL;
  return tcmips_sdl_modelist;
}

/* == Screen update == */
int SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w,
                   Uint32 h) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  if (screen && screen == g_tcmips_sdl.screen) {
    /* Present the full logical framebuffer directly to the hardware VRAM.
     * The VRAM driver converts the screen's pixel format to hardware
     * RGBA if necessary. */
    tcmips_sdl_vram_submit(screen);
  }
  return 0;
}

int SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) {
  (void)numrects;
  (void)rects;
  return SDL_UpdateRect(screen, 0, 0, 0, 0);
}

int SDL_Flip(SDL_Surface *screen) { return SDL_UpdateRect(screen, 0, 0, 0, 0); }

/* == Locking == */
int SDL_MustLock(SDL_Surface *surface) {
  (void)surface;
  return 0;
}

int SDL_LockSurface(SDL_Surface *surface) {
  if (!surface)
    return -1;
  surface->locked++;
  return 0;
}

void SDL_UnlockSurface(SDL_Surface *surface) {
  if (!surface)
    return;
  if (surface->locked > 0)
    surface->locked--;
}

/* == Pixel format helpers == */

/* Promote a sub-byte channel to the full 8-bit range by replicating the
 * low "loss" bits (matches the VRAM converter back in sdl_vram.c).  When the
 * channel mask is 0, the channel does not exist and a value of 0 is returned.
 */
static Uint8 tcm_channel_to8(Uint32 raw, Uint32 mask, Uint8 shift, Uint8 loss) {
  if (!mask)
    return 0;
  Uint32 v = (raw & mask) >> shift;
  if (loss == 0)
    return (Uint8)v;
  return (Uint8)((v << loss) | (v >> (8 - loss)));
}

/* Fold an 8-bit channel into the format's sub-byte field.  When the channel
 * mask is 0 (format does not store that channel) nothing is contributed. */
static Uint32 tcm_map_channel(Uint8 value, Uint32 mask, Uint8 shift,
                              Uint8 loss) {
  if (!mask)
    return 0;
  return ((Uint32)value >> loss) << shift;
}

Uint32 SDL_MapRGB(SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b) {
  if (!format)
    return 0;
  if (format->BytesPerPixel == 1 && format->palette) {
    /* Nearest palette entry lookup -- not optimized. */
    SDL_Palette *pal = format->palette;
    int best = 0;
    int best_dist = 0x7FFFFFFF;
    for (int i = 0; i < pal->ncolors; ++i) {
      int dr = (int)r - pal->colors[i].r;
      int dg = (int)g - pal->colors[i].g;
      int db = (int)b - pal->colors[i].b;
      int dist = dr * dr + dg * dg + db * db;
      if (dist < best_dist) {
        best_dist = dist;
        best = i;
      }
    }
    return (Uint32)best;
  }
  /* Per the official SDL contract, SDL_MapRGB maps an RGB triple to an
   * *opaque* pixel value: it must behave exactly like SDL_MapRGBA(format,
   * r, g, b, SDL_ALPHA_OPAQUE).  For formats with an alpha channel this
   * fills the alpha bits with 255 (never 0); for formats without one
   * (Amask == 0) the alpha term contributes nothing, which is also correct. */
  return SDL_MapRGBA(format, r, g, b, SDL_ALPHA_OPAQUE);
}

Uint32 SDL_MapRGBA(SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b,
                   Uint8 a) {
  if (!format)
    return 0;
  if (format->BytesPerPixel == 1 && format->palette) {
    return SDL_MapRGB(format, r, g, b);
  }
  return tcm_map_channel(r, format->Rmask, format->Rshift, format->Rloss) |
         tcm_map_channel(g, format->Gmask, format->Gshift, format->Gloss) |
         tcm_map_channel(b, format->Bmask, format->Bshift, format->Bloss) |
         tcm_map_channel(a, format->Amask, format->Ashift, format->Aloss);
}

void SDL_GetRGB(Uint32 pixel, SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g,
                Uint8 *b) {
  if (!fmt)
    return;
  if (fmt->BytesPerPixel == 1 && fmt->palette &&
      pixel < (Uint32)fmt->palette->ncolors) {
    /* 8‑bpp indexed surface – look up the palette entry. */
    SDL_Color *c = &fmt->palette->colors[pixel];
    *r = c->r;
    *g = c->g;
    *b = c->b;
    return;
  }
  *r = tcm_channel_to8(pixel, fmt->Rmask, fmt->Rshift, fmt->Rloss);
  *g = tcm_channel_to8(pixel, fmt->Gmask, fmt->Gshift, fmt->Gloss);
  *b = tcm_channel_to8(pixel, fmt->Bmask, fmt->Bshift, fmt->Bloss);
}

void SDL_GetRGBA(Uint32 pixel, SDL_PixelFormat *fmt, Uint8 *r, Uint8 *g,
                 Uint8 *b, Uint8 *a) {
  if (!fmt)
    return;
  if (fmt->BytesPerPixel == 1 && fmt->palette &&
      pixel < (Uint32)fmt->palette->ncolors) {
    /* 8‑bpp indexed surface – look up the palette entry. */
    SDL_Color *c = &fmt->palette->colors[pixel];
    *r = c->r;
    *g = c->g;
    *b = c->b;
    *a = 255;
    return;
  }
  *r = tcm_channel_to8(pixel, fmt->Rmask, fmt->Rshift, fmt->Rloss);
  *g = tcm_channel_to8(pixel, fmt->Gmask, fmt->Gshift, fmt->Gloss);
  *b = tcm_channel_to8(pixel, fmt->Bmask, fmt->Bshift, fmt->Bloss);
  /* SDL1 semantics: a surface with no alpha channel (Amask == 0) is treated
   * as fully opaque, so GetRGBA must report alpha = 255 in that case. */
  *a = fmt->Amask ? tcm_channel_to8(pixel, fmt->Amask, fmt->Ashift, fmt->Aloss)
                  : 255;
}

/* == Clip rects == */
int SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect) {
  if (!surface)
    return 0;
  if (rect) {
    surface->clip_rect = *rect;
  } else {
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = (Uint16)surface->w;
    surface->clip_rect.h = (Uint16)surface->h;
  }
  return 1;
}

void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect) {
  if (!surface || !rect)
    return;
  *rect = surface->clip_rect;
}

/* == Color key / alpha / palette == */
int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key) {
  if (!surface || !surface->format)
    return -1;
  surface->format->colorkey = key;
  if (flag & SDL_SRCCOLORKEY) {
    surface->flags |= SDL_SRCCOLORKEY;
  } else {
    surface->flags &= ~SDL_SRCCOLORKEY;
  }
  return 0;
}

int SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha) {
  if (!surface || !surface->format)
    return -1;
  surface->format->alpha = alpha;
  if (flag & SDL_SRCALPHA) {
    surface->flags |= SDL_SRCALPHA;
  } else {
    surface->flags &= ~SDL_SRCALPHA;
  }
  return 0;
}

int SDL_SetColors(SDL_Surface *surface, SDL_Color *colors, int firstcolor,
                  int ncolors) {
  if (!surface || !surface->format || !surface->format->palette)
    return 0;
  SDL_Palette *pal = surface->format->palette;
  if (firstcolor < 0 || firstcolor + ncolors > pal->ncolors)
    return 0;
  for (int i = 0; i < ncolors; ++i) {
    pal->colors[firstcolor + i] = colors[i];
  }
  return 1;
}

int SDL_SetPalette(SDL_Surface *surface, int flags, const SDL_Color *colors,
                   int firstcolor, int ncolors) {
  (void)flags;
  if (!surface || !surface->format || !colors || ncolors <= 0)
    return 0;
  if (!surface->format->palette) {
    surface->format->palette = (SDL_Palette *)SDL_malloc(sizeof(SDL_Palette));
    if (!surface->format->palette)
      return 0;
    surface->format->palette->ncolors = 256;
    surface->format->palette->colors =
        (SDL_Color *)SDL_malloc(256 * sizeof(SDL_Color));
    if (!surface->format->palette->colors) {
      SDL_free(surface->format->palette);
      surface->format->palette = NULL;
      return 0;
    }
  }
  int ret = SDL_SetColors(surface, (SDL_Color *)colors, firstcolor, ncolors);
  if ((surface == g_tcmips_sdl.screen) || (surface->flags & SDL_HWSURFACE))
    SDL_UpdateRect(g_tcmips_sdl.screen, 0, 0, 0, 0);

  return ret;
}

/* == OpenGL stubs == */
int SDL_GL_LoadLibrary(const char *path) {
  (void)path;
  return -1;
}
void *SDL_GL_GetProcAddress(const char *proc) {
  (void)proc;
  return NULL;
}
int SDL_GL_GetAttribute(SDL_GLattr attr, int *value) {
  (void)attr;
  (void)value;
  return -1;
}
int SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
  (void)attr;
  (void)value;
  return -1;
}
void SDL_GL_SwapBuffers(void) {}
