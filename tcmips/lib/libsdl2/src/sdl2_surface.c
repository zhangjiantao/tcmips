#include "tcmips_sdl2_internal.h"
#include <SDL.h>

#include <stdlib.h>
#include <string.h>

// TCMIPS uses RGBA32 (memory layout RGBA on little-endian, matching hardware).
#define TCMIPS_SDL_FORMAT SDL_PIXELFORMAT_RGBA32
#define TCMIPS_SDL_DEPTH 32
#define TCMIPS_SDL_RMASK 0x000000FF
#define TCMIPS_SDL_GMASK 0x0000FF00
#define TCMIPS_SDL_BMASK 0x00FF0000
#define TCMIPS_SDL_AMASK 0xFF000000

// Surface colour-key registry.  The upstream SDL_Surface stores colour-key
// state in its (private) blit map; TCMIPS SDL2 surfaces are plain, so we keep
// a small per-surface registry instead.  Games use at most a handful of
// sprites, so a fixed-size table is sufficient.
#define TCMIPS_SDL2_CK_MAX 32
typedef struct {
  SDL_Surface *surf;
  SDL_bool enabled;
  Uint32 key;
} tcmips_sdl2_ck_t;

static tcmips_sdl2_ck_t s_ck[TCMIPS_SDL2_CK_MAX];

static tcmips_sdl2_ck_t *tcmips_sdl2_ck_find(SDL_Surface *s) {
  for (int i = 0; i < TCMIPS_SDL2_CK_MAX; ++i) {
    if (s_ck[i].surf == s) {
      return &s_ck[i];
    }
  }
  return NULL;
}

static tcmips_sdl2_ck_t *tcmips_sdl2_ck_alloc(SDL_Surface *s) {
  tcmips_sdl2_ck_t *slot = tcmips_sdl2_ck_find(s);
  if (slot) {
    return slot;
  }
  for (int i = 0; i < TCMIPS_SDL2_CK_MAX; ++i) {
    if (s_ck[i].surf == NULL) {
      s_ck[i].surf = s;
      return &s_ck[i];
    }
  }
  return NULL; // registry full: colour key not stored
}

static void tcmips_sdl2_ck_free(SDL_Surface *s) {
  tcmips_sdl2_ck_t *slot = tcmips_sdl2_ck_find(s);
  if (slot) {
    slot->surf = NULL;
    slot->enabled = SDL_FALSE;
    slot->key = 0;
  }
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int width, int height,
                                            int depth, Uint32 format) {
  (void)flags;
  if (width <= 0 || height <= 0 || depth < 8)
    return NULL;

  /* Overflow-safe size cap (matches the SDL1 layer's 64 MiB pixel limit).
     Computed in 64-bit BEFORE the pitch multiply: a crafted huge width
     (e.g. via SDL_LoadBMP_RW on a hostile file) would otherwise wrap the
     int multiply into a small pitch and slide every subsequent bounds
     check. */
  Sint64 pixbytes = (Sint64)width * 4 * (Sint64)height;
  if (pixbytes <= 0 || pixbytes > ((Sint64)1 << 26)) {
    SDL_SetError("SDL_CreateRGBSurfaceWithFormat: surface too large");
    return NULL;
  }
  (void)format;

  SDL_Surface *s = (SDL_Surface *)SDL_calloc(
      1, sizeof(SDL_Surface) + sizeof(SDL_PixelFormat) + sizeof(SDL_Palette));
  if (!s)
    return NULL;

  s->format = (SDL_PixelFormat *)(s + 1);
  s->format->format = TCMIPS_SDL_FORMAT;
  s->format->BitsPerPixel = (Uint8)TCMIPS_SDL_DEPTH;
  s->format->BytesPerPixel = (Uint8)(TCMIPS_SDL_DEPTH / 8);
  s->format->Rmask = TCMIPS_SDL_RMASK;
  s->format->Gmask = TCMIPS_SDL_GMASK;
  s->format->Bmask = TCMIPS_SDL_BMASK;
  s->format->Amask = TCMIPS_SDL_AMASK;
  s->format->palette = (SDL_Palette *)(s->format + 1);
  s->format->palette->ncolors = 0;
  s->format->palette->colors = NULL;
  s->format->palette->version = 0;
  s->format->palette->refcount = 1;

  s->w = width;
  s->h = height;
  s->pitch = width * (TCMIPS_SDL_DEPTH / 8);
  s->clip_rect.x = 0;
  s->clip_rect.y = 0;
  s->clip_rect.w = width;
  s->clip_rect.h = height;
  s->refcount = 1;
  s->locked = 0;
  s->pixels = SDL_malloc((size_t)s->pitch * (size_t)height);
  if (!s->pixels) {
    SDL_free(s);
    return NULL;
  }
  SDL_memset(s->pixels, 0, (size_t)s->pitch * (size_t)height);
  return s;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                  int depth, Uint32 Rmask, Uint32 Gmask,
                                  Uint32 Bmask, Uint32 Amask) {
  (void)flags;
  (void)Rmask;
  (void)Gmask;
  (void)Bmask;
  (void)Amask;
  return SDL_CreateRGBSurfaceWithFormat(
      0, width, height, depth ? depth : TCMIPS_SDL_DEPTH, TCMIPS_SDL_FORMAT);
}

/* Surface that aliases caller-owned memory (upstream SDL_CreateRGBSurfaceFrom
 * semantics): flagged PREALLOC|DONTFREE so SDL_FreeSurface releases the
 * SDL_Surface shell but never the caller's pixels. */
