#include "tcmips_sdl2_internal.h"
#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#include <math.h>
#pragma GCC diagnostic pop

// Hard-coded to match the TCMIPS hardware backbuffer.
#define TCMIPS_SDL_FORMAT SDL_PIXELFORMAT_RGBA32

// Renderer state.
#define TCMIPS_SDL_MAX_TEXTURES 16

/* Forward declaration: source-over blend helper defined below, used by the
   per-pixel draw helpers above it. */
static Uint32 tcmips_sdl2_blend_over(Uint32 dst, Uint32 src);
static Uint32 tcmips_sdl2_blend_add(Uint32 dst, Uint32 src);
static Uint32 tcmips_sdl2_blend_mod(Uint32 dst, Uint32 src);
static int tcmips_sdl2_render_copy_impl(SDL_Texture *texture,
                                        const SDL_Rect *srcrect,
                                        const SDL_FRect *dstrect);

struct SDL_Texture {
  Uint32 format;
  int w, h;
  int access;
  void *pixels;
  int pitch;
  Uint8 colormod_r, colormod_g, colormod_b, colormod_a;
  SDL_BlendMode blend;
  SDL_ScaleMode scale;
  void *userdata;
  int refcount;
};

struct SDL_Renderer {
  SDL_Window *window;
  SDL_Surface *target;
  SDL_bool integer_scale;
  float scale_x;
  float scale_y;
  SDL_Rect viewport;
  SDL_bool clip_enabled;
  SDL_Rect clip;
  SDL_BlendMode blend;
  Uint8 r, g, b, a;
  SDL_bool vsync;
  SDL_Texture *render_target;
};

static struct SDL_Renderer s_renderer;
static struct SDL_Texture s_textures[TCMIPS_SDL_MAX_TEXTURES];

/* Draw coordinates are viewport-relative and scaled by the render scale
 * (SDL2 semantics for DrawPoint / DrawLine / FillRect / DrawRect). */
static int tcmips_sdl2_map_draw_x(const struct SDL_Renderer *ren, float x) {
  return (int)roundf((float)ren->viewport.x + x * ren->scale_x);
}

static int tcmips_sdl2_map_draw_y(const struct SDL_Renderer *ren, float y) {
  return (int)roundf((float)ren->viewport.y + y * ren->scale_y);
}

// Helper to get the "target" surface we render into.
// TCMIPS only supports the window surface as render target;
// SDL_RenderTargetSupported returns SDL_FALSE and SDL_SetRenderTarget is a
// stub.
static SDL_Surface *tcmips_sdl2_render_target(void) {
  return g_tcmips_sdl.win.surface;
}

static int tcmips_sdl2_clip_line(SDL_Rect *clip, int *x1, int *y1, int *x2,
                                 int *y2) {
  // Liang-Barsky line clipping against clip.
  float t0 = 0.0f, t1 = 1.0f;
  int dx = *x2 - *x1, dy = *y2 - *y1;
  int p[4] = {-dx, dx, -dy, dy};
  int q[4] = {*x1 - clip->x, clip->x + clip->w - *x1, *y1 - clip->y,
              clip->y + clip->h - *y1};
  for (int i = 0; i < 4; ++i) {
    if (p[i] == 0) {
      if (q[i] < 0)
        return 0;
      continue;
    }
    float t = (float)q[i] / (float)p[i];
    if (p[i] < 0) {
      if (t > t1)
        return 0;
      if (t > t0)
        t0 = t;
    } else {
      if (t < t0)
        return 0;
      if (t < t1)
        t1 = t;
    }
  }
  int nx1 = *x1 + (int)(t0 * dx);
  int ny1 = *y1 + (int)(t0 * dy);
  int nx2 = *x1 + (int)(t1 * dx);
  int ny2 = *y1 + (int)(t1 * dy);
  *x1 = nx1;
  *y1 = ny1;
  *x2 = nx2;
  *y2 = ny2;
  return 1;
}

static void tcmips_sdl2_render_set_pixel(int x, int y, Uint32 color) {
  SDL_Surface *t = tcmips_sdl2_render_target();
  if (!t || !t->pixels)
    return;
  if (x < t->clip_rect.x || y < t->clip_rect.y ||
      x >= t->clip_rect.x + t->clip_rect.w ||
      y >= t->clip_rect.y + t->clip_rect.h) {
    return;
  }
  Uint32 *pixels = (Uint32 *)t->pixels;
  Uint32 idx = (Uint32)(y * (t->pitch >> 2) + x);
  switch (s_renderer.blend) {
  case SDL_BLENDMODE_BLEND:
    pixels[idx] = tcmips_sdl2_blend_over(pixels[idx], color);
    break;
  case SDL_BLENDMODE_ADD:
    pixels[idx] = tcmips_sdl2_blend_add(pixels[idx], color);
    break;
  case SDL_BLENDMODE_MOD:
    pixels[idx] = tcmips_sdl2_blend_mod(pixels[idx], color);
    break;
  default:
    pixels[idx] = color;
    break;
  }
}

static Uint32 tcmips_sdl2_render_color(void) {
  Uint8 a = s_renderer.a;
  if (s_renderer.blend == SDL_BLENDMODE_NONE)
    a = 255;
  return (((Uint32)s_renderer.r)) | (((Uint32)s_renderer.g) << 8) |
         (((Uint32)s_renderer.b) << 16) | (((Uint32)a) << 24);
}

