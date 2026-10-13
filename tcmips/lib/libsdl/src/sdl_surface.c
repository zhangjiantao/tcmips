/* SDL compatibility layer: surface creation / blit / fill. */

#include "tcmips_sdl_internal.h"

#include <stdlib.h>
#include <string.h>

/* Word-sized fill — same semantics as SDL2's SDL_memset4 but scoped to
   this translation unit so it doesn't need a cross-library symbol. */
static inline void memset4(void *dst, Uint32 val, size_t dwords) {
  size_t n = (dwords + 3) / 4;
  Uint32 *p = (Uint32 *)dst;
  if (dwords == 0)
    return;
  switch (dwords % 4) {
    case 0: do {    *p++ = val;
    case 3:         *p++ = val;
    case 2:         *p++ = val;
    case 1:         *p++ = val;
    } while (--n);
  }
}

static Uint32 surf_default_rmask(int bpp) {
  if (bpp == 32)
    return 0x000000FFu;
  if (bpp == 24)
    return 0x00FF0000u;
  if (bpp == 16)
    return 0x0000F800u;
  if (bpp == 15)
    return 0x00007C00u;
  return 0;
}
static Uint32 surf_default_gmask(int bpp) {
  if (bpp == 32)
    return 0x0000FF00u;
  if (bpp == 24)
    return 0x0000FF00u;
  if (bpp == 16)
    return 0x000007E0u;
  if (bpp == 15)
    return 0x000003E0u;
  return 0;
}
static Uint32 surf_default_bmask(int bpp) {
  if (bpp == 32)
    return 0x00FF0000u;
  if (bpp == 24)
    return 0x000000FFu;
  if (bpp == 16)
    return 0x0000001Fu;
  if (bpp == 15)
    return 0x0000001Fu;
  return 0;
}
static Uint32 surf_default_amask(int bpp) {
  if (bpp == 32)
    return 0xFF000000u;
  return 0;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                  int depth, Uint32 Rmask, Uint32 Gmask,
                                  Uint32 Bmask, Uint32 Amask) {
  if (width <= 0 || height <= 0 || depth < 8)
    return NULL;

  Sint64 pitch = ((Sint64)width * depth + 7) / 8;
  /* Align pitch to 4 bytes. */
  pitch = (pitch + 3) & ~3;
  if (pitch <= 0 || pitch > 0xFFFF)
    return NULL;
  Sint64 size = pitch * height;
  if (size <= 0 || size > (Sint64)(1 << 26))
    return NULL;

  SDL_Surface *surf = (SDL_Surface *)SDL_malloc(sizeof(SDL_Surface));
  if (!surf)
    return NULL;
  SDL_memset(surf, 0, sizeof(*surf));

  surf->pixels = SDL_malloc((size_t)size);
  if (!surf->pixels) {
    SDL_free(surf);
    return NULL;
  }
  SDL_memset(surf->pixels, 0, (size_t)size);

  if (Rmask == 0 && Gmask == 0 && Bmask == 0 && Amask == 0) {
    Rmask = surf_default_rmask(depth);
    Gmask = surf_default_gmask(depth);
    Bmask = surf_default_bmask(depth);
    Amask = surf_default_amask(depth);
  }

  surf->format = tcmips_sdl_alloc_format(depth, Rmask, Gmask, Bmask, Amask);
  if (!surf->format) {
    SDL_free(surf->pixels);
    SDL_free(surf);
    return NULL;
  }

  surf->flags = flags | SDL_SWSURFACE;
  surf->w = width;
  surf->h = height;
  surf->pitch = (Uint16)pitch;
  surf->clip_rect.x = 0;
  surf->clip_rect.y = 0;
  surf->clip_rect.w = (Uint16)width;
  surf->clip_rect.h = (Uint16)height;
  surf->refcount = 1;
  surf->locked = 0;
  return surf;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height,
                                      int depth, int pitch, Uint32 Rmask,
                                      Uint32 Gmask, Uint32 Bmask,
                                      Uint32 Amask) {
  if (!pixels || width <= 0 || height <= 0)
    return NULL;
  SDL_Surface *surf = (SDL_Surface *)SDL_malloc(sizeof(SDL_Surface));
  if (!surf)
    return NULL;
  SDL_memset(surf, 0, sizeof(*surf));

  if (Rmask == 0 && Gmask == 0 && Bmask == 0 && Amask == 0) {
    Rmask = surf_default_rmask(depth);
    Gmask = surf_default_gmask(depth);
    Bmask = surf_default_bmask(depth);
    Amask = surf_default_amask(depth);
  }

  surf->format = tcmips_sdl_alloc_format(depth, Rmask, Gmask, Bmask, Amask);
  if (!surf->format) {
    SDL_free(surf);
    return NULL;
  }
  surf->flags = SDL_PREALLOC | SDL_SWSURFACE;
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
  return surf;
}

void SDL_FreeSurface(SDL_Surface *surface) {
  if (!surface)
    return;
  if (--surface->refcount > 0)
    return;
  if (!(surface->flags & SDL_PREALLOC) && surface->pixels) {
    SDL_free(surface->pixels);
    surface->pixels = NULL;
  }
  tcmips_sdl_free_format(surface->format);
  surface->format = NULL;
  SDL_free(surface);
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt,
                                Uint32 flags) {
  if (!src || !fmt)
    return NULL;
  SDL_Surface *dst =
      SDL_CreateRGBSurface(flags, src->w, src->h, fmt->BitsPerPixel, fmt->Rmask,
                           fmt->Gmask, fmt->Bmask, fmt->Amask);
  if (!dst)
    return NULL;
  if (SDL_BlitSurface(src, NULL, dst, NULL) < 0) {
    SDL_FreeSurface(dst);
    return NULL;
  }
  return dst;
}

SDL_Surface *SDL_DisplayFormat(SDL_Surface *surface) {
  if (!g_tcmips_sdl.screen) {
    SDL_SetError("Video not initialized");
    return NULL;
  }
  return SDL_ConvertSurface(surface, g_tcmips_sdl.screen->format, 0);
}

SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *surface) {
  /* For TCMIPS, there is no alpha channel in the screen copy; behave as
   * DisplayFormat. */
  return SDL_DisplayFormat(surface);
}