SDL_Surface *tcmips_sdl2_surface_alias_rgba(void *pixels, int width, int height,
                                            int pitch) {
  if (!pixels || width <= 0 || height <= 0 || pitch < width * 4)
    return NULL;

  SDL_Surface *s = (SDL_Surface *)SDL_calloc(1, sizeof(SDL_Surface) +
                                                    sizeof(SDL_PixelFormat));
  if (!s)
    return NULL;

  s->flags = SDL_PREALLOC | SDL_DONTFREE;
  s->format = (SDL_PixelFormat *)(s + 1);
  s->format->format = TCMIPS_SDL_FORMAT;
  s->format->BitsPerPixel = (Uint8)TCMIPS_SDL_DEPTH;
  s->format->BytesPerPixel = (Uint8)(TCMIPS_SDL_DEPTH / 8);
  s->format->Rmask = TCMIPS_SDL_RMASK;
  s->format->Gmask = TCMIPS_SDL_GMASK;
  s->format->Bmask = TCMIPS_SDL_BMASK;
  s->format->Amask = TCMIPS_SDL_AMASK;

  s->w = width;
  s->h = height;
  s->pitch = pitch;
  s->clip_rect.x = 0;
  s->clip_rect.y = 0;
  s->clip_rect.w = width;
  s->clip_rect.h = height;
  s->refcount = 1;
  s->locked = 0;
  s->pixels = pixels;
  return s;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height,
                                      int depth, int pitch, Uint32 Rmask,
                                      Uint32 Gmask, Uint32 Bmask,
                                      Uint32 Amask) {
  if (depth != TCMIPS_SDL_DEPTH || Rmask != TCMIPS_SDL_RMASK ||
      Gmask != TCMIPS_SDL_GMASK || Bmask != TCMIPS_SDL_BMASK ||
      Amask != TCMIPS_SDL_AMASK) {
    SDL_SetError("SDL_CreateRGBSurfaceFrom: only 32-bpp RGBA32 is supported");
    return NULL;
  }
  return tcmips_sdl2_surface_alias_rgba(pixels, width, height, pitch);
}

SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int width,
                                                int height, int depth,
                                                int pitch, Uint32 format) {
  if (depth != TCMIPS_SDL_DEPTH || format != TCMIPS_SDL_FORMAT) {
    SDL_SetError("SDL_CreateRGBSurfaceWithFormatFrom: only 32-bpp RGBA32");
    return NULL;
  }
  return tcmips_sdl2_surface_alias_rgba(pixels, width, height, pitch);
}

void SDL_FreeSurface(SDL_Surface *surface) {
  if (!surface)
    return;
  tcmips_sdl2_ck_free(surface);
  /* PREALLOC/DONTFREE surfaces alias memory they do not own (texture
     lock-out surfaces, SDL_CreateRGBSurfaceFrom callers): never free it. */
  if (!(surface->flags & (SDL_PREALLOC | SDL_DONTFREE)) && surface->pixels) {
    SDL_free(surface->pixels);
  }
  SDL_free(surface);
}

SDL_Surface *SDL_DuplicateSurface(SDL_Surface *surface) {
  SDL_Surface *copy = NULL;
  if (!surface)
    return NULL;
  copy = SDL_CreateRGBSurfaceWithFormat(
      0, surface->w, surface->h,
      surface->format ? surface->format->BitsPerPixel : TCMIPS_SDL_DEPTH,
      surface->format ? surface->format->format : TCMIPS_SDL_FORMAT);
  if (copy && copy->pixels && surface->pixels) {
    SDL_memcpy(copy->pixels, surface->pixels,
               (size_t)surface->pitch * (size_t)surface->h);
  }
  if (copy) {
    /* The duplicate should inherit the source's colour-key (SDL2). */
    tcmips_sdl2_ck_t *src_ck = tcmips_sdl2_ck_find(surface);
    if (src_ck && src_ck->enabled) {
      tcmips_sdl2_ck_t *copy_ck = tcmips_sdl2_ck_alloc(copy);
      if (copy_ck) {
        copy_ck->enabled = SDL_TRUE;
        copy_ck->key = src_ck->key;
      }
    }
  }
  return copy;
}

int SDL_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette) {
  (void)surface;
  (void)palette;
  TCMIPS_SDL_STUB_MSG("SDL_SetSurfacePalette");
  return 0;
}

int SDL_LockSurface(SDL_Surface *surface) {
  (void)surface;
  return 0;
}

void SDL_UnlockSurface(SDL_Surface *surface) { (void)surface; }

int SDL_SetSurfaceRLE(SDL_Surface *surface, int flag) {
  (void)surface;
  (void)flag;
  return 0;
}

SDL_bool SDL_SurfaceHasRLE(SDL_Surface *surface) {
  (void)surface;
  return SDL_FALSE;
}

int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key) {
  if (!surface) {
    return -1;
  }
  /* SDL2 passes SDL_TRUE to enable, SDL_FALSE (0) to disable. */
  if (flag & SDL_TRUE) {
    tcmips_sdl2_ck_t *slot = tcmips_sdl2_ck_alloc(surface);
    if (!slot) {
      SDL_SetError("TCMIPS colour-key registry full");
      return -1;
    }
    slot->enabled = SDL_TRUE;
    slot->key = key;
  } else {
    tcmips_sdl2_ck_t *slot = tcmips_sdl2_ck_find(surface);
    if (slot) {
      slot->enabled = SDL_FALSE;
    }
  }
  return 0;
}

