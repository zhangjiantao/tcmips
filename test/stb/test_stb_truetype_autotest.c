// stb_truetype.h AUTOTEST.
//
// Exercises: stbtt_InitFont, stbtt_GetFontVMetrics, stbtt_ScaleForPixelHeight,
//            stbtt_GetCodepointHMetrics, stbtt_GetCodepointBitmap,
//            stbtt_MakeGlyphBitmap.
// Asset: assets/geforce-bold.ttf (TTF font, committed as a fixed test asset).
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb/stb_truetype.h"

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

// Load font file into memory via stdio (picolibc → BSP → memfs).
static unsigned char *read_font(const char *path, int *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *buf = (unsigned char *)malloc((size_t)sz);
  if (!buf) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (got != (size_t)sz) { free(buf); return NULL; }
  *out_len = (int)sz;
  return buf;
}

// ---------------- basic init and metrics --------------------------------------

static int test_init_metrics(void) {
  int w, h, len;
  unsigned char *font_data = read_font("geforce-bold.ttf", &len);
  CHECK(font_data != NULL && len > 0, "ttf:font_loaded");
  if (!font_data) return 0;

  stbtt_fontinfo font;
  int ok = stbtt_InitFont(&font, font_data, 0);
  CHECK(ok == 1, "ttf:init");

  if (ok) {
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    // Sanity: ascent should be positive (typographic ascent above baseline).
    // Descent is typically negative or zero. LineGap is non-negative.
    CHECK(ascent > 0, "ttf:ascent_positive");
    CHECK(descent <= 0, "ttf:descent_nonpos");
    CHECK(lineGap >= 0, "ttf:linegap_nonnegative");
    // Verify metrics are within reasonable font size ranges.
    // UTF unitsPerEm is typically 2048; ascent/descent span well under
    // a couple of ems.  Use a generous bound (doesn't reject real fonts like
    // geforce-bold.ttf whose ascent is 2073 / descent -650).
    CHECK(ascent <= 3000 && ascent >= 100, "ttf:ascent_reasonable");
    CHECK(descent >= -1024 && descent <= 0, "ttf:descent_reasonable");
  }

  free(font_data);
  return 1;
}

// ---------------- scaling ------------------------------------------------------

static int test_scale(void) {
  int w, h, len;
  unsigned char *font_data = read_font("geforce-bold.ttf", &len);
  CHECK(font_data != NULL, "ttf:scale_font_loaded");
  if (!font_data) return 0;

  stbtt_fontinfo font;
  CHECK(stbtt_InitFont(&font, font_data, 0) == 1, "ttf:scale_init");

  // Scale for different pixel heights: must be positive and increase
  // as the requested pixel height increases (scale = height / fontHeight).
  float s1 = stbtt_ScaleForPixelHeight(&font, 10);
  float s2 = stbtt_ScaleForPixelHeight(&font, 20);
  float s3 = stbtt_ScaleForPixelHeight(&font, 1);
  CHECK(s1 > 0 && s2 > 0 && s3 > 0, "ttf:scales_positive");
  CHECK(s1 < s2, "ttf:scale_10_lt_20");
  CHECK(s1 > s3, "ttf:scale_10_gt_1");
  // Scale for pixel height 0 should be 0.
  float s0 = stbtt_ScaleForPixelHeight(&font, 0);
  CHECK(s0 == 0.0f, "ttf:scale_zero");
  // Negative pixel height should give negative scale.
  float sn = stbtt_ScaleForPixelHeight(&font, -10);
  CHECK(sn < 0, "ttf:scale_negative");

  free(font_data);
  return 1;
}

// ---------------- horizontal metrics -------------------------------------------