static int tcmips_sdl_intersect_rect(const SDL_Rect *a, const SDL_Rect *b,
                                     SDL_Rect *out) {
  int ax2 = a->x + a->w;
  int ay2 = a->y + a->h;
  int bx2 = b->x + b->w;
  int by2 = b->y + b->h;
  int x1 = (a->x > b->x) ? a->x : b->x;
  int y1 = (a->y > b->y) ? a->y : b->y;
  int x2 = (ax2 < bx2) ? ax2 : bx2;
  int y2 = (ay2 < by2) ? ay2 : by2;
  if (x2 <= x1 || y2 <= y1)
    return 0;
  out->x = x1;
  out->y = y1;
  out->w = (Uint16)(x2 - x1);
  out->h = (Uint16)(y2 - y1);
  return 1;
}

/* Compare the pixel at *p in the source surface's format against the colorkey
 * value.  Returns non-zero when the pixel is transparent and should be skipped
 * during blitting. */
static int tcmips_sdl_pixel_is_transparent(const Uint8 *p, int bpp,
                                           Uint32 colorkey) {
  switch (bpp) {
  case 1:
    return p[0] == (Uint8)(colorkey & 0xFF);
  case 2:
    return *(const Uint16 *)p == (Uint16)(colorkey & 0xFFFF);
  case 3:
    return p[0] == (Uint8)(colorkey & 0xFF) &&
           p[1] == (Uint8)((colorkey >> 8) & 0xFF) &&
           p[2] == (Uint8)((colorkey >> 16) & 0xFF);
  case 4:
    return *(const Uint32 *)p == colorkey;
  default:
    return 0;
  }
}

/* Per-surface alpha channel blend (SDL_SRCALPHA): matches the upstream
 * 1.2.15 software-blit rounding ((s*al + d*(255-al) + 127) / 255). */
static Uint8 tcmips_sdl_blend_channel(Uint8 s, Uint8 d, Uint32 al) {
  return (Uint8)(((Uint32)s * al + (Uint32)d * (255u - al) + 127u) / 255u);
}

static void tcmips_sdl_copy_rect_32_to_32(SDL_Surface *src, const SDL_Rect *sr,
                                          SDL_Surface *dst, const SDL_Rect *dr,
                                          int skip_ck, Uint32 ck, Uint8 alpha) {
  const Uint32 *sp = (const Uint32 *)src->pixels;
  Uint32 *dp = (Uint32 *)dst->pixels;
  int spitch = src->pitch >> 2;
  int dpitch = dst->pitch >> 2;

  int masks_match = (src->format->Rmask == dst->format->Rmask &&
                     src->format->Gmask == dst->format->Gmask &&
                     src->format->Bmask == dst->format->Bmask &&
                     src->format->Amask == dst->format->Amask);

  if (!skip_ck && masks_match && alpha <= 0) {
    /* No colorkey, no per-surface alpha and same pixel layout: use memcpy
     * for speed. */
    for (int y = 0; y < dr->h; ++y) {
      const Uint32 *srow = sp + (size_t)(sr->y + y) * spitch + sr->x;
      Uint32 *drow = dp + (size_t)(dr->y + y) * dpitch + dr->x;
      SDL_memcpy(drow, srow, (size_t)dr->w * sizeof(Uint32));
    }
  } else {
    for (int y = 0; y < dr->h; ++y) {
      const Uint32 *srow = sp + (size_t)(sr->y + y) * spitch + sr->x;
      Uint32 *drow = dp + (size_t)(dr->y + y) * dpitch + dr->x;
      for (int x = 0; x < dr->w; ++x) {
        if (skip_ck && srow[x] == ck)
          continue;
        if (alpha) {
          /* Per-surface alpha blend (SDL_SRCALPHA). */
          Uint8 sr, sg, sb, sa, dr2, dg2, db2, da2;
          SDL_GetRGBA(srow[x], src->format, &sr, &sg, &sb, &sa);
          SDL_GetRGBA(drow[x], dst->format, &dr2, &dg2, &db2, &da2);
          (void)sa;
          (void)da2;
          Uint32 r = tcmips_sdl_blend_channel(sr, dr2, alpha);
          Uint32 g = tcmips_sdl_blend_channel(sg, dg2, alpha);
          Uint32 b = tcmips_sdl_blend_channel(sb, db2, alpha);
          drow[x] = SDL_MapRGBA(dst->format, (Uint8)r, (Uint8)g, (Uint8)b, 255);
        } else if (masks_match) {
          drow[x] = srow[x];
        } else {
          Uint8 r, g, b, a;
          SDL_GetRGBA(srow[x], src->format, &r, &g, &b, &a);
          drow[x] = SDL_MapRGBA(dst->format, r, g, b, a);
        }
      }
    }
  }
}

static void tcmips_sdl_copy_rect_8_to_32(SDL_Surface *src, const SDL_Rect *sr,
                                         SDL_Surface *dst, const SDL_Rect *dr,
                                         int skip_ck, Uint32 ck, Uint8 alpha) {
  const Uint8 *sp = (const Uint8 *)src->pixels;
  Uint32 *dp = (Uint32 *)dst->pixels;
  int spitch = src->pitch;
  int dpitch = dst->pitch >> 2;

  SDL_Palette *pal =
      src->format && src->format->palette ? src->format->palette : NULL;
  Uint32 lut[256];
  if (pal) {
    int n = pal->ncolors < 256 ? pal->ncolors : 256;
    for (int i = 0; i < n; ++i)
      /* Map the palette entry through the *destination* format: a blind
       * 32-bit write of the raw 'val' word only matches dst layouts whose
       * byte order is R,G,B,A. */
      lut[i] = SDL_MapRGBA(dst->format, pal->colors[i].r, pal->colors[i].g,
                           pal->colors[i].b, 255);
    for (int i = n; i < 256; ++i)
      lut[i] = SDL_MapRGBA(dst->format, (Uint8)i, (Uint8)i, (Uint8)i, 255);
  } else {
    for (int i = 0; i < 256; ++i)
      lut[i] = SDL_MapRGBA(dst->format, (Uint8)i, (Uint8)i, (Uint8)i, 255);
  }

  Uint8 keyed = (Uint8)(ck & 0xFF);

  for (int y = 0; y < dr->h; ++y) {
    const Uint8 *srow = sp + (size_t)(sr->y + y) * spitch + sr->x;
    Uint32 *drow = dp + (size_t)(dr->y + y) * dpitch + dr->x;
    for (int x = 0; x < dr->w; ++x) {
      if (skip_ck && srow[x] == keyed)
        continue;
      Uint32 c = lut[srow[x]];
      if (alpha) {
        Uint8 sr, sg, sb, sa, dr2, dg2, db2, da2;
        SDL_GetRGBA(c, dst->format, &sr, &sg, &sb, &sa);
        SDL_GetRGBA(drow[x], dst->format, &dr2, &dg2, &db2, &da2);
        (void)sa;
        (void)da2;
        c = SDL_MapRGBA(dst->format, tcmips_sdl_blend_channel(sr, dr2, alpha),
                        tcmips_sdl_blend_channel(sg, dg2, alpha),
                        tcmips_sdl_blend_channel(sb, db2, alpha), 255);
      }
      drow[x] = c;
    }
  }
}