SDL_bool SDL_HasColorKey(SDL_Surface *surface) {
  if (!surface) {
    return SDL_FALSE;
  }
  tcmips_sdl2_ck_t *slot = tcmips_sdl2_ck_find(surface);
  return (slot && slot->enabled) ? SDL_TRUE : SDL_FALSE;
}

int SDL_GetColorKey(SDL_Surface *surface, Uint32 *key) {
  if (!surface) {
    return -1;
  }
  tcmips_sdl2_ck_t *slot = tcmips_sdl2_ck_find(surface);
  if (!slot || !slot->enabled) {
    return -1;
  }
  if (key) {
    *key = slot->key;
  }
  return 0;
}

int SDL_SetSurfaceColorMod(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b) {
  (void)surface;
  (void)r;
  (void)g;
  (void)b;
  return 0;
}

int SDL_GetSurfaceColorMod(SDL_Surface *surface, Uint8 *r, Uint8 *g, Uint8 *b) {
  (void)surface;
  if (r)
    *r = 255;
  if (g)
    *g = 255;
  if (b)
    *b = 255;
  return 0;
}

int SDL_SetSurfaceAlphaMod(SDL_Surface *surface, Uint8 alpha) {
  (void)surface;
  (void)alpha;
  return 0;
}

int SDL_GetSurfaceAlphaMod(SDL_Surface *surface, Uint8 *alpha) {
  (void)surface;
  if (alpha)
    *alpha = 255;
  return 0;
}

int SDL_SetSurfaceBlendMode(SDL_Surface *surface, SDL_BlendMode blendMode) {
  (void)surface;
  (void)blendMode;
  return 0;
}

int SDL_GetSurfaceBlendMode(SDL_Surface *surface, SDL_BlendMode *blendMode) {
  (void)surface;
  if (blendMode)
    *blendMode = SDL_BLENDMODE_NONE;
  return 0;
}

SDL_bool SDL_SetClipRect(SDL_Surface *surface, const SDL_Rect *rect) {
  if (!surface)
    return SDL_FALSE;
  if (rect) {
    /* Upstream SDL2 clamps the clip rect against the surface bounds.  This
     * matters for safety: the renderer's pixel writes only test clip_rect
     * (not the surface bounds), so an out-of-surface clip would turn any
     * drawing primitive into an out-of-bounds write. */
    SDL_Rect full = {0, 0, surface->w, surface->h};
    SDL_Rect clipped;
    if (!SDL_IntersectRect(rect, &full, &clipped)) {
      /* Entirely outside the surface: keep the previous clip and report
       * failure, mirroring upstream behaviour. */
      return SDL_FALSE;
    }
    surface->clip_rect = clipped;
  } else {
    surface->clip_rect.x = 0;
    surface->clip_rect.y = 0;
    surface->clip_rect.w = surface->w;
    surface->clip_rect.h = surface->h;
  }
  return SDL_TRUE;
}

void SDL_GetClipRect(SDL_Surface *surface, SDL_Rect *rect) {
  if (surface && rect) {
    *rect = surface->clip_rect;
  }
}

void tcmips_sdl2_pixel_to_rgba(Uint32 pixel, Uint8 *r, Uint8 *g, Uint8 *b,
                               Uint8 *a) {
  *r = (Uint8)(pixel & 0xFF);
  *g = (Uint8)((pixel >> 8) & 0xFF);
  *b = (Uint8)((pixel >> 16) & 0xFF);
  *a = (Uint8)((pixel >> 24) & 0xFF);
}

Uint32 tcmips_sdl2_rgba_to_pixel(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  return ((Uint32)r) | (((Uint32)g) << 8) | (((Uint32)b) << 16) |
         (((Uint32)a) << 24);
}

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color) {
  if (!dst || !dst->pixels)
    return -1;

  SDL_Rect fill = dst->clip_rect;
  if (rect) {
    SDL_Rect tmp;
    if (!SDL_IntersectRect(rect, &fill, &tmp)) {
      return 0;
    }
    fill = tmp;
  }

  /* Per the official SDL contract, SDL_FillRect stores the given pixel value
   * (including its alpha channel) into every pixel of the filled region; it
   * must NOT discard the alpha.  Keeping the alpha byte intact means colours
   * produced by SDL_MapRGB (alpha = 255) land in VRAM as opaque. */
  Uint32 c = color;

  Uint32 *pixels = (Uint32 *)dst->pixels;
  int pitch = dst->pitch >> 2;
  for (int y = fill.y; y < fill.y + fill.h; ++y) {
    for (int x = fill.x; x < fill.x + fill.w; ++x) {
      pixels[y * pitch + x] = c;
    }
  }
  return 0;
}

int SDL_FillRects(SDL_Surface *dst, const SDL_Rect *rects, int count,
                  Uint32 color) {
  if (!rects || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_FillRect(dst, &rects[i], color);
  }
  return 0;
}

static int tcmips_sdl2_clip_rect(const SDL_Rect *a, const SDL_Rect *b,
                                 SDL_Rect *out) {
  int x1 = a->x > b->x ? a->x : b->x;
  int x2 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
  int y1 = a->y > b->y ? a->y : b->y;
  int y2 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
  if (x2 <= x1 || y2 <= y1) {
    out->x = out->y = out->w = out->h = 0;
    return 0;
  }
  out->x = x1;
  out->y = y1;
  out->w = x2 - x1;
  out->h = y2 - y1;
  return 1;
}