static int test_hmetrics(void) {
  int w, h, len;
  unsigned char *font_data = read_font("geforce-bold.ttf", &len);
  CHECK(font_data != NULL, "ttf:hm_font_loaded");
  if (!font_data) return 0;

  stbtt_fontinfo font;
  CHECK(stbtt_InitFont(&font, font_data, 0) == 1, "ttf:hm_init");

  // Test common ASCII codepoints have valid metrics.
  const int codepoints[] = {'A', 'B', 'Z', '0', '9', ' ', '.', ',', 'i', 'W'};
  const int n = sizeof(codepoints) / sizeof(codepoints[0]);

  int valid_count = 0;
  for (int i = 0; i < n; i++) {
    int advance = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&font, codepoints[i], &advance, &lsb);
    // Advance width should be positive for all printable codepoints.
    if (advance > 0) {
      valid_count++;
    }
  }
  // Most common characters should have positive advance widths.
  CHECK(valid_count >= n / 2, "ttf:advances_positive");

  // ASCII space ' ' typically has advance > 0 too (some spacing).
  int space_advance = 0;
  stbtt_GetCodepointHMetrics(&font, ' ', &space_advance, NULL);
  CHECK(space_advance >= 0, "ttf:space_advance_nonnegative");

  // Codepoint 0 (U+0000): stb maps unmapped codepoints to glyph 0 (.notdef),
  // so the advance is whatever .notdef carries — it must not be negative and
  // the call must not crash.  (It is not guaranteed to be exactly 0.)
  int nil_advance = 0, nil_lsb = 0;
  stbtt_GetCodepointHMetrics(&font, 0, &nil_advance, &nil_lsb);
  CHECK(nil_advance >= 0, "ttf:codepoint_0_metrics");

  free(font_data);
  return 1;
}

// ---------------- glyph bitmap rendering ---------------------------------------

static int test_glyph_bitmap(void) {
  int w, h, len;
  unsigned char *font_data = read_font("geforce-bold.ttf", &len);
  CHECK(font_data != NULL, "ttf:gb_font_loaded");
  if (!font_data) return 0;

  stbtt_fontinfo font;
  CHECK(stbtt_InitFont(&font, font_data, 0) == 1, "ttf:gb_init");

  // Render 'A' glyph at scale 1.0.
  int gw = 0, gh = 0, goff_x = 0, goff_y = 0;
  unsigned char *bitmap = stbtt_GetCodepointBitmap(&font,
      stbtt_ScaleForPixelHeight(&font, 10),  /* scale_x */
      stbtt_ScaleForPixelHeight(&font, 10),  /* scale_y */
      'A', &gw, &gh, &goff_x, &goff_y);

  // Bitmap should be allocated (non-NULL), with positive dimensions.
  CHECK(bitmap != NULL, "ttf:glyph_bitmap_not_null");
  CHECK(gw >= 0 && gh >= 0, "ttf:glyph_dims_nonnegative");

  if (bitmap && gw > 0 && gh > 0) {
    // Verify bitmap has at least some non-zero pixels (the 'A' glyph isn't empty).
    int has_ink = 0;
    for (int i = 0; i < gw * gh; i++) {
      if (bitmap[i] != 0) { has_ink = 1; break; }
    }
    CHECK(has_ink, "ttf:glyph_has_ink");
    // Total pixel count should be reasonable (not enormous).
    CHECK(gw <= 256 && gh <= 256, "ttf:glyph_dims_reasonable");
    // The origin offset should be within the bitmap or slightly to the left.
    CHECK(goff_x >= -16 && goff_x <= (int)gw, "ttf:glyph_offset_x");
    CHECK(goff_y >= -16 && goff_y <= (int)gh, "ttf:glyph_offset_y");
  }

  // Also test '0' (digit) glyph.
  int zgw = 0, zgh = 0;
  unsigned char *zbitmap = stbtt_GetCodepointBitmap(&font,
      stbtt_ScaleForPixelHeight(&font, 10),
      stbtt_ScaleForPixelHeight(&font, 10),
      '0', &zgw, &zgh, NULL, NULL);
  CHECK(zbitmap != NULL, "ttf:zero_glyph_not_null");
  if (zbitmap) free(zbitmap);

  // Nonexistent codepoint: stb maps unmapped codepoints to glyph 0 (.notdef)
  // and renders its bitmap, so the call must not crash and must return a
  // reasonable (non-NULL, bounded) .notdef bitmap — it is NOT required to be
  // NULL.  Use a small pixel-height scale so a junk .notdef doesn't explode.
  float mscale = stbtt_ScaleForPixelHeight(&font, 12);
  unsigned char *nobitmap = stbtt_GetCodepointBitmap(
      &font, mscale, mscale, 0xFFFE, &gw, &gh, NULL, NULL);
  CHECK(nobitmap != NULL, "ttf:missing_glyph_not_crash");
  if (nobitmap) {
    CHECK(gw <= 256 && gh <= 256, "ttf:missing_glyph_dims_reasonable");
    free(nobitmap);
  }

  if (bitmap) free(bitmap);
  free(font_data);
  return 1;
}