static void tcmips_sdl2_render_line(int x0, int y0, int x1, int y1,
                                    Uint32 color) {
  SDL_Surface *target = tcmips_sdl2_render_target();
  if (!target)
    return;
  SDL_Rect *clip = &target->clip_rect;
  if (!tcmips_sdl2_clip_line(clip, &x0, &y0, &x1, &y1))
    return;

  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (1) {
    tcmips_sdl2_render_set_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static void tcmips_sdl2_render_rect_fill(const SDL_Rect *rect, Uint32 color) {
  SDL_Surface *t = tcmips_sdl2_render_target();
  SDL_Rect r;
  if (!SDL_IntersectRect(rect, &t->clip_rect, &r))
    return;
  if (r.w <= 0 || r.h <= 0)
    return;

  /* Clamp the intersected rectangle into the framebuffer bounds.  The
   * clip_rect origin (and/or the incoming rect) may be negative, in which
   * case SDL_IntersectRect can yield a negative r.x/r.y.  The fast row-fill
   * path below does a raw pointer write (`pixels + y*pitch + r.x`) with no
   * per-pixel bounds check, so a negative coordinate would write before the
   * start of the pixel buffer (undefined behaviour / heap corruption). */
  if (r.x < 0) {
    r.w += r.x; /* shrink from the left */
    r.x = 0;
  }
  if (r.y < 0) {
    r.h += r.y; /* shrink from the top */
    r.y = 0;
  }
  if (r.x + r.w > t->w)
    r.w = t->w - r.x;
  if (r.y + r.h > t->h)
    r.h = t->h - r.y;
  if (r.w <= 0 || r.h <= 0)
    return;

  Uint32 *pixels = (Uint32 *)t->pixels;
  int pitch = t->pitch >> 2; /* /4 */
  if (s_renderer.blend == SDL_BLENDMODE_NONE) {
    /* Flat, opaque: fill rows directly without per-pixel clip/blend tests. */
    for (int y = r.y; y < r.y + r.h; ++y) {
      Uint32 *row = pixels + (size_t)y * (size_t)pitch + r.x;
      for (int x = 0; x < r.w; ++x)
        row[x] = color;
    }
    return;
  }
  /* BLEND / ADD / MOD need the destination pixel, so take the per-pixel
   * path; render_set_pixel applies the draw blend. */
  for (int y = r.y; y < r.y + r.h; ++y)
    for (int x = r.x; x < r.x + r.w; ++x)
      tcmips_sdl2_render_set_pixel(x, y, color);
}

static void tcmips_sdl2_render_rect_draw(const SDL_Rect *rect, Uint32 color) {
  tcmips_sdl2_render_line(rect->x, rect->y, rect->x + rect->w - 1, rect->y,
                          color);
  tcmips_sdl2_render_line(rect->x + rect->w - 1, rect->y, rect->x + rect->w - 1,
                          rect->y + rect->h - 1, color);
  tcmips_sdl2_render_line(rect->x + rect->w - 1, rect->y + rect->h - 1, rect->x,
                          rect->y + rect->h - 1, color);
  tcmips_sdl2_render_line(rect->x, rect->y + rect->h - 1, rect->x, rect->y,
                          color);
}

/* Blending and colour-modulation helpers for texture copies.
 * TCMIPS surfaces are RGBA32; blend uses standard source-over compositing. */

static Uint32 tcmips_sdl2_color_mod(Uint32 c, Uint8 mr, Uint8 mg, Uint8 mb,
                                    Uint8 ma) {
  Uint8 r, g, b, a;
  tcmips_sdl2_pixel_to_rgba(c, &r, &g, &b, &a);
  r = (Uint8)(((Uint16)r * mr) / 255u);
  g = (Uint8)(((Uint16)g * mg) / 255u);
  b = (Uint8)(((Uint16)b * mb) / 255u);
  a = (Uint8)(((Uint16)a * ma) / 255u);
  return tcmips_sdl2_rgba_to_pixel(r, g, b, a);
}

/* Source-over alpha blend: out = src over dst per channel. */
static Uint32 tcmips_sdl2_blend_over(Uint32 dst, Uint32 src) {
  Uint8 dr, dg, db, da, sr, sg, sb, sa;
  tcmips_sdl2_pixel_to_rgba(dst, &dr, &dg, &db, &da);
  tcmips_sdl2_pixel_to_rgba(src, &sr, &sg, &sb, &sa);
  if (sa == 255)
    return src; /* fully opaque */
  if (sa == 0)
    return dst; /* fully transparent */
  Uint32 inv = 255u - sa;
  Uint8 br = (Uint8)(((Uint16)sr * sa + (Uint16)dr * inv) / 255u);
  Uint8 bg = (Uint8)(((Uint16)sg * sa + (Uint16)dg * inv) / 255u);
  Uint8 bb = (Uint8)(((Uint16)sb * sa + (Uint16)db * inv) / 255u);
  Uint8 ba = (Uint8)(255u - ((Uint16)(255u - da) * inv) / 255u);
  return tcmips_sdl2_rgba_to_pixel(br, bg, bb, ba);
}

/* Additive blend: dst.rgb += src.rgb * src.a (saturating), dst.a unchanged.
 * Matches the SDL2 software-renderer definition of SDL_BLENDMODE_ADD. */
static Uint32 tcmips_sdl2_blend_add(Uint32 dst, Uint32 src) {
  Uint8 dr, dg, db, da, sr, sg, sb, sa;
  tcmips_sdl2_pixel_to_rgba(dst, &dr, &dg, &db, &da);
  tcmips_sdl2_pixel_to_rgba(src, &sr, &sg, &sb, &sa);
  Uint32 rr = (Uint32)dr + (Uint32)sr * sa / 255u;
  Uint32 rg = (Uint32)dg + (Uint32)sg * sa / 255u;
  Uint32 rb = (Uint32)db + (Uint32)sb * sa / 255u;
  if (rr > 255u)
    rr = 255u;
  if (rg > 255u)
    rg = 255u;
  if (rb > 255u)
    rb = 255u;
  return tcmips_sdl2_rgba_to_pixel((Uint8)rr, (Uint8)rg, (Uint8)rb, da);
}

/* Modulate blend: dst.rgb *= src.rgb / 255, dst.a unchanged.
 * Matches the SDL2 software-renderer definition of SDL_BLENDMODE_MOD. */
static Uint32 tcmips_sdl2_blend_mod(Uint32 dst, Uint32 src) {
  Uint8 dr, dg, db, da, sr, sg, sb, sa;
  tcmips_sdl2_pixel_to_rgba(dst, &dr, &dg, &db, &da);
  tcmips_sdl2_pixel_to_rgba(src, &sr, &sg, &sb, &sa);
  (void)sa;
  Uint8 br = (Uint8)(((Uint16)sr * dr) / 255u);
  Uint8 bg = (Uint8)(((Uint16)sg * dg) / 255u);
  Uint8 bb = (Uint8)(((Uint16)sb * db) / 255u);
  return tcmips_sdl2_rgba_to_pixel(br, bg, bb, da);
}

/* Apply the renderer's draw blend to a target pixel. */
static void tcmips_sdl2_render_blend_set_pixel(int x, int y, Uint32 color,
                                               SDL_BlendMode blend) {
  SDL_Surface *t = tcmips_sdl2_render_target();
  if (!t || !t->pixels)
    return;
  if (x < t->clip_rect.x || y < t->clip_rect.y ||
      x >= t->clip_rect.x + t->clip_rect.w ||
      y >= t->clip_rect.y + t->clip_rect.h) {
    return;
  }
  Uint32 *pixels = (Uint32 *)t->pixels;
  Uint32 idx = (Uint32)(y * (t->pitch >> 2) + x);
  switch (blend) {
  case SDL_BLENDMODE_BLEND:
    pixels[idx] = tcmips_sdl2_blend_over(pixels[idx], color);
    break;
  case SDL_BLENDMODE_ADD:
    pixels[idx] = tcmips_sdl2_blend_add(pixels[idx], color);
    break;
  case SDL_BLENDMODE_MOD:
    pixels[idx] = tcmips_sdl2_blend_mod(pixels[idx], color);
    break;
  default:
    pixels[idx] = color;
    break;
  }
}

/* Rotate / flip-aware copy.  For each destination pixel (inside the
 * rotation-expanded bounding box) the source coordinate is computed by
 * inverting the requested rotation around the requested pivot, applying the
 * horizontal/vertical flip, and the texel is drawn when it falls inside the
 * source rectangle (pixels outside the rotated source are left untouched,
 * matching upstream behaviour). */
static int tcmips_sdl2_render_copy_ex(
    SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_FRect *dstrect,
    const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip) {
  /* Unrotated, unflipped copies are exactly RenderCopy (and pick up its
     1:1/integer fast paths); only the rotation sampler needs float math. */
  if (angle == 0.0 && flip == SDL_FLIP_NONE)
    return tcmips_sdl2_render_copy_impl(texture, srcrect, dstrect);
  if (!texture || !texture->pixels || !g_tcmips_sdl.win.surface)
    return -1;

  SDL_Rect src = {0, 0, texture->w, texture->h};
  if (srcrect) {
    src = *srcrect;
    if (src.w <= 0 || src.h <= 0)
      return -1;
  }

  SDL_FRect dst;
  if (dstrect) {
    dst = *dstrect;
    if (dst.w <= 0.0f || dst.h <= 0.0f)
      return -1;
  } else {
    dst.x = (float)s_renderer.viewport.x;
    dst.y = (float)s_renderer.viewport.y;
    dst.w = (float)s_renderer.viewport.w;
    dst.h = (float)s_renderer.viewport.h;
  }

  SDL_Surface *t = tcmips_sdl2_render_target();
  const Uint32 *sp = (const Uint32 *)texture->pixels;
  int spitch = texture->pitch >> 2;

  int dw = (int)roundf(dst.w);
  int dh = (int)roundf(dst.h);
  if (dw <= 0 || dh <= 0)
    return -1;

  /* The pivot is specified in destination coordinates relative to dst.x/.y
   * and defaults to the dst centre.  The corresponding source pivot uses the
   * same relative offset inside the source rect. */
  float pivot_dx = dst.x + (center ? center->x : dst.w * 0.5f);
  float pivot_dy = dst.y + (center ? center->y : dst.h * 0.5f);
  float pivot_sx =
      (float)src.x + (center ? center->x / (float)dw : 0.5f) * (float)src.w;
  float pivot_sy =
      (float)src.y + (center ? center->y / (float)dh : 0.5f) * (float)src.h;

  double arad = angle * 3.14159265358979323846 / 180.0;
  double ca = cos(arad), sa = sin(arad);

  /* Iterate the rotated bounding box of the dst rect: corners rotated
   * around the pivot may extend outside the unrotated box.  The software
   * renderer rotates anti-clockwise with the same inverse mapping used for
   * sampling below (sxp = rx*ca + ry*sa, syp = -rx*sa + ry*ca), so the
   * corners are transformed with the same matrix to get exact bounds. */
  float min_x = pivot_dx, max_x = pivot_dx, min_y = pivot_dy, max_y = pivot_dy;
  const float corners[4][2] = {{dst.x, dst.y},
                               {dst.x + dst.w, dst.y},
                               {dst.x + dst.w, dst.y + dst.h},
                               {dst.x, dst.y + dst.h}};
  for (int i = 0; i < 4; ++i) {
    float rx = corners[i][0] - pivot_dx;
    float ry = corners[i][1] - pivot_dy;
    float bx = (float)pivot_dx + rx * (float)ca - ry * (float)sa;
    float by = (float)pivot_dy + rx * (float)sa + ry * (float)ca;
    if (bx < min_x)
      min_x = bx;
    if (bx > max_x)
      max_x = bx;
    if (by < min_y)
      min_y = by;
    if (by > max_y)
      max_y = by;
  }

  SDL_Rect bbox = {(int)floorf(min_x) - 1, (int)floorf(min_y) - 1, 0, 0};
  bbox.w = (int)ceilf(max_x) - bbox.x + 1;
  bbox.h = (int)ceilf(max_y) - bbox.y + 1;
  SDL_Rect clipped;
  if (!SDL_IntersectRect(&bbox, &t->clip_rect, &clipped))
    return 0; /* nothing visible */

  int hone = (int)((flip & SDL_FLIP_HORIZONTAL) ? 1 : 0);
  int vone = (int)((flip & SDL_FLIP_VERTICAL) ? 1 : 0);

  SDL_BlendMode blend = texture->blend;
  for (int y = clipped.y; y < clipped.y + clipped.h; ++y) {
    for (int x = clipped.x; x < clipped.x + clipped.w; ++x) {
      /* Inverse rotation of the dst pixel around the pivot. */
      double rxp = (double)x - pivot_dx;
      double ryp = (double)y - pivot_dy;
      double sxf = rxp * ca + ryp * sa;
      double syf = -rxp * sa + ryp * ca;
      int sx = (int)floor(pivot_sx + sxf + 0.5);
      int sy = (int)floor(pivot_sy + syf + 0.5);
      /* Rotate: a dst pixel that maps outside the source rect is not drawn
       * (upstream leaves the destination untouched there). */
      if (sx < src.x || sx >= src.x + src.w || sy < src.y ||
          sy >= src.y + src.h)
        continue;
      /* Flip inside the source rect. */
      if (hone)
        sx = src.x + (src.x + src.w - 1 - (sx - src.x));
      if (vone)
        sy = src.y + (src.y + src.h - 1 - (sy - src.y));
      Uint32 c = sp[sy * spitch + sx];
      c = tcmips_sdl2_color_mod(c, texture->colormod_r, texture->colormod_g,
                                texture->colormod_b, texture->colormod_a);
      tcmips_sdl2_render_blend_set_pixel(x, y, c, blend);
    }
  }
  return 0;
}

static int tcmips_sdl2_render_copy_impl(SDL_Texture *texture,
                                        const SDL_Rect *srcrect,
                                        const SDL_FRect *dstrect) {
  if (!texture || !texture->pixels || !g_tcmips_sdl.win.surface)
    return -1;

  SDL_Rect src = {0, 0, texture->w, texture->h};
  if (srcrect) {
    src = *srcrect;
    /* Upstream rejects empty/invalid source rectangles with -1.  Without
     * this check the row clamp below would map to src.y - 1 and read the
     * pixel row before the texture buffer. */
    if (src.w <= 0 || src.h <= 0)
      return -1;
  }

  SDL_FRect dst;
  if (dstrect) {
    dst = *dstrect;
    if (dst.w <= 0.0f || dst.h <= 0.0f)
      return -1;
  } else {
    /* Upstream: dstrect == NULL copies the source over the whole viewport
     * (which defaults to the entire target, preserving the old behaviour). */
    dst.x = (float)s_renderer.viewport.x;
    dst.y = (float)s_renderer.viewport.y;
    dst.w = (float)s_renderer.viewport.w;
    dst.h = (float)s_renderer.viewport.h;
  }

  // Nearest-neighbor blit with colour modulation and alpha blending.
  SDL_Surface *t = tcmips_sdl2_render_target();
  if (!t || !t->pixels)
    return -1;
  const Uint8 *sp = (const Uint8 *)texture->pixels;

  int dx0 = (int)roundf(dst.x);
  int dy0 = (int)roundf(dst.y);
  int dw = (int)roundf(dst.w);
  int dh = (int)roundf(dst.h);
  if (dw <= 0 || dh <= 0)
    return -1;

  SDL_Rect dst_r = {dx0, dy0, dw, dh};
  SDL_Rect clipped;
  if (!SDL_IntersectRect(&dst_r, &t->clip_rect, &clipped))
    return 0; /* nothing visible: not an error */

  SDL_BlendMode blend = texture->blend;
  /* Skip per-pixel colour modulation when it is the identity (255,255,255,255).
   */
  int use_mod = !(texture->colormod_r == 255 && texture->colormod_g == 255 &&
                  texture->colormod_b == 255 && texture->colormod_a == 255);

  /* ---- 1:1 integer fast path ------------------------------------------ */
  /* Unscaled, unblended, unmodulated copies degenerate to row memcpys of
     the intersected region (exactly what SDL_UpperBlit's same-format path
     does in the SDL1 layer).  This keeps native-size RenderCopy work off
     the per-pixel soft-float sampler entirely. */
  if (blend == SDL_BLENDMODE_NONE && !use_mod && dw == src.w && dh == src.h) {
    for (int y = 0; y < clipped.h; ++y) {
      int sy = src.y + (clipped.y + y - dy0);
      int sx = src.x + (clipped.x - dx0);
      SDL_memcpy((Uint8 *)t->pixels +
                     (size_t)(clipped.y + y) * (size_t)t->pitch +
                     (size_t)clipped.x * 4u,
                 (const Uint8 *)texture->pixels +
                     (size_t)sy * (size_t)texture->pitch + (size_t)sx * 4u,
                 (size_t)clipped.w * 4u);
    }
    return 0;
  }

  /* ---- Exact integer nearest-neighbour sampler ------------------------- */
  /* The generic path used to compute a soft-float division and multiply
     per pixel (fx = (x-dx0)/(float)dw; sx = src.x + (int)(fx*src.w)) —
     ((int)fy_trunc semantics retained) — which is prohibitively slow on the
     soft-float, no-FPU TCMIPS core.  Replace it with the exact integer
     equivalent, driven incrementally (no per-pixel division):
         sx(x) = src.x + floor((x - dx0) * src.w / dw)
     with the numerator of the NEXT pixel moving by +src.w per step
     (Bresenham quotient/remainder) and the same dy treatment per row.  The
     sampled nearest-neighbour index can only differ from the float version
     where float rounding was previously non-deterministic at exact pixel
     boundaries; the integer version is the mathematically exact one. */

  /* Row mapping: sy(y) = src.y + (y - dy0) * src.h / dh (incremental per row).
   */
  int ynum = (clipped.y - dy0) * src.h;
  int yq = ynum / dh;
  int yrem = ynum - yq * dh; /* 0 <= yrem < dh */

  /* Column mapping seed at the first visible pixel. */
  int xq = (clipped.x - dx0) * src.w / dw;
  int xrem = (clipped.x - dx0) * src.w - xq * dw; /* 0 <= xrem < dw */

  for (int y = clipped.y; y < clipped.y + clipped.h; ++y) {
    int sy = src.y + yq;
    if (sy < src.y)
      sy = src.y;
    if (sy >= src.y + src.h)
      sy = src.y + src.h - 1;

    const Uint32 *srow =
        (const Uint32 *)(sp + (size_t)sy * (size_t)texture->pitch);
    // Uint32 *drow =
    //     (Uint32 *)((Uint8 *)t->pixels + (size_t)y * (size_t)t->pitch);

    int xq2 = xq;     /* column quotient for this row */
    int xrem2 = xrem; /* column remainder for this row  */
    for (int x = clipped.x; x < clipped.x + clipped.w; ++x) {
      int sx = src.x + xq2;
      if (sx < src.x)
        sx = src.x;
      if (sx >= src.x + src.w)
        sx = src.x + src.w - 1;
      Uint32 c = srow[sx];
      if (use_mod)
        c = tcmips_sdl2_color_mod(c, texture->colormod_r, texture->colormod_g,
                                  texture->colormod_b, texture->colormod_a);
      tcmips_sdl2_render_blend_set_pixel(x, y, c, blend);
      xrem2 += src.w;
      while (xrem2 >= dw) {
        xrem2 -= dw;
        ++xq2;
      }
    }

    yrem += src.h;
    while (yrem >= dh) {
      yrem -= dh;
      ++yq;
    }
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Public renderer API
// ---------------------------------------------------------------------------

int SDL_GetNumRenderDrivers(void) { return 1; }

int SDL_GetRenderDriverInfo(int index, SDL_RendererInfo *info) {
  (void)index;
  if (info) {
    info->name = "tcmips";
    info->flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    info->num_texture_formats = 1;
    info->texture_formats[0] = TCMIPS_SDL_FORMAT;
    info->max_texture_width = 1024;
    info->max_texture_height = 1024;
  }
  return 0;
}

int SDL_CreateWindowAndRenderer(int width, int height, Uint32 window_flags,
                                SDL_Window **window, SDL_Renderer **renderer) {
  *window =
      SDL_CreateWindow(NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       width, height, window_flags);
  if (!*window)
    return -1;
  *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
  if (!*renderer)
    return -1;
  return 0;
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, int index, Uint32 flags) {
  (void)index;
  (void)flags;
  if (!window)
    return NULL;

  // TCMIPS supports only a single renderer.
  if (g_tcmips_sdl.render.renderer) {
    SDL_SetError(
        "TCMIPS supports only one renderer; must SDL_DestroyRenderer first");
    return NULL;
  }

  SDL_memset(&s_renderer, 0, sizeof(s_renderer));
  s_renderer.window = window;
  s_renderer.scale_x = 1.0f;
  s_renderer.scale_y = 1.0f;
  s_renderer.viewport.x = 0;
  s_renderer.viewport.y = 0;
  if (g_tcmips_sdl.win.surface) {
    s_renderer.viewport.w = g_tcmips_sdl.win.surface->w;
    s_renderer.viewport.h = g_tcmips_sdl.win.surface->h;
  } else {
    s_renderer.viewport.w = window->w;
    s_renderer.viewport.h = window->h;
  }
  s_renderer.clip_enabled = SDL_FALSE;
  s_renderer.blend = SDL_BLENDMODE_NONE;
  s_renderer.r = s_renderer.g = s_renderer.b = 255;
  s_renderer.a = 255;
  s_renderer.vsync = SDL_FALSE;

  g_tcmips_sdl.render.renderer = &s_renderer;
  return &s_renderer;
}

SDL_Renderer *SDL_CreateSoftwareRenderer(SDL_Surface *surface) {
  (void)surface;
  TCMIPS_SDL_STUB_MSG("SDL_CreateSoftwareRenderer");
  return NULL;
}

SDL_Renderer *SDL_GetRenderer(SDL_Window *window) {
  if (window && g_tcmips_sdl.win.window == window) {
    return g_tcmips_sdl.render.renderer;
  }
  return NULL;
}

int SDL_GetRendererInfo(SDL_Renderer *renderer, SDL_RendererInfo *info) {
  (void)renderer;
  return SDL_GetRenderDriverInfo(0, info);
}

int SDL_GetRendererOutputSize(SDL_Renderer *renderer, int *w, int *h) {
  if (renderer && g_tcmips_sdl.win.surface) {
    if (w)
      *w = g_tcmips_sdl.win.surface->w;
    if (h)
      *h = g_tcmips_sdl.win.surface->h;
    return 0;
  }
  if (w)
    *w = 0;
  if (h)
    *h = 0;
  return -1;
}

void SDL_DestroyRenderer(SDL_Renderer *renderer) {
  if (renderer == g_tcmips_sdl.render.renderer) {
    g_tcmips_sdl.render.renderer = NULL;
  }
  /* Release any textures that the caller forgot to SDL_DestroyTexture and
     reset the static texture pool + renderer state so the next renderer is
     clean. */
  for (int i = 0; i < TCMIPS_SDL_MAX_TEXTURES; ++i) {
    if (s_textures[i].pixels) {
      SDL_free(s_textures[i].pixels);
    }
  }
  SDL_memset(s_textures, 0, sizeof(s_textures));
  SDL_memset(&s_renderer, 0, sizeof(s_renderer));
}

int SDL_RenderClear(SDL_Renderer *renderer) {
  (void)renderer;
  SDL_Surface *t = tcmips_sdl2_render_target();
  if (!t || !t->pixels)
    return -1;
  Uint32 color = tcmips_sdl2_render_color();
  SDL_Rect full = {0, 0, t->w, t->h};
  tcmips_sdl2_render_rect_fill(&full, color);
  return 0;
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
  (void)renderer;
  tcmips_sdl2_present_framebuffer();
}

int SDL_RenderSetLogicalSize(SDL_Renderer *renderer, int w, int h) {
  (void)renderer;
  (void)w;
  (void)h;
  TCMIPS_SDL_STUB_MSG("SDL_RenderSetLogicalSize");
  return 0;
}

void SDL_RenderGetLogicalSize(SDL_Renderer *renderer, int *w, int *h) {
  if (renderer && g_tcmips_sdl.win.surface) {
    if (w)
      *w = g_tcmips_sdl.win.surface->w;
    if (h)
      *h = g_tcmips_sdl.win.surface->h;
  } else {
    if (w)
      *w = 0;
    if (h)
      *h = 0;
  }
}

int SDL_RenderSetIntegerScale(SDL_Renderer *renderer, SDL_bool enable) {
  if (renderer)
    renderer->integer_scale = enable;
  return 0;
}

SDL_bool SDL_RenderGetIntegerScale(SDL_Renderer *renderer) {
  if (renderer)
    return renderer->integer_scale;
  return SDL_FALSE;
}

int SDL_RenderSetViewport(SDL_Renderer *renderer, const SDL_Rect *rect) {
  if (!renderer)
    return -1;
  if (rect) {
    renderer->viewport = *rect;
  } else {
    renderer->viewport.x = 0;
    renderer->viewport.y = 0;
    if (g_tcmips_sdl.win.surface) {
      renderer->viewport.w = g_tcmips_sdl.win.surface->w;
      renderer->viewport.h = g_tcmips_sdl.win.surface->h;
    }
  }
  return 0;
}

void SDL_RenderGetViewport(SDL_Renderer *renderer, SDL_Rect *rect) {
  if (renderer && rect) {
    *rect = renderer->viewport;
  } else if (rect) {
    rect->x = rect->y = 0;
    rect->w = rect->h = 0;
  }
}

int SDL_RenderSetClipRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
  if (!renderer)
    return -1;
  if (rect) {
    renderer->clip = *rect;
    renderer->clip_enabled = SDL_TRUE;
  } else {
    renderer->clip_enabled = SDL_FALSE;
  }
  return 0;
}

void SDL_RenderGetClipRect(SDL_Renderer *renderer, SDL_Rect *rect) {
  if (renderer && rect) {
    *rect = renderer->clip;
  } else if (rect) {
    rect->x = rect->y = rect->w = rect->h = 0;
  }
}

SDL_bool SDL_RenderIsClipEnabled(SDL_Renderer *renderer) {
  if (renderer)
    return renderer->clip_enabled;
  return SDL_FALSE;
}

int SDL_RenderSetScale(SDL_Renderer *renderer, float scaleX, float scaleY) {
  if (!renderer)
    return -1;
  renderer->scale_x = scaleX;
  renderer->scale_y = scaleY;
  return 0;
}

void SDL_RenderGetScale(SDL_Renderer *renderer, float *scaleX, float *scaleY) {
  if (renderer) {
    if (scaleX)
      *scaleX = renderer->scale_x;
    if (scaleY)
      *scaleY = renderer->scale_y;
  } else {
    if (scaleX)
      *scaleX = 1.0f;
    if (scaleY)
      *scaleY = 1.0f;
  }
}

void SDL_RenderWindowToLogical(SDL_Renderer *renderer, int windowX, int windowY,
                               float *logicalX, float *logicalY) {
  float sx = 1.0f, sy = 1.0f;
  SDL_RenderGetScale(renderer, &sx, &sy);
  if (logicalX)
    *logicalX = (float)windowX / sx;
  if (logicalY)
    *logicalY = (float)windowY / sy;
}

void SDL_RenderLogicalToWindow(SDL_Renderer *renderer, float logicalX,
                               float logicalY, int *windowX, int *windowY) {
  float sx = 1.0f, sy = 1.0f;
  SDL_RenderGetScale(renderer, &sx, &sy);
  if (windowX)
    *windowX = (int)(logicalX * sx);
  if (windowY)
    *windowY = (int)(logicalY * sy);
}

int SDL_RenderSetVSync(SDL_Renderer *renderer, int vsync) {
  if (renderer)
    renderer->vsync = vsync ? SDL_TRUE : SDL_FALSE;
  return 0;
}

int SDL_RenderGetVSync(SDL_Renderer *renderer, int *vsync) {
  if (renderer && vsync)
    *vsync = renderer->vsync ? 1 : 0;
  return 0;
}

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b,
                           Uint8 a) {
  if (!renderer)
    return -1;
  renderer->r = r;
  renderer->g = g;
  renderer->b = b;
  renderer->a = a;
  return 0;
}

int SDL_GetRenderDrawColor(SDL_Renderer *renderer, Uint8 *r, Uint8 *g, Uint8 *b,
                           Uint8 *a) {
  if (!renderer)
    return -1;
  if (r)
    *r = renderer->r;
  if (g)
    *g = renderer->g;
  if (b)
    *b = renderer->b;
  if (a)
    *a = renderer->a;
  return 0;
}

int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer,
                               SDL_BlendMode blendMode) {
  if (!renderer)
    return -1;
  renderer->blend = blendMode;
  return 0;
}

int SDL_GetRenderDrawBlendMode(SDL_Renderer *renderer,
                               SDL_BlendMode *blendMode) {
  if (renderer && blendMode) {
    *blendMode = renderer->blend;
    return 0;
  }
  return -1;
}

int SDL_RenderFillRectF(SDL_Renderer *renderer, const SDL_FRect *rect) {
  if (!renderer)
    return -1;
  SDL_Surface *t = tcmips_sdl2_render_target();
  if (!t || !t->pixels)
    return -1; /* NULL-target guard: renderer outlives its window */

  SDL_Rect r;
  if (rect) {
    /* Draw coordinates are viewport-relative and scaled (upstream). */
    r.x = tcmips_sdl2_map_draw_x(renderer, rect->x);
    r.y = tcmips_sdl2_map_draw_y(renderer, rect->y);
    r.w = (int)roundf(rect->w * renderer->scale_x);
    r.h = (int)roundf(rect->h * renderer->scale_y);
  } else {
    /* Upstream: NULL rect fills the whole viewport. */
    r = renderer->viewport;
  }
  if (renderer->clip_enabled) {
    SDL_Rect tmp;
    SDL_IntersectRect(&r, &renderer->clip, &tmp);
    r = tmp;
  }
  tcmips_sdl2_render_rect_fill(&r, tcmips_sdl2_render_color());
  return 0;
}

int SDL_RenderFillRectsF(SDL_Renderer *renderer, const SDL_FRect *rects,
                         int count) {
  if (!rects || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_RenderFillRectF(renderer, &rects[i]);
  }
  return 0;
}

int SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
  SDL_FRect fr = {0.0f, 0.0f, 0.0f, 0.0f};
  if (rect) {
    fr.x = (float)rect->x;
    fr.y = (float)rect->y;
    fr.w = (float)rect->w;
    fr.h = (float)rect->h;
  }
  return SDL_RenderFillRectF(renderer, rect ? &fr : NULL);
}

int SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_Rect *rects,
                        int count) {
  if (!rects || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_RenderFillRect(renderer, &rects[i]);
  }
  return 0;
}

int SDL_RenderDrawRectF(SDL_Renderer *renderer, const SDL_FRect *rect) {
  if (!renderer)
    return -1;
  SDL_Rect r;
  if (rect) {
    r.x = tcmips_sdl2_map_draw_x(renderer, rect->x);
    r.y = tcmips_sdl2_map_draw_y(renderer, rect->y);
    r.w = (int)roundf(rect->w * renderer->scale_x);
    r.h = (int)roundf(rect->h * renderer->scale_y);
  } else {
    r = renderer->viewport;
  }
  tcmips_sdl2_render_rect_draw(&r, tcmips_sdl2_render_color());
  return 0;
}

int SDL_RenderDrawRectsF(SDL_Renderer *renderer, const SDL_FRect *rects,
                         int count) {
  if (!rects || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_RenderDrawRectF(renderer, &rects[i]);
  }
  return 0;
}

int SDL_RenderDrawRect(SDL_Renderer *renderer, const SDL_Rect *rect) {
  SDL_FRect fr = {0.0f, 0.0f, 1.0f, 1.0f};
  if (rect) {
    fr.x = (float)rect->x;
    fr.y = (float)rect->y;
    fr.w = (float)rect->w;
    fr.h = (float)rect->h;
  }
  return SDL_RenderDrawRectF(renderer, rect ? &fr : NULL);
}