int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst,
                  SDL_Rect *dstrect) {
  if (!src || !dst || !src->pixels || !dst->pixels)
    return -1;

  SDL_Rect src_r = {0, 0, src->w, src->h};
  if (srcrect)
    src_r = *srcrect;

  SDL_Rect dst_r = {0, 0, src_r.w, src_r.h};
  if (dstrect) {
    if (dstrect->w != 0)
      dst_r.w = dstrect->w;
    if (dstrect->h != 0)
      dst_r.h = dstrect->h;
    dst_r.x = dstrect->x;
    dst_r.y = dstrect->y;
  }

  SDL_Rect clipped;
  if (!tcmips_sdl2_clip_rect(&dst_r, &dst->clip_rect, &clipped)) {
    if (dstrect)
      *dstrect = dst_r;
    return 0;
  }
  int dx = clipped.x - dst_r.x;
  int dy = clipped.y - dst_r.y;
  SDL_Rect actual_src = {src_r.x + dx, src_r.y + dy, clipped.w, clipped.h};
  if (!tcmips_sdl2_clip_rect(&actual_src, &src_r, &actual_src)) {
    if (dstrect)
      *dstrect = dst_r;
    return 0;
  }
  /* Clamp the source read region into the source surface as well, mirroring
     the offset back into the destination.  A negative srcrect origin maps
     actual_src outside [0, src->w]x[0, src->h]; both the source read region
     and the destination write region must shift in lockstep (1:1). */
  {
    int sx0 = actual_src.x;
    int sy0 = actual_src.y;
    SDL_Rect src_bounds = {0, 0, src->w, src->h};
    if (!tcmips_sdl2_clip_rect(&actual_src, &src_bounds, &actual_src)) {
      if (dstrect)
        *dstrect = dst_r;
      return 0;
    }
    clipped.x += actual_src.x - sx0;
    clipped.y += actual_src.y - sy0;
    clipped.w = actual_src.w;
    clipped.h = actual_src.h;
  }

  int sx = actual_src.x;
  int sy = actual_src.y;
  int dx0 = clipped.x;
  int dy0 = clipped.y;
  int w = actual_src.w;
  int h = actual_src.h;

  Uint32 *sp = (Uint32 *)src->pixels;
  Uint32 *dp = (Uint32 *)dst->pixels;
  int spitch = src->pitch >> 2;
  int dpitch = dst->pitch >> 2;

  /* Colour-key transparency: skip source pixels equal to the key. */
  tcmips_sdl2_ck_t *ck = tcmips_sdl2_ck_find(src);
  int use_ck = (ck && ck->enabled) ? 1 : 0;
  Uint32 ckey = use_ck ? ck->key : 0;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      Uint32 srcpx = sp[(sy + y) * spitch + (sx + x)];
      if (use_ck && (srcpx & 0x00FFFFFFu) == (ckey & 0x00FFFFFFu)) {
        continue; /* transparent: keep existing destination pixel */
      }
      dp[(dy0 + y) * dpitch + (dx0 + x)] = srcpx;
    }
  }

  if (dstrect)
    *dstrect = dst_r;
  return 0;
}

/* Compute source/Destination coordinates such that srcrect/dstrect both fit
   into the surface (clipping to the surfaces' clip rectangles).  Caller still
   needs to handle the pitch-overflow case for negative coordinates. */
static int
tcmips_sdl2_prepare_blit_scaled(const SDL_Surface *src, const SDL_Rect *srcrect,
                                SDL_Surface *dst, const SDL_Rect *dstrect,
                                SDL_Rect *src_out, SDL_Rect *dst_out) {
  if (!src || !dst)
    return -1;
  SDL_Rect src_r = {0, 0, src->w, src->h};
  if (srcrect)
    src_r = *srcrect;
  SDL_Rect dst_r;
  if (dstrect) {
    dst_r = *dstrect;
    if (dst_r.w == 0)
      dst_r.w = src_r.w;
    if (dst_r.h == 0)
      dst_r.h = src_r.h;
  } else {
    dst_r.x = dst_r.y = 0;
    dst_r.w = src_r.w;
    dst_r.h = src_r.h;
  }

  /* Intersection against clip_rect for dst. */
  SDL_Rect clip = dst->clip_rect;
  int cx1 = dst_r.x > clip.x ? dst_r.x : clip.x;
  int cy1 = dst_r.y > clip.y ? dst_r.y : clip.y;
  int cx2 = (dst_r.x + dst_r.w) < (clip.x + clip.w) ? (dst_r.x + dst_r.w)
                                                    : (clip.x + clip.w);
  int cy2 = (dst_r.y + dst_r.h) < (clip.y + clip.h) ? (dst_r.y + dst_r.h)
                                                    : (clip.y + clip.h);
  int cw = cx2 - cx1;
  int ch = cy2 - cy1;
  if (cw <= 0 || ch <= 0)
    return 0;

  /* Map dst clipping back into source space. */
  int sw = src_r.w;
  int sh = src_r.h;
  int sx0 = src_r.x + (cx1 - dst_r.x) * sw / dst_r.w;
  int sy0 = src_r.y + (cy1 - dst_r.y) * sh / dst_r.h;
  int new_sw = cw * sw / dst_r.w;
  int new_sh = ch * sh / dst_r.h;
  if (new_sw <= 0)
    new_sw = 1;
  if (new_sh <= 0)
    new_sh = 1;

  /* Clamp the source rectangle into the source surface on all four edges.
     The right/bottom clamp alone is insufficient: a negative srcrect origin
     (sx0/sy0 < 0) must also be clipped, otherwise the source pointer reads
     before the pixel buffer.  Crucially, every source-space clamp must be
     reflected into the destination rectangle in the same proportion
     (dst_r.w : sw), otherwise the src->dst scale is distorted and the blit
     stretches/misplaces content. */
  if (sx0 < 0) {
    int clipped = -sx0;
    int dst_clipped = clipped * dst_r.w / sw;
    sx0 = 0;
    new_sw -= clipped;
    cx1 += dst_clipped;
    cw -= dst_clipped;
  }
  if (sy0 < 0) {
    int clipped = -sy0;
    int dst_clipped = clipped * dst_r.h / sh;
    sy0 = 0;
    new_sh -= clipped;
    cy1 += dst_clipped;
    ch -= dst_clipped;
  }
  if (sx0 + new_sw > src->w) {
    int clipped = (sx0 + new_sw) - src->w;
    int dst_clipped = clipped * dst_r.w / sw;
    new_sw -= clipped;
    cw -= dst_clipped;
  }
  if (sy0 + new_sh > src->h) {
    int clipped = (sy0 + new_sh) - src->h;
    int dst_clipped = clipped * dst_r.h / sh;
    new_sh -= clipped;
    ch -= dst_clipped;
  }
  if (new_sw <= 0 || new_sh <= 0 || cw <= 0 || ch <= 0)
    return 0;

  src_out->x = sx0;
  src_out->y = sy0;
  src_out->w = new_sw;
  src_out->h = new_sh;
  dst_out->x = cx1;
  dst_out->y = cy1;
  dst_out->w = cw;
  dst_out->h = ch;
  return 1;
}