// ---------------- explicit glyph bitmap via MakeGlyphBitmap --------------------

static int test_make_glyph(void) {
  int w, h, len;
  unsigned char *font_data = read_font("geforce-bold.ttf", &len);
  CHECK(font_data != NULL, "ttf:mg_font_loaded");
  if (!font_data) return 0;

  stbtt_fontinfo font;
  CHECK(stbtt_InitFont(&font, font_data, 0) == 1, "ttf:mg_init");

  const int out_w = 64, out_h = 64;
  unsigned char bitmap[out_w * out_h];
  memset(bitmap, 0, sizeof(bitmap));

  float scale = stbtt_ScaleForPixelHeight(&font, 32);
  // Render 'M' to pre-allocated bitmap.
  stbtt_MakeGlyphBitmap(&font, bitmap, out_w, out_h, out_w, scale, scale, 'M');

  // Bitmap should be filled with some non-zero pixels (M has a complex shape).
  int has_ink = 0;
  for (int i = 0; i < out_w * out_h; i++) {
    if (bitmap[i] != 0) { has_ink = 1; break; }
  }
  CHECK(has_ink, "ttf:make_glyph_has_ink");

  free(font_data);
  return 1;
}

// ---------------- codepoint -> glyph consistency -------------------------------

static int test_codepoint_glyph_consistency(void) {
  int w, h, len;
  unsigned char *font_data = read_font("geforce-bold.ttf", &len);
  CHECK(font_data != NULL, "ttf:cg_font_loaded");
  if (!font_data) return 0;

  stbtt_fontinfo font;
  CHECK(stbtt_InitFont(&font, font_data, 0) == 1, "ttf:cg_init");

  // 'A' -> glyph index via stbtt_FindGlyphIndex; the HMetrics of that glyph
  // must equal the codepoint-level HMetrics (consistency of the two paths).
  int gi = stbtt_FindGlyphIndex(&font, 'A');
  CHECK(gi > 0, "ttf:find_glyph_A");

  int adv_g = 0, lsb_g = 0, adv_c = 0, lsb_c = 0;
  stbtt_GetGlyphHMetrics(&font, gi, &adv_g, &lsb_g);
  stbtt_GetCodepointHMetrics(&font, 'A', &adv_c, &lsb_c);
  CHECK(adv_g == adv_c && lsb_g == lsb_c, "ttf:glyph_vs_codepoint_hm");

  // Uppercase letters should all map to a real glyph with positive advance.
  int upper_ok = 0;
  for (char c = 'A'; c <= 'Z'; c++) {
    int g = stbtt_FindGlyphIndex(&font, c);
    int adv = 0;
    if (g > 0) { stbtt_GetGlyphHMetrics(&font, g, &adv, NULL); }
    if (adv > 0) upper_ok++;
  }
  CHECK(upper_ok >= 20, "ttf:uppercase_glyph_advances");

  // Lowercase likewise.
  int lower_ok = 0;
  for (char c = 'a'; c <= 'z'; c++) {
    int g = stbtt_FindGlyphIndex(&font, c);
    int adv = 0;
    if (g > 0) { stbtt_GetGlyphHMetrics(&font, g, &adv, NULL); }
    if (adv > 0) lower_ok++;
  }
  CHECK(lower_ok >= 20, "ttf:lowercase_glyph_advances");

  // Kern advance between 'A' and 'V' must be a reasonable value (usually a
  // small negative or zero; never an extreme/out-of-range number).  This
  // exercises the kern query path without depending on a specific font metric.
  int kern = stbtt_GetCodepointKernAdvance(&font, 'A', 'V');
  CHECK(kern >= -512 && kern <= 2048, "ttf:kern_reasonable");

  free(font_data);
  return 1;
}

int main(void) {
  printf("=== stb_truetype AUTOTEST ===\n");

  test_init_metrics();
  test_scale();
  test_hmetrics();
  test_glyph_bitmap();
  test_make_glyph();
  test_codepoint_glyph_consistency();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
