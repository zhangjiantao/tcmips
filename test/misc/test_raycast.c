//
// raycast 基准：软件浮点 vs libfixmath Q16.16 定点。
//
// 逐字复刻 demo/demo/raycast_softfp.c 的球求交内核（320×240），
// 用 BENCH_FIXED 宏区分 float 版和 libfixmath 版。
// 跑 -hold=0 -mode=fast 看指令数；-hold=1 看效果。

#include <dev/console.h>
#include <dev/syscall.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define W 640
#define H 480

#ifndef BENCH_FIXED
/* ===================== float 版 ===================== */

typedef struct { float x, y, z; } Vec3;

static inline float vdot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline void vnorm(Vec3 *v) {
  float l = sqrtf(vdot(*v, *v));
  if (l > 0.0f) { v->x /= l; v->y /= l; v->z /= l; }
}

static void render(uint32_t *vram) {
  Vec3 ctr = {0.0f, 0.0f, 3.5f};
  float r = 2.45f;
  Vec3 light = {-1.0f, 1.0f, -1.0f};
  vnorm(&light);
  float asp = (float)W / (float)H;

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      float px = (((float)x / W) * 2.0f - 1.0f) * asp;
      float py = -(((float)y / H) * 2.0f - 1.0f);
      Vec3 dir = {px, py, 1.0f};
      vnorm(&dir);
      Vec3 oc = {-ctr.x, -ctr.y, -ctr.z};
      float b = 2.0f * vdot(dir, oc);
      float c = vdot(oc, oc) - r * r;
      float disc = b * b - 4.0f * c;
      if (disc >= 0.0f) {
        float t = (-b - sqrtf(disc)) * 0.5f;
        if (t > 0.0f) {
          Vec3 hit = {dir.x * t, dir.y * t, dir.z * t};
          Vec3 n = {hit.x - ctr.x, hit.y - ctr.y, hit.z - ctr.z};
          vnorm(&n);
          float i = vdot(n, light);
          if (i < 0.0f) i = 0.0f;
          i += 0.05f; if (i > 1.0f) i = 1.0f;
          uint8_t cc = (uint8_t)(i * 255.0f);
          vram[y * W + x] = ((uint32_t)cc << 24) | ((uint32_t)cc << 16) |
                            ((uint32_t)cc << 8) | cc;
        }
      }
    }
  }
}

#else
/* ===================== libfixmath Q16.16 版 ===================== */

#include <fixmath/fix16.h>

typedef struct { fix16_t x, y, z; } Vec3;

static inline fix16_t vdot(Vec3 a, Vec3 b) {
  return fix16_mul(a.x, b.x) + fix16_mul(a.y, b.y) + fix16_mul(a.z, b.z);
}
static inline void vnorm(Vec3 *v) {
  fix16_t l = fix16_sqrt(vdot(*v, *v));
  if (l > 0) { v->x = fix16_div(v->x, l); v->y = fix16_div(v->y, l); v->z = fix16_div(v->z, l); }
}

static void render(uint32_t *vram) {
  Vec3 ctr = {0, 0, (fix16_t)(3 << 16) + (1 << 15)}; /* 3.5 */
  fix16_t r = (fix16_t)(2 << 16) + 29491;              /* 2.45 */
  Vec3 light = {-fix16_one, fix16_one, -fix16_one};
  vnorm(&light);
  fix16_t asp = fix16_div(fix16_from_int(4), fix16_from_int(3));

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      fix16_t px = fix16_mul(
          fix16_div(fix16_from_int(x * 2 - W), fix16_from_int(W)), asp);
      fix16_t py = fix16_div(fix16_from_int(H - y * 2), fix16_from_int(H));
      Vec3 dir = {px, py, fix16_one};
      vnorm(&dir);
      Vec3 oc = {-ctr.x, -ctr.y, -ctr.z};
      fix16_t b = fix16_mul(fix16_from_int(2), vdot(dir, oc));
      fix16_t c = vdot(oc, oc) - fix16_mul(r, r);
      fix16_t disc = fix16_mul(b, b) - fix16_mul(fix16_from_int(4), c);
      if (disc >= 0) {
        fix16_t two = fix16_from_int(2);
        fix16_t t = fix16_div(-b - fix16_sqrt(disc), two);
        if (t > 0) {
          Vec3 hit = {fix16_mul(dir.x, t), fix16_mul(dir.y, t),
                      fix16_mul(dir.z, t)};
          Vec3 n = {hit.x - ctr.x, hit.y - ctr.y, hit.z - ctr.z};
          vnorm(&n);
          fix16_t i = vdot(n, light);
          if (i < 0) i = 0;
          i += fix16_one / 20; if (i > fix16_one) i = fix16_one;
          uint8_t cc = (uint8_t)(i >> 8); /* Q16.16 → 0..255 */
          vram[y * W + x] = ((uint32_t)cc << 24) | ((uint32_t)cc << 16) |
                            ((uint32_t)cc << 8) | cc;
        }
      }
    }
  }
}
#endif /* BENCH_FIXED */

int main(void) {
  uint32_t *vram = (uint32_t *)tcm_pixel_console_init(CONSOLE_MODE_PIXEL_32, W);
  tcm_pixel_console_clear();
  render(vram);
  printf("mode=%s rendered\n",
#ifdef BENCH_FIXED
         "fixed"
#else
         "float"
#endif
  );
  return 0;
}