int SDL_UpperBlitScaled(SDL_Surface *src, const SDL_Rect *srcrect,
                        SDL_Surface *dst, SDL_Rect *dstrect) {
  if (!src || !dst || !src->pixels || !dst->pixels)
    return -1;

  SDL_Rect src_r, dst_r;
  if (!tcmips_sdl2_prepare_blit_scaled(src, srcrect, dst, dstrect, &src_r,
                                       &dst_r)) {
    return 0;
  }

  /* Colour-key transparency skips source pixels equal to the key; in that
     case the per-pixel scaled loop must be used (memcpy row-copy below
     cannot skip individual pixels). */
  tcmips_sdl2_ck_t *ck = tcmips_sdl2_ck_find(src);
  int use_ck = (ck && ck->enabled) ? 1 : 0;
  Uint32 ckey = use_ck ? ck->key : 0;

  /* TCMIPS SDL2 surfaces are always 32-bpp RGBA, so when the logical width
     matches the destination width (horizontal 1:1, only vertical scaling),
     each source row can be copied verbatim with memcpy instead of the
     per-pixel nearest-neighbour loop. */
  if (src_r.w == dst_r.w && !use_ck) {
    int sw = src_r.h;
    int dw = dst_r.w;
    int dh = dst_r.h;
    if (dw <= 0 || dh <= 0)
      return 0;
    const Uint8 *src_base = (const Uint8 *)src->pixels;
    Uint8 *dst_base = (Uint8 *)dst->pixels;
    size_t row_bytes = (size_t)dw * 4u;
    for (int y = 0; y < dh; ++y) {
      int sy = src_r.y + (y * sw) / dh;
      if (sy >= src->h)
        sy = src->h - 1;
      SDL_memcpy(dst_base + ((size_t)(dst_r.y + y) * (size_t)dst->pitch +
                             (size_t)dst_r.x * 4u),
                 src_base +
                     ((size_t)sy * (size_t)src->pitch + (size_t)src_r.x * 4u),
                 row_bytes);
    }
    if (dstrect) {
      *dstrect = dst_r;
    }
    return 0;
  }

  const Uint32 *sp = (const Uint32 *)src->pixels;
  Uint32 *dp = (Uint32 *)dst->pixels;
  int spitch = src->pitch >> 2;
  int dpitch = dst->pitch >> 2;

  /* Nearest-neighbour blit from src_r -> dst_r. */
  int sw = src_r.w;
  int sh = src_r.h;
  int dw = dst_r.w;
  int dh = dst_r.h;
  if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
    return 0;

  for (int y = 0; y < dh; ++y) {
    int sy = src_r.y + (y * sh) / dh;
    if (sy >= src->h)
      sy = src->h - 1;
    for (int x = 0; x < dw; ++x) {
      int sx = src_r.x + (x * sw) / dw;
      if (sx >= src->w)
        sx = src->w - 1;
      Uint32 srcpx = sp[sy * spitch + sx];
      if (use_ck && (srcpx & 0x00FFFFFFu) == (ckey & 0x00FFFFFFu)) {
        continue; /* transparent: keep existing destination pixel */
      }
      dp[(dst_r.y + y) * dpitch + (dst_r.x + x)] = srcpx;
    }
  }

  if (dstrect) {
    dstrect->x = dst_r.x;
    dstrect->y = dst_r.y;
    dstrect->w = dst_r.w;
    dstrect->h = dst_r.h;
  }
  return 0;
}

int SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst,
                    const SDL_Rect *dstrect) {
  return SDL_UpperBlitScaled(src, srcrect, dst, (SDL_Rect *)dstrect);
}

int SDL_SoftStretchLinear(SDL_Surface *src, const SDL_Rect *srcrect,
                          SDL_Surface *dst, const SDL_Rect *dstrect) {
  return SDL_SoftStretch(src, srcrect, dst, dstrect);
}