/* 8-to-8 palette-index copy. */
static void tcmips_sdl_copy_rect_8_to_8(SDL_Surface *src, const SDL_Rect *sr,
                                        SDL_Surface *dst, const SDL_Rect *dr,
                                        int skip_ck, Uint32 ck) {
  const Uint8 *sp = (const Uint8 *)src->pixels;
  Uint8 *dp = (Uint8 *)dst->pixels;
  int spitch = src->pitch;
  int dpitch = dst->pitch;

  if (!skip_ck) {
    /* No colorkey: copy palette indices row by row. */
    for (int y = 0; y < dr->h; ++y) {
      const Uint8 *srow = sp + (size_t)(sr->y + y) * spitch + sr->x;
      Uint8 *drow = dp + (size_t)(dr->y + y) * dpitch + dr->x;
      SDL_memcpy(drow, srow, (size_t)dr->w);
    }
  } else {
    Uint8 keyed = (Uint8)(ck & 0xFF);
    for (int y = 0; y < dr->h; ++y) {
      const Uint8 *srow = sp + (size_t)(sr->y + y) * spitch + sr->x;
      Uint8 *drow = dp + (size_t)(dr->y + y) * dpitch + dr->x;
      for (int x = 0; x < dr->w; ++x) {
        if (srow[x] == keyed)
          continue;
        drow[x] = srow[x];
      }
    }
  }
}

/* Generic cross-bpp copy using the public SDL_GetRGBA / SDL_MapRGBA helpers.
   This handles the less common combinations (16->32, 24->32, etc.) as well as
   same-bpp blits when source and destination format masks differ. */
static void tcmips_sdl_copy_rect_any_to_any(SDL_Surface *src,
                                            const SDL_Rect *sr,
                                            SDL_Surface *dst,
                                            const SDL_Rect *dr, int skip_ck,
                                            Uint32 ck, Uint8 alpha) {
  Uint8 sbpp = src->format->BytesPerPixel
                   ? src->format->BytesPerPixel
                   : (Uint8)(src->format->BitsPerPixel / 8);
  Uint8 dbpp = dst->format->BytesPerPixel
                   ? dst->format->BytesPerPixel
                   : (Uint8)(dst->format->BitsPerPixel / 8);

  for (int y = 0; y < dr->h; ++y) {
    for (int x = 0; x < dr->w; ++x) {
      const Uint8 *sp = (const Uint8 *)src->pixels +
                        (size_t)(sr->y + y) * src->pitch +
                        (size_t)(sr->x + x) * sbpp;
      if (skip_ck && tcmips_sdl_pixel_is_transparent(sp, sbpp, ck))
        continue;

      Uint32 spix;
      switch (sbpp) {
      case 1:
        spix = sp[0];
        break;
      case 2:
        spix = (Uint32) * (const Uint16 *)sp;
        break;
      case 3:
        spix = ((Uint32)sp[2] << 16) | ((Uint32)sp[1] << 8) | sp[0];
        break;
      case 4:
      default:
        spix = *(const Uint32 *)sp;
        break;
      }

      Uint8 r, g, b, a;
      SDL_GetRGBA(spix, src->format, &r, &g, &b, &a);
      Uint32 dpix;

      if (alpha) {
        /* Per-surface alpha blend over the destination pixel. */
        const Uint8 *dprev = (const Uint8 *)dst->pixels +
                             (size_t)(dr->y + y) * dst->pitch +
                             (size_t)(dr->x + x) * dbpp;
        Uint32 existing;
        switch (dbpp) {
        case 1:
          existing = dprev[0];
          break;
        case 2:
          existing = *(const Uint16 *)dprev;
          break;
        case 3:
          existing =
              ((Uint32)dprev[2] << 16) | ((Uint32)dprev[1] << 8) | dprev[0];
          break;
        case 4:
        default:
          existing = *(const Uint32 *)dprev;
          break;
        }
        Uint8 dr2, dg2, db2, da2;
        SDL_GetRGBA(existing, dst->format, &dr2, &dg2, &db2, &da2);
        (void)da2;
        dpix = SDL_MapRGBA(dst->format, tcmips_sdl_blend_channel(r, dr2, alpha),
                           tcmips_sdl_blend_channel(g, dg2, alpha),
                           tcmips_sdl_blend_channel(b, db2, alpha), 255);
      } else {
        dpix = SDL_MapRGBA(dst->format, r, g, b, a);
      }

      Uint8 *dp = (Uint8 *)dst->pixels + (size_t)(dr->y + y) * dst->pitch +
                  (size_t)(dr->x + x) * dbpp;
      switch (dbpp) {
      case 1:
        dp[0] = (Uint8)(dpix & 0xff);
        break;
      case 2:
        *(Uint16 *)dp = (Uint16)dpix;
        break;
      case 3:
        dp[0] = dpix & 0xff;
        dp[1] = (dpix >> 8) & 0xff;
        dp[2] = (dpix >> 16) & 0xff;
        break;
      case 4:
      default:
        *(Uint32 *)dp = dpix;
        break;
      }
    }
  }
}

static void tcmips_sdl_fill_rect_32(SDL_Surface *dst, const SDL_Rect *dr,
                                    Uint32 color) {
  int dpitch = dst->pitch >> 2;

  /* Fill each row with memset4 (word-sized Duff's-device store,
     equivalent to a per-word loop but without the inner branch). */
  for (int y = 0; y < dr->h; ++y) {
    Uint32 *row = (Uint32 *)dst->pixels + (size_t)(dr->y + y) * dpitch + dr->x;
    memset4(row, color, (size_t)dr->w);
  }
}

