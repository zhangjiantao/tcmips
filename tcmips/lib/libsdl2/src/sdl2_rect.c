#include <SDL.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#include <math.h>
#pragma GCC diagnostic pop

SDL_bool SDL_HasIntersection(const SDL_Rect *A, const SDL_Rect *B) {
  if (!A || !B)
    return SDL_FALSE;
  return (A->x < B->x + B->w && A->x + A->w > B->x && A->y < B->y + B->h &&
          A->y + A->h > B->y)
             ? SDL_TRUE
             : SDL_FALSE;
}

SDL_bool SDL_IntersectRect(const SDL_Rect *A, const SDL_Rect *B,
                           SDL_Rect *result) {
  if (!A || !B || !result)
    return SDL_FALSE;
  int x1 = A->x > B->x ? A->x : B->x;
  int x2 = A->x + A->w < B->x + B->w ? A->x + A->w : B->x + B->w;
  int y1 = A->y > B->y ? A->y : B->y;
  int y2 = A->y + A->h < B->y + B->h ? A->y + A->h : B->y + B->h;
  if (x2 <= x1 || y2 <= y1) {
    result->x = result->y = result->w = result->h = 0;
    return SDL_FALSE;
  }
  result->x = x1;
  result->y = y1;
  result->w = x2 - x1;
  result->h = y2 - y1;
  return SDL_TRUE;
}

void SDL_UnionRect(const SDL_Rect *A, const SDL_Rect *B, SDL_Rect *result) {
  if (!A || !B || !result)
    return;
  int x1 = A->x < B->x ? A->x : B->x;
  int y1 = A->y < B->y ? A->y : B->y;
  int x2 = A->x + A->w > B->x + B->w ? A->x + A->w : B->x + B->w;
  int y2 = A->y + A->h > B->y + B->h ? A->y + A->h : B->y + B->h;
  result->x = x1;
  result->y = y1;
  result->w = x2 - x1;
  result->h = y2 - y1;
}

SDL_bool SDL_EnclosePoints(const SDL_Point *points, int count,
                           const SDL_Rect *clip, SDL_Rect *result) {
  if (!points || count <= 0 || !result)
    return SDL_FALSE;

  int minx = points[0].x, maxx = points[0].x;
  int miny = points[0].y, maxy = points[0].y;
  for (int i = 1; i < count; ++i) {
    if (points[i].x < minx)
      minx = points[i].x;
    if (points[i].x > maxx)
      maxx = points[i].x;
    if (points[i].y < miny)
      miny = points[i].y;
    if (points[i].y > maxy)
      maxy = points[i].y;
  }
  SDL_Rect r = {minx, miny, maxx - minx + 1, maxy - miny + 1};

  if (clip) {
    if (!SDL_IntersectRect(&r, clip, result)) {
      SDL_memset(result, 0, sizeof(*result));
      return SDL_FALSE;
    }
  } else {
    *result = r;
  }
  return SDL_TRUE;
}

SDL_bool SDL_IntersectRectAndLine(const SDL_Rect *rect, int *X1, int *Y1,
                                  int *X2, int *Y2) {
  if (!rect || !X1 || !Y1 || !X2 || !Y2)
    return SDL_FALSE;
  SDL_Rect r = *rect;
  int x1 = *X1, y1 = *Y1, x2 = *X2, y2 = *Y2;
  int dx = x2 - x1, dy = y2 - y1;
  float t0 = 0.0f, t1 = 1.0f;
  int p[4] = {-dx, dx, -dy, dy};
  int q[4] = {x1 - r.x, r.x + r.w - x1, y1 - r.y, r.y + r.h - y1};
  for (int i = 0; i < 4; ++i) {
    if (p[i] == 0) {
      if (q[i] < 0) {
        *X1 = *X2 = *Y1 = *Y2 = 0; // sanitize
        return SDL_FALSE;
      }
      continue;
    }
    float t = (float)q[i] / (float)p[i];
    if (p[i] < 0) {
      if (t > t1) {
        *X1 = *X2 = *Y1 = *Y2 = 0;
        return SDL_FALSE;
      }
      if (t > t0)
        t0 = t;
    } else {
      if (t < t0) {
        *X1 = *X2 = *Y1 = *Y2 = 0;
        return SDL_FALSE;
      }
      if (t < t1)
        t1 = t;
    }
  }
  if (t1 < t0) {
    *X1 = *X2 = *Y1 = *Y2 = 0;
    return SDL_FALSE;
  }
  *X1 = x1 + (int)(t0 * dx);
  *Y1 = y1 + (int)(t0 * dy);
  *X2 = x1 + (int)(t1 * dx);
  *Y2 = y1 + (int)(t1 * dy);
  return SDL_TRUE;
}