int SDL_RenderDrawRects(SDL_Renderer *renderer, const SDL_Rect *rects,
                        int count) {
  if (!rects || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_RenderDrawRect(renderer, &rects[i]);
  }
  return 0;
}

int SDL_RenderDrawLineF(SDL_Renderer *renderer, float x1, float y1, float x2,
                        float y2) {
  if (!renderer || !tcmips_sdl2_render_target())
    return -1;
  int ix1 = tcmips_sdl2_map_draw_x(renderer, x1);
  int iy1 = tcmips_sdl2_map_draw_y(renderer, y1);
  int ix2 = tcmips_sdl2_map_draw_x(renderer, x2);
  int iy2 = tcmips_sdl2_map_draw_y(renderer, y2);
  tcmips_sdl2_render_line(ix1, iy1, ix2, iy2, tcmips_sdl2_render_color());
  return 0;
}

int SDL_RenderDrawLinesF(SDL_Renderer *renderer, const SDL_FPoint *points,
                         int count) {
  if (!points || count < 2)
    return 0;
  for (int i = 0; i < count - 1; ++i) {
    SDL_RenderDrawLineF(renderer, points[i].x, points[i].y, points[i + 1].x,
                        points[i + 1].y);
  }
  return 0;
}

int SDL_RenderDrawLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2) {
  return SDL_RenderDrawLineF(renderer, (float)x1, (float)y1, (float)x2,
                             (float)y2);
}