int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst,
                    SDL_Rect *dstrect) {
  if (!src || !dst || !src->format || !dst->format)
    return -1;
  if (!src->pixels || !dst->pixels)
    return -1;

  /* Check for colorkey (transparent pixel skip). */
  int has_colorkey = (src->flags & SDL_SRCCOLORKEY) ? 1 : 0;
  Uint32 colorkey = has_colorkey ? src->format->colorkey : 0;

  /* Per-surface alpha (SDL_SetAlpha / SDL_SRCALPHA): 1..254 blends the
   * source over the destination; 255 (or unset) is fully opaque and keeps
   * the opaque fast paths. */
  Uint8 blitalpha = 0;
  if (src->flags & SDL_SRCALPHA)
    blitalpha = src->format->alpha;

  SDL_Rect src_r, dst_r;
  if (srcrect) {
    src_r = *srcrect;
  } else {
    src_r.x = 0;
    src_r.y = 0;
    src_r.w = (Uint16)src->w;
    src_r.h = (Uint16)src->h;
  }

  if (dstrect) {
    dst_r = *dstrect;
  } else {
    dst_r.x = 0;
    dst_r.y = 0;
  }
  dst_r.w = src_r.w;
  dst_r.h = src_r.h;

  /* A 1:1 blit keeps src and dst in lockstep: clipping either rectangle (for
     a negative origin or an overrun) must be mirrored into the other by the
     same pixel offset.  This mirrors both the srcrect and the dstrect clip
     offset, so a partially-off-screen sprite (or a negative srcrect) draws
     the correct remaining portion. */

  /* 1. Clip the source rect into the source surface; mirror the offset into
        the destination rect. */
  {
    SDL_Rect src_full = {0, 0, (Uint16)src->w, (Uint16)src->h};
    int sx0 = src_r.x;
    int sy0 = src_r.y;
    if (!tcmips_sdl_intersect_rect(&src_r, &src_full, &src_r))
      return 0;
    dst_r.x += src_r.x - sx0;
    dst_r.y += src_r.y - sy0;
    dst_r.w = src_r.w;
    dst_r.h = src_r.h;
  }

  /* 2. Clip the destination rect into the clip_rect; mirror the offset back
        into the source rect. */
  {
    int dx0 = dst_r.x;
    int dy0 = dst_r.y;
    if (!tcmips_sdl_intersect_rect(&dst_r, &dst->clip_rect, &dst_r))
      return 0;
    src_r.x += dst_r.x - dx0;
    src_r.y += dst_r.y - dy0;
    src_r.w = dst_r.w;
    src_r.h = dst_r.h;
    if (src_r.w <= 0 || src_r.h <= 0)
      return 0;
  }

  /* Same-bpp with identical format masks: raw byte-copy per row.
     This avoids per-pixel GetRGBA / MapRGBA overhead for the common case
     where two surfaces share the same pixel layout (e.g. default 32-bit
     ABGR, default 16-bit 5:5:5:1, default 24-bit RGB, etc.). */
  if (dst->format->BitsPerPixel == src->format->BitsPerPixel &&
      dst->format->Rmask == src->format->Rmask &&
      dst->format->Gmask == src->format->Gmask &&
      dst->format->Bmask == src->format->Bmask &&
      dst->format->Amask == src->format->Amask && !has_colorkey &&
      blitalpha == 0) {
    Uint8 bpp = dst->format->BytesPerPixel
                    ? dst->format->BytesPerPixel
                    : (Uint8)(dst->format->BitsPerPixel / 8);
    size_t row_bytes = (size_t)dst_r.w * (size_t)bpp;
    for (int y = 0; y < dst_r.h; ++y) {
      const Uint8 *s = (const Uint8 *)src->pixels +
                       (size_t)(src_r.y + y) * src->pitch +
                       (size_t)src_r.x * (size_t)bpp;
      Uint8 *d = (Uint8 *)dst->pixels + (size_t)(dst_r.y + y) * dst->pitch +
                 (size_t)dst_r.x * (size_t)bpp;
      SDL_memcpy(d, s, row_bytes);
    }
    if (dstrect) {
      dstrect->w = dst_r.w;
      dstrect->h = dst_r.h;
    }
    return 0;
  }

  /* Standard dispatch for different-bpp, mismatched masks, or colorkey. */
  if (dst->format->BitsPerPixel == src->format->BitsPerPixel) {
    if (dst->format->BitsPerPixel == 32) {
      tcmips_sdl_copy_rect_32_to_32(src, &src_r, dst, &dst_r, has_colorkey,
                                    colorkey, blitalpha);
    } else if (dst->format->BitsPerPixel == 8) {
      tcmips_sdl_copy_rect_8_to_8(src, &src_r, dst, &dst_r, has_colorkey,
                                  colorkey);
    } else if (dst->format->BitsPerPixel <= 32) {
      /* Same-bpp but mismatched masks, or colorkey active. */
      tcmips_sdl_copy_rect_any_to_any(src, &src_r, dst, &dst_r, has_colorkey,
                                      colorkey, blitalpha);
    } else {
      SDL_SetError("BlitSurface: unsupported bpp %d",
                   dst->format->BitsPerPixel);
      return -1;
    }
  } else {
    /* Cross-bpp conversion. */
    if (dst->format->BitsPerPixel == 32 && src->format->BitsPerPixel == 8) {
      tcmips_sdl_copy_rect_8_to_32(src, &src_r, dst, &dst_r, has_colorkey,
                                   colorkey, blitalpha);
    } else if (dst->format->BitsPerPixel <= 32 &&
               src->format->BitsPerPixel <= 32) {
      tcmips_sdl_copy_rect_any_to_any(src, &src_r, dst, &dst_r, has_colorkey,
                                      colorkey, blitalpha);
    } else {
      SDL_SetError("BlitSurface: unsupported bpp combination %d -> %d",
                   src->format->BitsPerPixel, dst->format->BitsPerPixel);
      return -1;
    }
  }

  if (dstrect) {
    dstrect->w = dst_r.w;
    dstrect->h = dst_r.h;
  }
  return 0;
}