// We only support same-format conversion for now.
SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, const SDL_PixelFormat *fmt,
                                Uint32 flags) {
  (void)fmt;
  (void)flags;
  if (!src)
    return NULL;
  return SDL_DuplicateSurface(src);
}

SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 pixel_format,
                                      Uint32 flags) {
  (void)pixel_format;
  (void)flags;
  if (!src)
    return NULL;
  return SDL_DuplicateSurface(src);
}

int SDL_ConvertPixels(int width, int height, Uint32 src_format, const void *src,
                      int src_pitch, Uint32 dst_format, void *dst,
                      int dst_pitch) {
  if (!src || !dst || width <= 0 || height <= 0)
    return -1;

  /* This layer does a byte copy, not a real format conversion, so it only
     supports identical source/destination formats.  Derive the row size from
     the actual bytes-per-pixel instead of hard-coding 32bpp. */
  int src_bpp = SDL_BYTESPERPIXEL(src_format);
  int dst_bpp = SDL_BYTESPERPIXEL(dst_format);
  if (src_format != dst_format || src_bpp <= 0 || dst_bpp <= 0)
    return -1;

  int row_bytes = width * src_bpp;
  /* A row stride (absolute value) smaller than a row would overrun the
     buffer on subsequent rows; negative pitch (bottom-up) is legal SDL and
     handled by the y * pitch offset below. */
  int src_stride = src_pitch < 0 ? -src_pitch : src_pitch;
  int dst_stride = dst_pitch < 0 ? -dst_pitch : dst_pitch;
  if (src_stride < row_bytes || dst_stride < row_bytes)
    return -1;

  if (src_pitch == dst_pitch && row_bytes == src_pitch) {
    SDL_memcpy(dst, src, (size_t)row_bytes * (size_t)height);
  } else {
    for (int y = 0; y < height; ++y) {
      SDL_memcpy((Uint8 *)dst + y * dst_pitch,
                 (const Uint8 *)src + y * src_pitch, row_bytes);
    }
  }
  return 0;
}

int SDL_PremultiplyAlpha(int width, int height, Uint32 src_format,
                         const void *src, int src_pitch, Uint32 dst_format,
                         void *dst, int dst_pitch) {
  // Simple copy: real premultiplication would unpack/multiply/repack.
  return SDL_ConvertPixels(width, height, src_format, src, src_pitch,
                           dst_format, dst, dst_pitch);
}

int SDL_SetSurfaceColorKeyAndColorMod(SDL_Surface *surface, SDL_bool flag,
                                      Uint32 key) {
  return SDL_SetColorKey(surface, flag, key);
}

Uint32 SDL_MasksToPixelFormatEnum(int bpp, Uint32 Rmask, Uint32 Gmask,
                                  Uint32 Bmask, Uint32 Amask) {
  if (bpp == TCMIPS_SDL_DEPTH && Rmask == TCMIPS_SDL_RMASK &&
      Gmask == TCMIPS_SDL_GMASK && Bmask == TCMIPS_SDL_BMASK &&
      Amask == TCMIPS_SDL_AMASK) {
    return TCMIPS_SDL_FORMAT;
  }
  return SDL_PIXELFORMAT_UNKNOWN;
}

SDL_bool SDL_PixelFormatEnumToMasks(Uint32 format, int *bpp, Uint32 *Rmask,
                                    Uint32 *Gmask, Uint32 *Bmask,
                                    Uint32 *Amask) {
  if (!bpp || !Rmask || !Gmask || !Bmask || !Amask)
    return SDL_FALSE;
  if (format == TCMIPS_SDL_FORMAT) {
    *bpp = TCMIPS_SDL_DEPTH;
    *Rmask = TCMIPS_SDL_RMASK;
    *Gmask = TCMIPS_SDL_GMASK;
    *Bmask = TCMIPS_SDL_BMASK;
    *Amask = TCMIPS_SDL_AMASK;
    return SDL_TRUE;
  }
  *bpp = 0;
  *Rmask = *Gmask = *Bmask = *Amask = 0;
  return SDL_FALSE;
}

// Map RGBA values into the canonical TCMIPS pixel format.
// Per the official SDL contract, SDL_MapRGB maps an RGB triple to an *opaque*
// pixel value, i.e. it is equivalent to SDL_MapRGBA(format, r, g, b,
// SDL_ALPHA_OPAQUE).  The alpha channel must therefore be filled with 255,
// never 0 — otherwise the produced pixel reads as fully transparent under
// SDL_BLENDMODE_BLEND (see tcmips_sdl2_blend_over) and is discarded.
Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b) {
  (void)format;
  return tcmips_sdl2_rgba_to_pixel(r, g, b, SDL_ALPHA_OPAQUE);
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b,
                   Uint8 a) {
  (void)format;
  return tcmips_sdl2_rgba_to_pixel(r, g, b, a);
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r, Uint8 *g,
                Uint8 *b) {
  (void)format;
  Uint8 unused_a;
  tcmips_sdl2_pixel_to_rgba(pixel, r, g, b, &unused_a);
}

void SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *format, Uint8 *r,
                 Uint8 *g, Uint8 *b, Uint8 *a) {
  (void)format;
  tcmips_sdl2_pixel_to_rgba(pixel, r, g, b, a);
}

int SDL_SetPixelFormatPalette(SDL_PixelFormat *format, SDL_Palette *palette) {
  (void)format;
  (void)palette;
  TCMIPS_SDL_STUB_MSG("SDL_SetPixelFormatPalette");
  return -1;
}