int SDL_RenderDrawLines(SDL_Renderer *renderer, const SDL_Point *points,
                        int count) {
  if (!points || count < 2)
    return 0;
  for (int i = 0; i < count - 1; ++i) {
    SDL_RenderDrawLine(renderer, points[i].x, points[i].y, points[i + 1].x,
                       points[i + 1].y);
  }
  return 0;
}

int SDL_RenderDrawPointF(SDL_Renderer *renderer, float x, float y) {
  if (!renderer || !tcmips_sdl2_render_target())
    return -1;
  tcmips_sdl2_render_set_pixel(tcmips_sdl2_map_draw_x(renderer, x),
                               tcmips_sdl2_map_draw_y(renderer, y),
                               tcmips_sdl2_render_color());
  return 0;
}

int SDL_RenderDrawPointsF(SDL_Renderer *renderer, const SDL_FPoint *points,
                          int count) {
  if (!points || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_RenderDrawPointF(renderer, points[i].x, points[i].y);
  }
  return 0;
}

int SDL_RenderDrawPoint(SDL_Renderer *renderer, int x, int y) {
  return SDL_RenderDrawPointF(renderer, (float)x, (float)y);
}

int SDL_RenderDrawPoints(SDL_Renderer *renderer, const SDL_Point *points,
                         int count) {
  if (!points || count <= 0)
    return 0;
  for (int i = 0; i < count; ++i) {
    SDL_RenderDrawPoint(renderer, points[i].x, points[i].y);
  }
  return 0;
}