int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color) {
  if (!dst || !dst->format || !dst->pixels)
    return -1;

  SDL_Rect r;
  if (dstrect) {
    r = *dstrect;
  } else {
    r.x = 0;
    r.y = 0;
    r.w = (Uint16)dst->w;
    r.h = (Uint16)dst->h;
  }

  SDL_Rect clipped;
  if (!tcmips_sdl_intersect_rect(&r, &dst->clip_rect, &clipped))
    return 0;

  /* Upstream 1.2.15 reports the area actually filled back through
   * *dstrect. */
  if (dstrect)
    *dstrect = clipped;

  if (dst->format->BitsPerPixel == 32) {
    tcmips_sdl_fill_rect_32(dst, &clipped, color);
  } else if (dst->format->BitsPerPixel == 8) {
    Uint8 idx = (Uint8)(color & 0xFF);
    for (int y = 0; y < clipped.h; ++y) {
      Uint8 *row = (Uint8 *)dst->pixels + (size_t)(clipped.y + y) * dst->pitch +
                   clipped.x;
      SDL_memset(row, idx, (size_t)clipped.w);
    }
  } else if (dst->format->BitsPerPixel == 16) {
    /* RGB565 / RGB555 surface: write the packed 16-bit color. */
    Uint16 c16 = (Uint16)(color & 0xFFFF);
    int dpitch = dst->pitch / 2;
    for (int y = 0; y < clipped.h; ++y) {
      Uint16 *row =
          (Uint16 *)dst->pixels + (size_t)(clipped.y + y) * dpitch + clipped.x;
      for (int x = 0; x < clipped.w; ++x)
        row[x] = c16;
    }
  } else if (dst->format->BitsPerPixel == 24) {
    /* BGR888 / RGB888 over 3 bytes per pixel. */
    Uint8 r8 = (Uint8)(color & 0xFF), g8 = (Uint8)((color >> 8) & 0xFF),
          b8 = (Uint8)((color >> 16) & 0xFF);
    for (int y = 0; y < clipped.h; ++y) {
      Uint8 *row = (Uint8 *)dst->pixels + (size_t)(clipped.y + y) * dst->pitch +
                   clipped.x * 3u;
      for (int x = 0; x < clipped.w; ++x) {
        row[x * 3 + 0] = r8;
        row[x * 3 + 1] = g8;
        row[x * 3 + 2] = b8;
      }
    }
  }
  return 0;
}

/* SDL_SoftStretch: nearest-neighbour stretch between 32-bpp surfaces.
 *
 * Upstream 1.2.15 conformant bits:
 *   - dstrect == NULL  ->  the full DESTINATION surface;
 *   - explicit rects that do not fit the surfaces are an error upstream.
 * TCMIPS extension: out-of-bounds origins/overruns are proportionally
 * clipped (instead of failing), and cross-format surfaces are converted
 * via SDL_GetRGBA/SDL_MapRGBA (upstream requires equal BytesPerPixel). */
int SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst,
                    SDL_Rect *dstrect) {
  if (!src || !dst || !src->format || !dst->format)
    return -1;
  if (!src->pixels || !dst->pixels)
    return -1;

  SDL_Rect src_r, dst_r;
  if (srcrect) {
    src_r = *srcrect;
  } else {
    src_r.x = 0;
    src_r.y = 0;
    src_r.w = (Uint16)src->w;
    src_r.h = (Uint16)src->h;
  }
  if (dstrect) {
    dst_r = *dstrect;
  } else {
    /* Upstream 1.2.15: dstrect == NULL means the full DESTINATION surface
       (dst_w = dst->w, dst_h = dst->h), not the source size.  Defaulting to
       the source size made SDL_SoftStretch(img, NULL, screen, NULL) land
       only in the top-left corner of the screen. */
    dst_r.x = 0;
    dst_r.y = 0;
    dst_r.w = (Uint16)dst->w;
    dst_r.h = (Uint16)dst->h;
  }

  if (src_r.w <= 0 || src_r.h <= 0 || dst_r.w <= 0 || dst_r.h <= 0) {
    return -1;
  }

  /* Clamp the source rect into the source surface on all four edges, and
     mirror each clamp into the destination rect in the same proportion
     (dst : src) so the src->dst scale stays correct.  A negative srcrect
     origin or an overrunning srcrect would otherwise map to out-of-bounds
     source coordinates (read before/after the pixel buffer). */
  {
    int sw = src_r.w;
    int sh = src_r.h;
    int dw = dst_r.w;
    int dh = dst_r.h;
    if (src_r.x < 0) {
      int clip = -src_r.x;
      dst_r.x += clip * dw / sw;
      dst_r.w -= clip * dw / sw;
      src_r.x = 0;
      src_r.w -= clip;
    }
    if (src_r.y < 0) {
      int clip = -src_r.y;
      dst_r.y += clip * dh / sh;
      dst_r.h -= clip * dh / sh;
      src_r.y = 0;
      src_r.h -= clip;
    }
    if (src_r.x + src_r.w > src->w) {
      int clip = (src_r.x + src_r.w) - src->w;
      dst_r.w -= clip * dw / sw;
      src_r.w -= clip;
    }
    if (src_r.y + src_r.h > src->h) {
      int clip = (src_r.y + src_r.h) - src->h;
      dst_r.h -= clip * dh / sh;
      src_r.h -= clip;
    }
    if (src_r.w <= 0 || src_r.h <= 0 || dst_r.w <= 0 || dst_r.h <= 0)
      return 0;
  }

  /* Clip destination to surface clip_rect. */
  SDL_Rect clipped;
  if (!tcmips_sdl_intersect_rect(&dst_r, &dst->clip_rect, &clipped)) {
    return 0;
  }

  /* Adjust source rect proportionally to the post-clip destination. */
  int dx_off = clipped.x - dst_r.x;
  int dy_off = clipped.y - dst_r.y;
  int cw = clipped.w;
  int ch = clipped.h;

  int sbpp = src->format->BytesPerPixel ? src->format->BytesPerPixel
                                        : src->format->BitsPerPixel / 8;
  int dbpp = dst->format->BytesPerPixel ? dst->format->BytesPerPixel
                                        : dst->format->BitsPerPixel / 8;
  if (sbpp <= 0 || dbpp <= 0 || sbpp > 4 || dbpp > 4) {
    SDL_SetError("SDL_SoftStretch: unsupported bpp");
    return -1;
  }

  /* Horizontal 1:1 fast path -- dispatched BEFORE the coordinate tables so
     the common same-size case (full-screen stretches in particular) completes
     with plain row memcpys and never touches the heap: the mallocs only pay
     off for genuinely rescaled blits.  With src_r.w == dst_r.w the sx_table
     mapping degenerates to sx = src_r.x + dx_off + dx (the dx_off fix), so
     this block reproduces the post-table behaviour bit for bit. */
  if (src_r.w == dst_r.w && sbpp == dbpp) {
    int sx_off = dx_off; /* (dx_off * src_r.w) / dst_r.w == dx_off when equal */
    size_t row_bytes = (size_t)cw * (size_t)sbpp;
    for (int dy = 0; dy < ch; ++dy) {
      int sy = src_r.y + ((dy_off + dy) * src_r.h) / dst_r.h;
      if (sy < 0)
        sy = 0;
      if (sy >= src->h)
        sy = src->h - 1;
      const Uint8 *srow = (const Uint8 *)src->pixels + (size_t)sy * src->pitch +
                          (size_t)(src_r.x + sx_off) * sbpp;
      Uint8 *drow = (Uint8 *)dst->pixels +
                    (size_t)(clipped.y + dy) * dst->pitch +
                    (size_t)clipped.x * dbpp;
      SDL_memcpy(drow, srow, row_bytes);
    }
    return 0;
  }

  /* Precompute source coordinates once per call to avoid expensive integer
     divisions inside the inner loops. */
  int *sy_table = (int *)SDL_malloc((size_t)ch * sizeof(int));
  int *sx_table = (int *)SDL_malloc((size_t)cw * sizeof(int));
  if (!sy_table || !sx_table) {
    SDL_SetError("SDL_SoftStretch: out of memory");
    SDL_free(sx_table);
    SDL_free(sy_table);
    return -1;
  }
  for (int dy = 0; dy < ch; ++dy) {
    int sy = src_r.y + ((dy_off + dy) * src_r.h) / dst_r.h;
    /* Clamp the mapped source coordinate into the source surface on both
       edges.  A negative srcrect origin (or an overrunning srcrect) makes the
       mapped sy/sx land outside [0, size), which would read before/after the
       source pixel buffer. */
    if (sy < 0)
      sy = 0;
    if (sy >= src->h)
      sy = src->h - 1;
    sy_table[dy] = sy;
  }
  for (int dx = 0; dx < cw; ++dx) {
    int sx = src_r.x + ((dx_off + dx) * src_r.w) / dst_r.w;
    if (sx < 0)
      sx = 0;
    if (sx >= src->w)
      sx = src->w - 1;
    sx_table[dx] = sx;
  }

  /* 8-bpp palette -> 32-bpp stretch fast path. */
  if (sbpp == 1 && dbpp == 4) {
    SDL_Palette *pal =
        src->format && src->format->palette ? src->format->palette : NULL;
    Uint32 lut[256];
    if (pal) {
      int n = pal->ncolors < 256 ? pal->ncolors : 256;
      for (int i = 0; i < n; ++i)
        /* Map through the *destination* format (see copy_rect_8_to_32). */
        lut[i] = SDL_MapRGBA(dst->format, pal->colors[i].r, pal->colors[i].g,
                             pal->colors[i].b, 255);
      for (int i = n; i < 256; ++i)
        lut[i] = SDL_MapRGBA(dst->format, (Uint8)i, (Uint8)i, (Uint8)i, 255);
    } else {
      for (int i = 0; i < 256; ++i)
        lut[i] = SDL_MapRGBA(dst->format, (Uint8)i, (Uint8)i, (Uint8)i, 255);
    }
    int dpitch = dst->pitch >> 2;
    for (int dy = 0; dy < ch; ++dy) {
      const Uint8 *srow =
          (const Uint8 *)src->pixels + (size_t)sy_table[dy] * src->pitch;
      Uint32 *drow =
          (Uint32 *)dst->pixels + (size_t)(clipped.y + dy) * dpitch + clipped.x;
      for (int dx = 0; dx < cw; ++dx) {
        drow[dx] = lut[srow[sx_table[dx]]];
      }
    }
    SDL_free(sx_table);
    SDL_free(sy_table);
    return 0;
  }

  /* Generic cross-bpp or non-1:1 scaling. */
  for (int dy = 0; dy < ch; ++dy) {
    const Uint8 *srow =
        (const Uint8 *)src->pixels + (size_t)sy_table[dy] * src->pitch;
    Uint8 *drow = (Uint8 *)dst->pixels + (size_t)(clipped.y + dy) * dst->pitch +
                  (size_t)clipped.x * dbpp;

    for (int dx = 0; dx < cw; ++dx) {
      const Uint8 *sp = srow + (size_t)sx_table[dx] * sbpp;
      Uint8 *dp = drow + (size_t)dx * dbpp;

      Uint32 spix;
      switch (sbpp) {
      case 1:
        spix = sp[0];
        break;
      case 2:
        spix = (Uint32) * (const Uint16 *)sp;
        break;
      case 3:
        spix = ((Uint32)sp[2] << 16) | ((Uint32)sp[1] << 8) | sp[0];
        break;
      case 4:
      default:
        spix = *(const Uint32 *)sp;
        break;
      }

      Uint8 r, g, b, a;
      SDL_GetRGBA(spix, src->format, &r, &g, &b, &a);
      Uint32 dpix = SDL_MapRGBA(dst->format, r, g, b, a);

      switch (dbpp) {
      case 1:
        dp[0] = (Uint8)(dpix & 0xff);
        break;
      case 2:
        *(Uint16 *)dp = (Uint16)dpix;
        break;
      case 3:
        dp[0] = dpix & 0xff;
        dp[1] = (dpix >> 8) & 0xff;
        dp[2] = (dpix >> 16) & 0xff;
        break;
      case 4:
      default:
        *(Uint32 *)dp = dpix;
        break;
      }
    }
  }

  SDL_free(sx_table);
  SDL_free(sy_table);
  return 0;
}

SDL_Surface *SDL_LoadBMP_RW(SDL_RWops *src, int freesrc) {
  if (!src)
    return NULL;

  SDL_Surface *surf = NULL;

  /* BMP is little-endian. */
#define READ_U16(p) ((Uint16)(((p)[0]) | ((p)[1] << 8)))
#define READ_U32(p)                                                            \
  ((Uint32)(((p)[0]) | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24)))
