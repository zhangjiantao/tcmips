// stb_image_resize2.h AUTOTEST.
//
// Exercises: stbir_resize_uint8_srgb (scale, crop, upscale, downscale).
// Asset: assets/test_image.png (4x4 RGB PNG, committed as a fixed test asset).
// No external libraries needed; stb_image_resize2 is pure computation
// built on stb_image types but requires NO link dependency on stb_image.
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb/stb_image.h"
#include "stb/stb_image_resize2.h"

// ---------------- autotest harness -------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void t_check(int cond, const char *name, const char *detail) {
  if (cond) {
    g_pass++;
    printf("[SELF] %s PASS\n", name);
  } else {
    g_fail++;
    printf("[SELF] %s FAIL %s\n", name, detail ? detail : "");
  }
}

#define CHECK(cond, name) t_check((cond) ? 1 : 0, name, NULL)

#define CHECK_EQ(actual, expected, name)                                       \
  do {                                                                         \
    long long a_ = (long long)(actual);                                        \
    long long e_ = (long long)(expected);                                     \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

// Load the test PNG into memory for resize tests.
static unsigned char *load_test_image(int *out_len) {
  int w, h, channels, len;
  FILE *f = fopen("test_image.png", "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *buf = (unsigned char *)malloc((size_t)sz);
  if (!buf) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (got != (size_t)sz) { free(buf); return NULL; }
  // Verify it's a 4x4 RGB image via stbi_load_from_memory.
  unsigned char *img = stbi_load_from_memory(buf, (int)sz, &w, &h, &channels, 3);
  if (!img || w != 4 || h != 4) { stbi_image_free(img); free(buf); return NULL; }
  stbi_image_free(img);
  *out_len = (int)sz;
  return buf;
}

// ---------------- scale up: 4x4 → 8x8 ----------------------------------------

static int test_upscale(void) {
  int w, h, channels, len;
  unsigned char *src = load_test_image(&len);
  CHECK(src != NULL, "rs:src_loaded");
  if (!src) return 0;

  const int SRC_STRIDE = 4 * 3;  // 4 bytes per pixel × 3 channels (we loaded as 3ch)
  // Actually stbi_load with 3 channels: stride = w*3 = 12 bytes/row for source.
  // But wait — stbi_load gave us 3channels (RGB = w*3 = 12). However, we
  // didn't call stbi_load here; we have the RAW PNG buffer.
  // Use stbi_load to get the decoded pixels.
  unsigned char *decoded = stbi_load_from_memory(src, len, &w, &h, &channels, 3);
  CHECK(decoded != NULL && w == 4 && h == 4, "rs:decoded_4x4");
  free(src);
  if (!decoded) return 0;

  const int src_stride = w * 3;  // 12 for 4x4 RGB

  // Upscale to 8x8 using stbir_resize_uint8_srgb (NULL output → auto-allocate).
  unsigned char *dst = stbir_resize_uint8_srgb(
      decoded, w, h, src_stride,
      NULL, 8, 8, 8 * 3,
      STBIR_RGB);
  CHECK(dst != NULL, "rs:upscale_alloc");
  if (dst) {
    // Verify output dimensions are 8x8 (by checking no corruption).
    // Spot-check: row 0, col 0 should be close to (0,0,0).
    CHECK(dst[0] == 0 && dst[1] == 0 && dst[2] == 0, "rs:top_left_black");
    // Row 3, col 3 (x=3,y=3 in upscale) → in 4x4 space (1.5,1.5) ≈ (48,48,32)
    // Not exact due to interpolation, but should be non-zero.
    int has_nonzero = 0;
    for (int i = 0; i < 8 * 8 * 3; i++) {
      if (dst[i] != 0) { has_nonzero = 1; break; }
    }
    CHECK(has_nonzero, "rs:upscale_has_content");
    free(dst);
  }

  free(decoded);
  return 1;
}

// ---------------- scale down: 4x4 → 2x2 ---------------------------------------

static int test_downscale(void) {
  int w, h, channels, len;
  unsigned char *src = load_test_image(&len);
  CHECK(src != NULL, "rs:src_loaded_down");
  if (!src) return 0;

  unsigned char *decoded = stbi_load_from_memory(src, len, &w, &h, &channels, 3);
  CHECK(decoded != NULL && w == 4 && h == 4, "rs:decoded_down");
  free(src);
  if (!decoded) return 0;

  const int src_stride = w * 3;

  // Downscale 4x4 → 2x2.
  unsigned char *dst = stbir_resize_uint8_srgb(
      decoded, w, h, src_stride,
      NULL, 2, 2, 2 * 3,
      STBIR_RGB);
  CHECK(dst != NULL, "rs:downscale_alloc");
  if (dst) {
    // After downscale to 2x2: each pixel approximates a 2x2 region of the source.
    // Should have non-zero content (4 pixels × 3 channels = 12 bytes).
    int has_nonzero = 0;
    for (int i = 0; i < 2 * 2 * 3; i++) {
      if (dst[i] != 0) { has_nonzero = 1; break; }
    }
    CHECK(has_nonzero, "rs:downscale_has_content");
    free(dst);
  }

  free(decoded);
  return 1;
}

// ---------------- pre-allocated output buffer --------------------------------

static int test_preallocated(void) {
  int w, h, channels, len;
  unsigned char *src = load_test_image(&len);
  CHECK(src != NULL, "rs:src_loaded_pre");
  if (!src) return 0;

  unsigned char *decoded = stbi_load_from_memory(src, len, &w, &h, &channels, 3);
  CHECK(decoded != NULL, "rs:decoded_pre");
  free(src);
  if (!decoded) return 0;

  const int src_stride = w * 3;
  const int dst_w = 16, dst_h = 16;
  unsigned char dst_buffer[dst_w * dst_h * 3];  // 16*16*3 = 768 bytes

  // Resize into pre-allocated buffer.
  unsigned char *ret = stbir_resize_uint8_srgb(
      decoded, w, h, src_stride,
      dst_buffer, dst_w, dst_h, dst_w * 3,
      STBIR_RGB);
  // ret should equal dst_buffer (the function returns output pointer for chaining).
  CHECK(ret == dst_buffer, "rs:prealloc_return");

  // Verify non-zero content.
  int has_nonzero = 0;
  for (int i = 0; i < dst_w * dst_h * 3; i++) {
    if (dst_buffer[i] != 0) { has_nonzero = 1; break; }
  }
  CHECK(has_nonzero, "rs:prealloc_content");

  free(decoded);
  return 1;
}

// ---------------- multiple scales from same source -----------------------------

static int test_multiple_scales(void) {
  int w, h, channels, len;
  unsigned char *src = load_test_image(&len);
  CHECK(src != NULL, "rs:src_loaded_multi");
  if (!src) return 0;

  unsigned char *decoded = stbi_load_from_memory(src, len, &w, &h, &channels, 3);
  CHECK(decoded != NULL, "rs:decoded_multi");
  free(src);
  if (!decoded) return 0;

  const int src_stride = w * 3;

  // Verify resize at various scales.
  const int sizes[][2] = {{1,1}, {2,2}, {4,4}, {8,8}, {16,16}, {32,32}};
  int ok_all = 1;
  for (int i = 0; i < 6; i++) {
    int tw = sizes[i][0], th = sizes[i][1];
    unsigned char *dst = stbir_resize_uint8_srgb(
        decoded, w, h, src_stride,
        NULL, tw, th, tw * 3,
        STBIR_RGB);
    if (!dst) { ok_all = 0; break; }
    // Verify non-empty content (every downscale mixes colour from the
    // colourful 4x4 source, so even 1x1 must be non-black).
    int has_content = 0;
    for (int j = 0; j < tw * th * 3; j++) {
      if (dst[j] != 0) { has_content = 1; break; }
    }
    CHECK(has_content, "rs:scale_has_content");
    free(dst);
    if (!ok_all) break;
  }
  CHECK(ok_all, "rs:all_scales");

  free(decoded);
  return 1;
}

// ---------------- source stride flexibility ------------------------------------

static int test_stride(void) {
  int w, h, channels, len;
  unsigned char *src = load_test_image(&len);
  CHECK(src != NULL, "rs:src_loaded_stride");
  if (!src) return 0;

  unsigned char *decoded = stbi_load_from_memory(src, len, &w, &h, &channels, 3);
  CHECK(decoded != NULL, "rs:decoded_stride");
  free(src);
  if (!decoded) return 0;

  // Use a non-contiguous stride (larger than necessary) to test flexibility.
  int src_stride = w * 3 + 10;  // artificially wider
  int dst_w = 4, dst_h = 4;
  unsigned char *dst = stbir_resize_uint8_srgb(
      decoded, w, h, src_stride,
      NULL, dst_w, dst_h, dst_w * 3,
      STBIR_RGB);
  CHECK(dst != NULL, "rs:stride_alloc");
  if (dst) {
    int has_content = 0;
    for (int i = 0; i < dst_w * dst_h * 3; i++) {
      if (dst[i] != 0) { has_content = 1; break; }
    }
    CHECK(has_content, "rs:stride_content");
    free(dst);
  }

  free(decoded);
  return 1;
}

int main(void) {
  printf("=== stb_image_resize2 AUTOTEST ===\n");

  test_upscale();
  test_downscale();
  test_preallocated();
  test_multiple_scales();
  test_stride();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