int SDL_RenderCopyF(SDL_Renderer *renderer, SDL_Texture *texture,
                    const SDL_Rect *srcrect, const SDL_FRect *dstrect) {
  (void)renderer;
  return tcmips_sdl2_render_copy_impl(texture, srcrect, dstrect);
}

int SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
                   const SDL_Rect *srcrect, const SDL_Rect *dstrect) {
  SDL_FRect fr = {0.0f, 0.0f, 0.0f, 0.0f};
  if (dstrect) {
    fr.x = (float)dstrect->x;
    fr.y = (float)dstrect->y;
    fr.w = (float)dstrect->w;
    fr.h = (float)dstrect->h;
  }
  return SDL_RenderCopyF(renderer, texture, srcrect, dstrect ? &fr : NULL);
}

int SDL_RenderCopyExF(SDL_Renderer *renderer, SDL_Texture *texture,
                      const SDL_Rect *srcrect, const SDL_FRect *dstrect,
                      const double angle, const SDL_FPoint *center,
                      const SDL_RendererFlip flip) {
  (void)renderer;
  return tcmips_sdl2_render_copy_ex(texture, srcrect, dstrect, angle, center,
                                    flip);
}

int SDL_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
                     const SDL_Rect *srcrect, const SDL_Rect *dstrect,
                     const double angle, const SDL_Point *center,
                     const SDL_RendererFlip flip) {
  SDL_FRect fr = {0.0f, 0.0f, 0.0f, 0.0f};
  SDL_FPoint fc = {0.0f, 0.0f};
  if (dstrect) {
    fr.x = (float)dstrect->x;
    fr.y = (float)dstrect->y;
    fr.w = (float)dstrect->w;
    fr.h = (float)dstrect->h;
  }
  if (center) {
    fc.x = (float)center->x;
    fc.y = (float)center->y;
  }
  return SDL_RenderCopyExF(renderer, texture, srcrect, dstrect ? &fr : NULL,
                           angle, center ? &fc : NULL, flip);
}