#define READ_S32(p) ((Sint32)READ_U32(p))

  Uint8 file_header[14];
  if (SDL_RWread(src, file_header, 14, 1) != 1)
    goto done;
  if (file_header[0] != 'B' || file_header[1] != 'M')
    goto done;
  Uint32 off_bits = READ_U32(file_header + 10);

  /* DIB header.  We need BITMAPINFOHEADER (40 bytes); skip any extra fields. */
  Uint8 dib_header[40];
  if (SDL_RWread(src, dib_header, 40, 1) != 1)
    goto done;
  Uint32 dib_size = READ_U32(dib_header + 0);
  if (dib_size < 40)
    goto done;
  if (dib_size > 40) {
    if (SDL_RWseek(src, (int)(dib_size - 40), SEEK_CUR) < 0)
      goto done;
  }

  int width = READ_S32(dib_header + 4);
  int height = READ_S32(dib_header + 8);
  Uint16 planes = READ_U16(dib_header + 12);
  Uint16 bpp = READ_U16(dib_header + 14);
  Uint32 compression = READ_U32(dib_header + 16);
  Uint32 colors_used = READ_U32(dib_header + 32);

  if (planes != 1 || compression != 0)
    goto done;
  if (width <= 0 || height == 0)
    goto done;

  int top_down = (height < 0);
  int h = top_down ? -height : height;

  /* Only support the common formats for now. */
  if (bpp != 8 && bpp != 24 && bpp != 32) {
    SDL_SetError("SDL_LoadBMP_RW: unsupported bpp %u", (unsigned)bpp);
    goto done;
  }

  /* Determine palette size (capped at 256 for 8-bpp). */
  int palette_size = 0;
  if (bpp == 8) {
    palette_size = colors_used ? (int)colors_used : 256;
    if (palette_size > 256)
      palette_size = 256;
  }

  SDL_Color palette[256];
  SDL_memset(palette, 0, sizeof(palette));
  if (palette_size > 0) {
    size_t need = (size_t)palette_size * 4u;
    Uint8 *pal_buf = (Uint8 *)SDL_malloc(need);
    if (!pal_buf) {
      SDL_SetError("SDL_LoadBMP_RW: out of memory");
      goto done;
    }
    if (SDL_RWread(src, pal_buf, (int)need, 1) != 1) {
      SDL_free(pal_buf);
      goto done;
    }
    for (int i = 0; i < palette_size; ++i) {
      palette[i].b = pal_buf[i * 4 + 0];
      palette[i].g = pal_buf[i * 4 + 1];
      palette[i].r = pal_buf[i * 4 + 2];
      palette[i].unused = 0;
      palette[i].val = ((Uint32)pal_buf[i * 4 + 2]) |
                       ((Uint32)pal_buf[i * 4 + 1] << 8) |
                       ((Uint32)pal_buf[i * 4 + 0] << 16) | 0xFF000000u;
    }
    SDL_free(pal_buf);
  }

  /* Create the SDL surface.  24-bpp BMP becomes 32-bpp ABGR. */
  int dst_depth = (bpp == 8) ? 8 : 32;
  surf = SDL_CreateRGBSurface(SDL_SWSURFACE, width, h, dst_depth, 0, 0, 0, 0);
  if (!surf) {
    goto done;
  }

  /* Set the palette for 8-bpp surfaces. */
  if (bpp == 8) {
    if (surf->format->palette) {
      SDL_memcpy(surf->format->palette->colors, palette,
                 (size_t)palette_size * sizeof(SDL_Color));
      surf->format->palette->ncolors = palette_size;
    }
  }

  /* Seek to pixel data. */
  if (SDL_RWseek(src, (int)off_bits, SEEK_SET) < 0) {
    SDL_FreeSurface(surf);
    surf = NULL;
    goto done;
  }

  /* Row buffer for raw BMP scanline (with padding). */
  int raw_row_bytes;
  if (bpp == 8) {
    raw_row_bytes = (width + 3) & ~3;
  } else if (bpp == 24) {
    raw_row_bytes = (width * 3 + 3) & ~3;
  } else { /* 32 */
    raw_row_bytes = width * 4;
  }

  Uint8 *row_buf = (Uint8 *)SDL_malloc((size_t)raw_row_bytes);
  if (!row_buf) {
    SDL_SetError("SDL_LoadBMP_RW: out of memory");
    SDL_FreeSurface(surf);
    surf = NULL;
    goto done;
  }

  for (int row = 0; row < h; ++row) {
    if (SDL_RWread(src, row_buf, raw_row_bytes, 1) != 1) {
      SDL_free(row_buf);
      SDL_FreeSurface(surf);
      surf = NULL;
      goto done;
    }
    int dst_row = top_down ? row : (h - 1 - row);
    Uint8 *d = (Uint8 *)surf->pixels + (size_t)dst_row * surf->pitch;

    if (bpp == 8) {
      SDL_memcpy(d, row_buf, (size_t)width);
    } else if (bpp == 24) {
      Uint32 *dp = (Uint32 *)d;
      for (int x = 0; x < width; ++x) {
        Uint8 b = row_buf[x * 3 + 0];
        Uint8 g = row_buf[x * 3 + 1];
        Uint8 r = row_buf[x * 3 + 2];
        dp[x] =
            ((Uint32)r) | ((Uint32)g << 8) | ((Uint32)b << 16) | 0xFF000000u;
      }
    } else { /* 32 */
      Uint32 *dp = (Uint32 *)d;
      for (int x = 0; x < width; ++x) {
        Uint8 b = row_buf[x * 4 + 0];
        Uint8 g = row_buf[x * 4 + 1];
        Uint8 r = row_buf[x * 4 + 2];
        Uint8 a = row_buf[x * 4 + 3];
        dp[x] = ((Uint32)r) | ((Uint32)g << 8) | ((Uint32)b << 16) |
                ((Uint32)a << 24);
      }
    }
  }

  SDL_free(row_buf);

#undef READ_U16
#undef READ_U32
#undef READ_S32

done:
  if (freesrc && src)
    SDL_RWclose(src);
  if (!surf)
    SDL_SetError("SDL_LoadBMP_RW: failed to load bitmap");
  return surf;
}