int SDL_SetPaletteColors(SDL_Palette *palette, const SDL_Color *colors,
                         int firstcolor, int ncolors) {
  (void)palette;
  (void)colors;
  (void)firstcolor;
  (void)ncolors;
  TCMIPS_SDL_STUB_MSG("SDL_SetPaletteColors");
  return 0;
}

SDL_Palette *SDL_AllocPalette(int ncolors) {
  (void)ncolors;
  TCMIPS_SDL_STUB_MSG("SDL_AllocPalette");
  return NULL;
}

void SDL_FreePalette(SDL_Palette *palette) { (void)palette; }

SDL_PixelFormat *SDL_AllocFormat(Uint32 pixel_format) {
  (void)pixel_format;
  TCMIPS_SDL_STUB_MSG("SDL_AllocFormat");
  return NULL;
}

void SDL_FreeFormat(SDL_PixelFormat *format) { (void)format; }

// Shape API is unsupported.
SDL_Window *SDL_CreateShapedWindow(const char *title, unsigned int x,
                                   unsigned int y, unsigned int w,
                                   unsigned int h, Uint32 flags) {
  (void)title;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)flags;
  TCMIPS_SDL_STUB_MSG("SDL_CreateShapedWindow");
  return NULL;
}

SDL_bool SDL_IsShapedWindow(const SDL_Window *window) {
  (void)window;
  return SDL_FALSE;
}

int SDL_SetWindowShape(SDL_Window *window, SDL_Surface *shape,
                       SDL_WindowShapeMode *shape_mode) {
  (void)window;
  (void)shape;
  (void)shape_mode;
  TCMIPS_SDL_STUB_MSG("SDL_SetWindowShape");
  return -1;
}

int SDL_GetShapedWindowMode(SDL_Window *window,
                            SDL_WindowShapeMode *shape_mode) {
  (void)window;
  (void)shape_mode;
  return -1;
}

/* ---- BMP loading / saving ----
 * BMP files store pixels little-endian as BGR(A) bytes.  TCMIPS SDL2
 * surfaces are fixed RGBA32 (host little-endian, R in byte 0), so loading
 * decodes BGR(A) -> RGBA32 and saving converts back.  Supports 8 / 24 / 32
 * bits-per-pixel scanlines; the 8-bit palette is resolved to RGBA32 pixels.
 */

SDL_Surface *SDL_LoadBMP_RW(SDL_RWops *src, int freesrc) {
  if (!src) {
    if (freesrc)
      SDL_RWclose(src);
    return NULL;
  }

  Uint8 fh[14];
  if (SDL_RWread(src, fh, 1, 14) != 14)
    goto fail;
  if (fh[0] != 'B' || fh[1] != 'M')
    goto fail;
  Uint32 off_bits = ((Uint32)fh[10]) | (((Uint32)fh[11]) << 8) |
                    (((Uint32)fh[12]) << 16) | (((Uint32)fh[13]) << 24);

  Uint8 dib[40];
  if (SDL_RWread(src, dib, 1, 40) != 40)
    goto fail;
  Uint32 dib_size = ((Uint32)dib[0]) | (((Uint32)dib[1]) << 8) |
                    (((Uint32)dib[2]) << 16) | (((Uint32)dib[3]) << 24);
  if (dib_size < 40)
    goto fail;
  if (dib_size > 40) {
    if (SDL_RWseek(src, (Sint64)(dib_size - 40), RW_SEEK_CUR) < 0)
      goto fail;
  }

  Sint32 w = (Sint32)(((Uint32)dib[4]) | ((Uint32)dib[5] << 8) |
                      ((Uint32)dib[6] << 16) | ((Uint32)dib[7] << 24));
  Sint32 h = (Sint32)(((Uint32)dib[8]) | ((Uint32)dib[9] << 8) |
                      ((Uint32)dib[10] << 16) | ((Uint32)dib[11] << 24));
  Uint16 planes = (Uint16)(((Uint16)dib[12]) | ((Uint16)dib[13] << 8));
  Uint16 bpp = (Uint16)(((Uint16)dib[14]) | ((Uint16)dib[15] << 8));
  Uint32 compression = ((Uint32)dib[16]) | ((Uint32)dib[17] << 8) |
                       ((Uint32)dib[18] << 16) | ((Uint32)dib[19] << 24);
  Uint32 colors_used = ((Uint32)dib[32]) | ((Uint32)dib[33] << 8) |
                       ((Uint32)dib[34] << 16) | ((Uint32)dib[35] << 24);

  if (planes != 1 || compression != 0 || w <= 0 || h == 0)
    goto fail;
  int top_down = (h < 0);
  int height = top_down ? -h : h;

  if (bpp != 8 && bpp != 24 && bpp != 32)
    goto fail;

  int pal_size = 0;
  Uint8 palette[256][4];
  SDL_memset(palette, 0,
             sizeof(palette)); /* zero-fill so out-of-range indexes are safe */
  if (bpp == 8) {
    pal_size = colors_used ? (int)colors_used : 256;
    if (pal_size > 256)
      pal_size = 256;
    if (SDL_RWread(src, palette, 4, (size_t)pal_size) != (size_t)pal_size)
      goto fail;
  }

  if (SDL_RWseek(src, (Sint64)off_bits, RW_SEEK_SET) < 0)
    goto fail;

  int raw_row = (bpp == 8)    ? ((w + 3) & ~3)
                : (bpp == 24) ? ((w * 3 + 3) & ~3)
                              : (w * 4);
  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
      0, w, height, TCMIPS_SDL_DEPTH, TCMIPS_SDL_FORMAT);
  if (!surf)
    goto fail;

  Uint8 *row = (Uint8 *)SDL_malloc((size_t)raw_row);
  if (!row) {
    SDL_FreeSurface(surf);
    goto fail;
  }

  Uint32 *pix = (Uint32 *)surf->pixels;
  for (int r = 0; r < height; ++r) {
    if (SDL_RWread(src, row, 1, (size_t)raw_row) != (size_t)raw_row) {
      SDL_free(row);
      SDL_FreeSurface(surf);
      goto fail;
    }
    int dst_y = top_down ? r : (height - 1 - r);
    Uint32 *srow = pix + (size_t)dst_y * (surf->pitch >> 2);
    if (bpp == 8) {
      for (int x = 0; x < w; ++x) {
        Uint8 idx = row[x];
        Uint8 pr = palette[idx][2], pg = palette[idx][1], pb = palette[idx][0];
        srow[x] = tcmips_sdl2_rgba_to_pixel(pr, pg, pb, 255);
      }
    } else if (bpp == 24) {
      for (int x = 0; x < w; ++x) {
        Uint8 pb = row[x * 3 + 0], pg = row[x * 3 + 1], pr = row[x * 3 + 2];
        srow[x] = tcmips_sdl2_rgba_to_pixel(pr, pg, pb, 255);
      }
    } else { /* 32 */
      for (int x = 0; x < w; ++x) {
        Uint8 pb = row[x * 4 + 0], pg = row[x * 4 + 1], pr = row[x * 4 + 2];
        /* BI_RGB 32-bit pixels carry an undefined X byte in file byte 3.
           The TCMIPS layer treats every 32-bpp surface as RGBA32, so the
           X byte must load as opaque — treating it as alpha made whole
           images vanish under SDL_BLENDMODE_BLEND when the file padding
           happened to be zero. */
        srow[x] = tcmips_sdl2_rgba_to_pixel(pr, pg, pb, 255);
      }
    }
  }
  SDL_free(row);

  if (freesrc)
    SDL_RWclose(src);
  return surf;