int SDL_RenderGeometry(SDL_Renderer *renderer, SDL_Texture *texture,
                       const SDL_Vertex *vertices, int num_vertices,
                       const int *indices, int num_indices) {
  (void)renderer;
  (void)texture;
  (void)vertices;
  (void)num_vertices;
  (void)indices;
  (void)num_indices;
  TCMIPS_SDL_STUB_MSG("SDL_RenderGeometry");
  return 0;
}

int SDL_RenderGeometryRaw(SDL_Renderer *renderer, SDL_Texture *texture,
                          const float *xy, int xy_stride,
                          const SDL_Color *color, int color_stride,
                          const float *uv, int uv_stride, int num_vertices,
                          const void *indices, int num_indices,
                          int size_indices) {
  (void)renderer;
  (void)texture;
  (void)xy;
  (void)xy_stride;
  (void)color;
  (void)color_stride;
  (void)uv;
  (void)uv_stride;
  (void)num_vertices;
  (void)indices;
  (void)num_indices;
  (void)size_indices;
  TCMIPS_SDL_STUB_MSG("SDL_RenderGeometryRaw");
  return 0;
}

int SDL_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
                         Uint32 format, void *pixels, int pitch) {
  (void)renderer;
  (void)rect;
  (void)format;
  (void)pixels;
  (void)pitch;
  TCMIPS_SDL_STUB_MSG("SDL_RenderReadPixels");
  return -1;
}

SDL_bool SDL_RenderTargetSupported(SDL_Renderer *renderer) {
  (void)renderer;
  return SDL_FALSE;
}

int SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
  (void)renderer;
  (void)texture;
  /* Render targets are unsupported (SDL_RenderTargetSupported ==
     SDL_FALSE): report the failure instead of silently returning success
     while rendering keeps going to the window. */
  TCMIPS_SDL_STUB_MSG("SDL_SetRenderTarget");
  return -1;
}

SDL_Texture *SDL_GetRenderTarget(SDL_Renderer *renderer) {
  (void)renderer;
  return NULL;
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format,
                               int access, int w, int h) {
  (void)renderer;
  if (w <= 0 || h <= 0)
    return NULL;

  SDL_Texture *t = NULL;
  for (int i = 0; i < TCMIPS_SDL_MAX_TEXTURES; ++i) {
    if (s_textures[i].pitch == 0) {
      t = &s_textures[i];
      break;
    }
  }
  if (!t)
    return NULL;

  t->format = format;
  t->w = w;
  t->h = h;
  t->access = access;
  t->pitch = w * 4;
  t->colormod_r = t->colormod_g = t->colormod_b = t->colormod_a = 255;
  t->blend = SDL_BLENDMODE_NONE;
  t->scale = SDL_ScaleModeNearest;
  t->userdata = NULL;
  t->refcount = 1;
  t->pixels = SDL_malloc((size_t)t->pitch * (size_t)h);
  if (!t->pixels) {
    SDL_memset(t, 0, sizeof(*t));
    return NULL;
  }
  SDL_memset(t->pixels, 0, (size_t)t->pitch * (size_t)h);
  return t;
}

SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer,
                                          SDL_Surface *surface) {
  (void)renderer;
  if (!surface || !surface->pixels || !surface->format)
    return NULL;
  if (surface->format->BytesPerPixel != 4) {
    /* TCMIPS textures are RGBA32 only; refusing here keeps every later
       texture frame buffer access inside its w*4*h allocation. */
    SDL_SetError("SDL_CreateTextureFromSurface: only 32-bpp surfaces");
    return NULL;
  }
  SDL_Texture *t =
      SDL_CreateTexture(renderer, TCMIPS_SDL_FORMAT, SDL_TEXTUREACCESS_STATIC,
                        surface->w, surface->h);
  if (!t)
    return NULL;

  /* Copy row-by-row honouring the source stride: a surface pitch may carry
     padding, so a whole-block memcpy cannot be used for the transfer.  The
     texture keeps its own tightly packed w*4 pitch (the previous version
     took over the surface pitch, which alias-widened row reads). */
  const Uint8 *src8 = (const Uint8 *)surface->pixels;
  Uint8 *dst8 = (Uint8 *)t->pixels;
  size_t row = (size_t)surface->w * 4u;

  /* A surface colour-key means the keyed pixels should become transparent
   * (alpha 0) in the texture so renderer copies blend them through. */
  SDL_bool ck = SDL_HasColorKey(surface);
  Uint32 key = 0;
  if (ck)
    SDL_GetColorKey(surface, &key);
  for (int y = 0; y < surface->h; ++y) {
    const Uint32 *src =
        (const Uint32 *)(src8 + (size_t)y * (size_t)surface->pitch);
    Uint32 *dst = (Uint32 *)(dst8 + (size_t)y * (size_t)t->pitch);
    if (ck) {
      for (int x = 0; x < surface->w; ++x) {
        if ((src[x] & 0x00FFFFFFu) == (key & 0x00FFFFFFu))
          dst[x] = (src[x] & 0x00FFFFFFu); /* force alpha = 0 */
        else
          dst[x] = src[x];
      }
    } else {
      SDL_memcpy(dst, src, row);
    }
  }
  return t;
}