int SDL_SaveBMP_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst) {
  if (!surface || !dst || !surface->pixels || !surface->format) {
    if (freedst && dst)
      SDL_RWclose(dst);
    return -1;
  }

  int w = surface->w;
  int h = surface->h;
  int bpp = surface->format->BitsPerPixel;

  /* Only support pixel formats that can be directly mapped to BMP. */
  if (bpp != 8 && bpp != 24 && bpp != 32) {
    TCMIPS_SDL_STUB_MSG("SDL_SaveBMP_RW: unsupported bpp");
    if (freedst && dst)
      SDL_RWclose(dst);
    return -1;
  }

  /* Compute per-row scanline size (BMP requires 4-byte row alignment). */
  int raw_row;
  if (bpp == 8)
    raw_row = (w + 3) & ~3;
  else if (bpp == 24)
    raw_row = (w * 3 + 3) & ~3;
  else
    raw_row = w * 4; /* bpp == 32: already 4-byte aligned */

  /* 8-bit output requires a palette. */
  if (bpp == 8 && !surface->format->palette) {
    TCMIPS_SDL_STUB_MSG("SDL_SaveBMP_RW: 8-bit surface has no palette");
    if (freedst && dst)
      SDL_RWclose(dst);
    return -1;
  }

  /* Per-row row buffer (with padding) avoids many small RWwrite calls.
     Allocate before writing headers so a failure leaves the stream untouched.
   */
  Uint8 *row_buf = (Uint8 *)SDL_malloc((size_t)raw_row * sizeof(Uint8));
  if (!row_buf) {
    TCMIPS_SDL_STUB_MSG("SDL_SaveBMP_RW: out of memory");
    if (freedst && dst)
      SDL_RWclose(dst);
    return -1;
  }

  /* File size: header + (optional) palette + pixel data. */
  Uint32 palette_bytes = 0;
  if (bpp == 8 && surface->format->palette)
    palette_bytes = (Uint32)surface->format->palette->ncolors * 4u;
  Uint32 file_size = 14u + 40u + palette_bytes + (Uint32)raw_row * (Uint32)h;
  Uint32 off_bits = 14u + 40u + palette_bytes;

  /* --- BITMAPFILEHEADER (14 bytes, little-endian) --- */
  {
    Uint8 hdr[14];
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[2] = (Uint8)(file_size);
    hdr[3] = (Uint8)(file_size >> 8);
    hdr[4] = (Uint8)(file_size >> 16);
    hdr[5] = (Uint8)(file_size >> 24);
    hdr[6] = 0;
    hdr[7] = 0;
    hdr[8] = (Uint8)(off_bits);
    hdr[9] = (Uint8)(off_bits >> 8);
    hdr[10] = (Uint8)(off_bits >> 16);
    hdr[11] = (Uint8)(off_bits >> 24);
    hdr[12] = 0;
    hdr[13] = 0;
    if (SDL_RWwrite(dst, hdr, 14, 1) != 1)
      goto bail;
  }

  /* --- BITMAPINFOHEADER (40 bytes) --- */
  {
    Uint8 dih[40];
    /* biSize */
    dih[0] = 40;
    dih[1] = 0;
    dih[2] = 0;
    dih[3] = 0;
    /* biWidth */
    dih[4] = (Uint8)w;
    dih[5] = (Uint8)(w >> 8);
    dih[6] = (Uint8)(w >> 16);
    dih[7] = (Uint8)(w >> 24);
    /* biHeight -- positive => bottom-up (standard BMP) */
    dih[8] = (Uint8)h;
    dih[9] = (Uint8)(h >> 8);
    dih[10] = (Uint8)(h >> 16);
    dih[11] = (Uint8)(h >> 24);
    /* biPlanes = 1 */
    dih[12] = 1;
    dih[13] = 0;
    /* biBitCount */
    dih[14] = (Uint8)bpp;
    dih[15] = 0;
    /* biCompression = 0 (none) */
    dih[16] = 0;
    dih[17] = 0;
    dih[18] = 0;
    dih[19] = 0;
    /* biSizeImage (0 = computed, fine for uncompressed) */
    dih[20] = 0;
    dih[21] = 0;
    dih[22] = 0;
    dih[23] = 0;
    /* biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant = 0 */
    SDL_memset(dih + 24, 0, 16);
    if (SDL_RWwrite(dst, dih, 40, 1) != 1)
      goto bail;
  }

  /* --- Palette entries (8-bit only) --- */
  if (bpp == 8 && surface->format->palette) {
    SDL_Palette *pal = surface->format->palette;
    for (int i = 0; i < pal->ncolors; ++i) {
      Uint8 b = pal->colors[i].b;
      Uint8 g = pal->colors[i].g;
      Uint8 r = pal->colors[i].r;
      Uint8 pad = 0;
      Uint8 entry[4] = {b, g, r, pad};
      if (SDL_RWwrite(dst, entry, 4, 1) != 1)
        goto bail;
    }
  }

  /* --- Pixel data (bottom-up scanlines) --- */
  for (int row = 0; row < h; ++row) {
    int src_row = h - 1 - row; /* BMP is bottom-up: row 0 = last surface row */
    const Uint8 *sp = (const Uint8 *)surface->pixels +
                      (size_t)src_row * (size_t)surface->pitch;

    if (bpp == 8) {
      /* Just copy palette indices directly. */
      SDL_memcpy(row_buf, sp, (size_t)w);
      /* Pad rest of row to 4-byte boundary. */
      int pad = raw_row - w;
      SDL_memset(row_buf + (size_t)w, 0, (size_t)pad);
      if (SDL_RWwrite(dst, row_buf, (size_t)raw_row, 1) != 1)
        goto bail;
    } else if (bpp == 24) {
      /* 24-bit surface stores packed 3-byte pixels.  The default TCMIPS
         24-bit mask order ([B][G][R] in memory) matches BMP, so a direct
         copy is correct for the standard format. */
      int pad = raw_row - w * 3;
      for (int x = 0; x < w; ++x) {
        row_buf[x * 3 + 0] = sp[x * 3 + 0]; /* B */
        row_buf[x * 3 + 1] = sp[x * 3 + 1]; /* G */
        row_buf[x * 3 + 2] = sp[x * 3 + 2]; /* R */
      }
      SDL_memset(row_buf + (size_t)w * 3, 0, (size_t)pad);
      if (SDL_RWwrite(dst, row_buf, (size_t)raw_row, 1) != 1)
        goto bail;
    } else { /* bpp == 32 */
      /* TCMIPS 32-bpp surfaces use R in the low byte (little-endian):
             surface in memory:  [R][G][B][A]
         BMP pixel layout is fixed B,G,R,A regardless of CPU endianness:
             BMP row bytes:      [B][G][R][A]
         The red and blue channels must therefore be swapped when saving. */
      for (int x = 0; x < w; ++x) {
        Uint8 r8 = sp[x * 4 + 0]; /* R */
        Uint8 g8 = sp[x * 4 + 1]; /* G */
        Uint8 b8 = sp[x * 4 + 2]; /* B */
        Uint8 a8 = sp[x * 4 + 3]; /* A */
        row_buf[x * 4 + 0] = b8;  /* BMP B */
        row_buf[x * 4 + 1] = g8;  /* BMP G */
        row_buf[x * 4 + 2] = r8;  /* BMP R */
        row_buf[x * 4 + 3] = a8;  /* BMP A */
      }
      /* 32-bit rows are already 4-byte aligned, no padding needed. */
      if (SDL_RWwrite(dst, row_buf, (size_t)raw_row, 1) != 1)
        goto bail;
    }
  }

  SDL_free(row_buf);

  if (freedst && dst)
    SDL_RWclose(dst);
  return 0;

bail:;
  if (freedst && dst)
    SDL_RWclose(dst);
  SDL_free(row_buf);
  SDL_SetError("SDL_SaveBMP_RW: write failed");
  return -1;
}