fail:
  if (freesrc)
    SDL_RWclose(src);
  return NULL;
}

int SDL_SaveBMP_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst) {
  if (!surface || !dst || !surface->pixels) {
    if (freedst)
      SDL_RWclose(dst);
    return -1;
  }

  int w = surface->w;
  int h = surface->h;
  int raw_row = (w * 3 + 3) & ~3; /* save as 24-bpp BMP, 4-byte row pad */

  Uint8 *row = (Uint8 *)SDL_malloc((size_t)raw_row);
  if (!row) {
    if (freedst)
      SDL_RWclose(dst);
    return -1;
  }

  Uint32 file_size = 14 + 40 + (Uint32)raw_row * (Uint32)h;
  Uint32 off = 14 + 40;

  Uint8 hdr[14];
  hdr[0] = 'B';
  hdr[1] = 'M';
  hdr[2] = (Uint8)file_size;
  hdr[3] = (Uint8)(file_size >> 8);
  hdr[4] = (Uint8)(file_size >> 16);
  hdr[5] = (Uint8)(file_size >> 24);
  hdr[6] = 0;
  hdr[7] = 0;
  hdr[8] = (Uint8)off;
  hdr[9] = (Uint8)(off >> 8);
  hdr[10] = (Uint8)(off >> 16);
  hdr[11] = (Uint8)(off >> 24);
  hdr[12] = 0;
  hdr[13] = 0;
  if (SDL_RWwrite(dst, hdr, 1, 14) != 14)
    goto bail;

  Uint8 dih[40];
  SDL_memset(dih, 0, sizeof(dih));
  dih[0] = 40;
  dih[4] = (Uint8)w;
  dih[5] = (Uint8)(w >> 8);
  dih[6] = (Uint8)(w >> 16);
  dih[7] = (Uint8)(w >> 24);
  dih[8] = (Uint8)h;
  dih[9] = (Uint8)(h >> 8);
  dih[10] = (Uint8)(h >> 16);
  dih[11] = (Uint8)(h >> 24);
  dih[12] = 1;  /* planes */
  dih[14] = 24; /* bpp */
  if (SDL_RWwrite(dst, dih, 1, 40) != 40)
    goto bail;

  const Uint32 *pix = (const Uint32 *)surface->pixels;
  for (int r = 0; r < h; ++r) {
    int src_y = h - 1 - r; /* BMP is bottom-up */
    const Uint32 *srow = pix + (size_t)src_y * (surface->pitch >> 2);
    for (int x = 0; x < w; ++x) {
      Uint8 r8, g8, b8, a8;
      tcmips_sdl2_pixel_to_rgba(srow[x], &r8, &g8, &b8, &a8);
      row[x * 3 + 0] = b8; /* B */
      row[x * 3 + 1] = g8; /* G */
      row[x * 3 + 2] = r8; /* R */
    }
    int pad = raw_row - w * 3;
    SDL_memset(row + (size_t)w * 3, 0, (size_t)pad);
    if (SDL_RWwrite(dst, row, 1, (size_t)raw_row) != (size_t)raw_row)
      goto bail;
  }

  SDL_free(row);
  if (freedst)
    SDL_RWclose(dst);
  return 0;

bail:
  SDL_free(row);
  if (freedst)
    SDL_RWclose(dst);
  return -1;
}