int SDL_QueryTexture(SDL_Texture *texture, Uint32 *format, int *access, int *w,
                     int *h) {
  if (!texture)
    return -1;
  if (format)
    *format = texture->format;
  if (access)
    *access = texture->access;
  if (w)
    *w = texture->w;
  if (h)
    *h = texture->h;
  return 0;
}

int SDL_SetTextureColorMod(SDL_Texture *texture, Uint8 r, Uint8 g, Uint8 b) {
  if (!texture)
    return -1;
  texture->colormod_r = r;
  texture->colormod_g = g;
  texture->colormod_b = b;
  return 0;
}

int SDL_GetTextureColorMod(SDL_Texture *texture, Uint8 *r, Uint8 *g, Uint8 *b) {
  if (!texture)
    return -1;
  if (r)
    *r = texture->colormod_r;
  if (g)
    *g = texture->colormod_g;
  if (b)
    *b = texture->colormod_b;
  return 0;
}

int SDL_SetTextureAlphaMod(SDL_Texture *texture, Uint8 alpha) {
  if (!texture)
    return -1;
  texture->colormod_a = alpha;
  return 0;
}

int SDL_GetTextureAlphaMod(SDL_Texture *texture, Uint8 *alpha) {
  if (!texture)
    return -1;
  if (alpha)
    *alpha = texture->colormod_a;
  return 0;
}

int SDL_SetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode blendMode) {
  if (!texture)
    return -1;
  texture->blend = blendMode;
  return 0;
}

int SDL_GetTextureBlendMode(SDL_Texture *texture, SDL_BlendMode *blendMode) {
  if (!texture)
    return -1;
  if (blendMode)
    *blendMode = texture->blend;
  return 0;
}

int SDL_SetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode scaleMode) {
  if (!texture)
    return -1;
  texture->scale = scaleMode;
  return 0;
}

int SDL_GetTextureScaleMode(SDL_Texture *texture, SDL_ScaleMode *scaleMode) {
  if (!texture)
    return -1;
  if (scaleMode)
    *scaleMode = texture->scale;
  return 0;
}

int SDL_SetTextureUserData(SDL_Texture *texture, void *userdata) {
  if (!texture)
    return -1;
  texture->userdata = userdata;
  return 0;
}

void *SDL_GetTextureUserData(SDL_Texture *texture) {
  if (!texture)
    return NULL;
  return texture->userdata;
}

int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect,
                      const void *pixels, int pitch) {
  if (!texture || !pixels)
    return -1;

  SDL_Rect r = {0, 0, texture->w, texture->h};
  if (rect)
    r = *rect;

  /* Clamp the update rectangle into the texture bounds.  SDL allows negative
     origins and overruns; anything outside [0,w]x[0,h] must be dropped, not
     written before/after the pixel buffer. */
  if (r.x < 0) {
    r.w += r.x;
    r.x = 0;
  }
  if (r.y < 0) {
    r.h += r.y;
    r.y = 0;
  }
  if (r.x + r.w > texture->w)
    r.w = texture->w - r.x;
  if (r.y + r.h > texture->h)
    r.h = texture->h - r.y;
  if (r.w <= 0 || r.h <= 0)
    return 0;

  /* Reject a caller-supplied pitch that would read out of bounds: the source
     row stride must cover the bytes we actually copy per row. */
  int row_len = r.w * 4;
  if (pitch < row_len)
    return -1;

  Uint8 *dst = (Uint8 *)texture->pixels + r.y * texture->pitch + r.x * 4;
  const Uint8 *src = (const Uint8 *)pixels;
  for (int y = 0; y < r.h; ++y) {
    SDL_memcpy(dst + y * texture->pitch, src + y * pitch, row_len);
  }
  return 0;
}

int SDL_UpdateYUVTexture(SDL_Texture *texture, const SDL_Rect *rect,
                         const Uint8 *Yplane, int Ypitch, const Uint8 *Uplane,
                         int Upitch, const Uint8 *Vplane, int Vpitch) {
  (void)texture;
  (void)rect;
  (void)Yplane;
  (void)Ypitch;
  (void)Uplane;
  (void)Upitch;
  (void)Vplane;
  (void)Vpitch;
  TCMIPS_SDL_STUB_MSG("SDL_UpdateYUVTexture");
  return -1;
}

int SDL_UpdateNVTexture(SDL_Texture *texture, const SDL_Rect *rect,
                        const Uint8 *Yplane, int Ypitch, const Uint8 *UVplane,
                        int UVpitch) {
  (void)texture;
  (void)rect;
  (void)Yplane;
  (void)Ypitch;
  (void)UVplane;
  (void)UVpitch;
  TCMIPS_SDL_STUB_MSG("SDL_UpdateNVTexture");
  return -1;
}

int SDL_LockTexture(SDL_Texture *texture, const SDL_Rect *rect, void **pixels,
                    int *pitch) {
  (void)rect;
  if (!texture || !pixels || !pitch)
    return -1;
  *pixels = texture->pixels;
  *pitch = texture->pitch;
  return 0;
}

int SDL_LockTextureToSurface(SDL_Texture *texture, const SDL_Rect *rect,
                             SDL_Surface **surface) {
  (void)rect;
  if (!texture || !surface)
    return -1;
  /* Alias the texture pixel buffer (upstream semantics): edits made through
     the returned surface are visible to subsequent RenderCopy calls, and
     SDL_FreeSurface releases only the surface shell (PREALLOC|DONTFREE). */
  *surface = tcmips_sdl2_surface_alias_rgba(texture->pixels, texture->w,
                                            texture->h, texture->pitch);
  return *surface ? 0 : -1;
}

void SDL_UnlockTexture(SDL_Texture *texture) { (void)texture; }

void SDL_DestroyTexture(SDL_Texture *texture) {
  if (!texture)
    return;
  for (int i = 0; i < TCMIPS_SDL_MAX_TEXTURES; ++i) {
    if (&s_textures[i] == texture) {
      if (s_textures[i].pixels) {
        SDL_free(s_textures[i].pixels);
      }
      SDL_memset(&s_textures[i], 0, sizeof(SDL_Texture));
      break;
    }
  }
}

int SDL_GL_BindTexture(SDL_Texture *texture, float *texw, float *texh) {
  (void)texture;
  (void)texw;
  (void)texh;
  TCMIPS_SDL_STUB_MSG("SDL_GL_BindTexture");
  return 0;
}

int SDL_GL_UnbindTexture(SDL_Texture *texture) {
  (void)texture;
  TCMIPS_SDL_STUB_MSG("SDL_GL_UnbindTexture");
  return 0;
}

// Metal / GL are unsupported in TCMIPS.
void *SDL_RenderGetMetalLayer(SDL_Renderer *renderer) {
  (void)renderer;
  return NULL;
}
void *SDL_RenderGetMetalCommandEncoder(SDL_Renderer *renderer) {
  (void)renderer;
  return NULL;
}

// F variants are implemented above; the non-F variants forward to them.