SDL_bool SDL_HasIntersectionF(const SDL_FRect *A, const SDL_FRect *B) {
  if (!A || !B)
    return SDL_FALSE;
  return (A->x < B->x + B->w && A->x + A->w > B->x && A->y < B->y + B->h &&
          A->y + A->h > B->y)
             ? SDL_TRUE
             : SDL_FALSE;
}

SDL_bool SDL_IntersectFRect(const SDL_FRect *A, const SDL_FRect *B,
                            SDL_FRect *result) {
  if (!A || !B || !result)
    return SDL_FALSE;
  float x1 = A->x > B->x ? A->x : B->x;
  float x2 = A->x + A->w < B->x + B->w ? A->x + A->w : B->x + B->w;
  float y1 = A->y > B->y ? A->y : B->y;
  float y2 = A->y + A->h < B->y + B->h ? A->y + A->h : B->y + B->h;
  if (x2 <= x1 || y2 <= y1) {
    result->x = result->y = result->w = result->h = 0.0f;
    return SDL_FALSE;
  }
  result->x = x1;
  result->y = y1;
  result->w = x2 - x1;
  result->h = y2 - y1;
  return SDL_TRUE;
}

void SDL_UnionFRect(const SDL_FRect *A, const SDL_FRect *B, SDL_FRect *result) {
  if (!A || !B || !result)
    return;
  float x1 = A->x < B->x ? A->x : B->x;
  float y1 = A->y < B->y ? A->y : B->y;
  float x2 = A->x + A->w > B->x + B->w ? A->x + A->w : B->x + B->w;
  float y2 = A->y + A->h > B->y + B->h ? A->y + A->h : B->y + B->h;
  result->x = x1;
  result->y = y1;
  result->w = x2 - x1;
  result->h = y2 - y1;
}

SDL_bool SDL_EncloseFPoints(const SDL_FPoint *points, int count,
                            const SDL_FRect *clip, SDL_FRect *result) {
  if (!points || count <= 0 || !result)
    return SDL_FALSE;

  float minx = points[0].x, maxx = points[0].x;
  float miny = points[0].y, maxy = points[0].y;
  for (int i = 1; i < count; ++i) {
    if (points[i].x < minx)
      minx = points[i].x;
    if (points[i].x > maxx)
      maxx = points[i].x;
    if (points[i].y < miny)
      miny = points[i].y;
    if (points[i].y > maxy)
      maxy = points[i].y;
  }
  SDL_FRect r = {minx, miny, maxx - minx + 1.0f, maxy - miny + 1.0f};

  if (clip) {
    if (!SDL_IntersectFRect(&r, clip, result)) {
      SDL_memset(result, 0, sizeof(*result));
      return SDL_FALSE;
    }
  } else {
    *result = r;
  }
  return SDL_TRUE;
}

SDL_bool SDL_IntersectFRectAndLine(const SDL_FRect *rect, float *X1, float *Y1,
                                   float *X2, float *Y2) {
  if (!rect || !X1 || !Y1 || !X2 || !Y2)
    return SDL_FALSE;
  int ix1 = (int)roundf(*X1), iy1 = (int)roundf(*Y1);
  int ix2 = (int)roundf(*X2), iy2 = (int)roundf(*Y2);
  SDL_Rect r = {(int)rect->x, (int)rect->y, (int)rect->w, (int)rect->h};
  SDL_bool ok = SDL_IntersectRectAndLine(&r, &ix1, &iy1, &ix2, &iy2);
  *X1 = (float)ix1;
  *Y1 = (float)iy1;
  *X2 = (float)ix2;
  *Y2 = (float)iy2;
  return ok;
}

// SDL_PointInFRect, SDL_RectEmpty, and SDL_FRectEmpty are SDL_FORCE_INLINE
// macros/functions in SDL_rect.h, so we do not provide out-of-line definitions.
