// stb_image.h AUTOTEST.
//
// Exercises: stbi_load / stbi_load_from_memory / stbi_image_free,
//            pixel format and dimension verification.
// Asset: assets/test_image.png (4x4 RGB PNG, committed as a fixed test asset).
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb/stb_image.h"

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

// Read entire file into memory via stdio (picolibc → BSP → memfs).
static unsigned char *read_file(const char *filename, int *out_len) {
  FILE *f = fopen(filename, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len <= 0) { fclose(f); return NULL; }
  unsigned char *buf = (unsigned char *)malloc((size_t)len);
  if (!buf) { fclose(f); return NULL; }
  size_t read = fread(buf, 1, (size_t)len, f);
  fclose(f);
  if (read != (size_t)len) { free(buf); return NULL; }
  *out_len = (int)len;
  return buf;
}

// ---------------- file-based load (stbi_load via memfs FILE*) -----------------

static int test_load_file(void) {
  int w, h, channels;
  unsigned char *img = stbi_load("test_image.png", &w, &h, &channels, 0);

  CHECK(img != NULL, "img:file_load_not_null");
  CHECK(w == 4 && h == 4, "img:file_dimensions");
  CHECK(channels == 3, "img:file_channels_rgb");

  if (img) {
    // Verify pixel values match generator pattern: R=x*64, G=y*64, B=(x+y)*32.
    // Row-major, top to bottom.
    int ok = 1;
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        int idx = (y * 4 + x) * 3;
        unsigned char r = img[idx];
        unsigned char g = img[idx + 1];
        unsigned char b = img[idx + 2];
        if (r != (unsigned char)(x * 64) ||
            g != (unsigned char)(y * 64) ||
            b != (unsigned char)((x + y) * 32)) {
          ok = 0;
          printf("    pixel(%d,%d): got (%d,%d,%d) want (%d,%d,%d)\n",
                 x, y, r, g, b, x * 64, y * 64, (x + y) * 32);
        }
      }
    }
    CHECK(ok, "img:file_pixels_correct");
    stbi_image_free(img);
  }

  return 1;
}

// ---------------- memory-based load (stbi_load_from_memory) --------------------

static int test_load_memory(void) {
  int w, h, channels, len;
  unsigned char *file_data = read_file("test_image.png", &len);
  CHECK(file_data != NULL, "img:file_read");

  unsigned char *img = stbi_load_from_memory(file_data, len, &w, &h, &channels, 0);
  CHECK(img != NULL, "img:mem_load_not_null");
  CHECK(w == 4 && h == 4, "img:mem_dimensions");
  CHECK(channels == 3, "img:mem_channels_rgb");

  if (img && file_data) {
    // Verify same pixel pattern as file test.
    int ok = 1;
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        int idx = (y * 4 + x) * 3;
        if (img[idx] != (unsigned char)(x * 64) ||
            img[idx + 1] != (unsigned char)(y * 64) ||
            img[idx + 2] != (unsigned char)((x + y) * 32)) {
          ok = 0;
        }
      }
    }
    CHECK(ok, "img:mem_pixels_correct");
    stbi_image_free(img);
  }

  free(file_data);
  return 1;
}

// ---------------- explicit channel requests -----------------------------------

static int test_channel_requests(void) {
  int w, h, comp = 0;
  unsigned char *img;

  // Request 1 channel (grayscale): stbi converts to w*h*1 bytes, each pixel
  // covered by a single byte.  The fixed PNG's (0,0) is black, so luma is 0.
  img = stbi_load("test_image.png", &w, &h, &comp, 1);
  CHECK(img != NULL && w == 4 && h == 4, "img:1channel");
  if (img) {
    CHECK(img[0] == 0, "img:1channel_luma");   // (0,0) black -> luma 0
    stbi_image_free(img);
  }

  // Request 3 channels (RGB): data stride is w*h*3, RGB order.
  img = stbi_load("test_image.png", &w, &h, &comp, 3);
  CHECK(img != NULL && w == 4 && h == 4, "img:3channel");
  if (img) {
    // First pixel of the fixed asset is (R=0,G=0,B=0) at (0,0); second is
    // (R=64,G=0,B=32) at (1,0) per the generator formula.
    CHECK(img[0] == 0 && img[1] == 0 && img[2] == 0 &&
          img[3] == 64 && img[5] == 32, "img:3channel_rgb_order");
    stbi_image_free(img);
  }

  // Request 4 channels (RGBA): data stride is w*h*4, alpha present.
  img = stbi_load("test_image.png", &w, &h, &comp, 4);
  CHECK(img != NULL && w == 4 && h == 4, "img:4channel");
  if (img) {
    // First pixel: R=0,G=0,B=0,A=255 (opaque source).
    CHECK(img[0] == 0 && img[1] == 0 && img[2] == 0 && img[3] == 255,
          "img:4channel_alpha");
    stbi_image_free(img);
  }

  return 1;
}

// ---------------- error handling -----------------------------------------------

static int test_error(void) {
  int w, h, channels;

  // Nonexistent file should return NULL.
  unsigned char *img = stbi_load("nonexistent_file.png", &w, &h, &channels, 0);
  CHECK(img == NULL, "img:nonexistent_null");

  // Null filename should return NULL.
  img = stbi_load(NULL, &w, &h, &channels, 0);
  CHECK(img == NULL, "img:null_filename_null");

  // Invalid memory buffer (empty) — stbi_load_from_memory(0 bytes) should fail or
  // return NULL rather than crash.
  img = stbi_load_from_memory((const unsigned char *)"", 0, &w, &h, &channels, 0);
  CHECK(img == NULL, "img:empty_mem_null");

  return 1;
}

// ---------------- metadata: image info ------------------------------------------

static int test_info(void) {
  // stbi_info: check file existence + dimensions without loading pixel data.
  int w, h, channels;
  int ok = stbi_info("test_image.png", &w, &h, &channels);
  CHECK(ok == 1, "img:info_exists");
  CHECK(w == 4 && h == 4, "img:info_dimensions");
  CHECK(channels == 3 || channels == 4, "img:info_channels");

  // Nonexistent file should return 0.
  ok = stbi_info("nonexistent.png", &w, &h, &channels);
  CHECK(ok == 0, "img:info_nonexistent");

  return 1;
}

// ---------------- alternate IO: FILE*, callbacks, and 16-bit ------------------

typedef struct { const unsigned char *p; int len; int pos; } memio_t;

static int memio_read(void *user, char *data, int size) {
  memio_t *m = (memio_t *)user;
  int n = m->pos + size > m->len ? m->len - m->pos : size;
  if (n <= 0) return 0;
  memcpy(data, m->p + m->pos, (size_t)n);
  m->pos += n;
  return n;
}
static void memio_skip(void *user, int n) {
  memio_t *m = (memio_t *)user;
  m->pos += n;
  if (m->pos < 0) m->pos = 0;
  if (m->pos > m->len) m->pos = m->len;
}
static int memio_eof(void *user) {
  return ((memio_t *)user)->pos >= ((memio_t *)user)->len;
}

static int test_io_variants(void) {
  int w, h, channels, len;
  unsigned char *data = read_file("test_image.png", &len);
  CHECK(data != NULL, "img:io_file_read");

  // stbi_load_from_file(FILE*): feed via stdio on the memfs-backed file.
  if (data) {
    FILE *f = fopen("test_image.png", "rb");
    CHECK(f != NULL, "img:io_fopen");
    if (f) {
      unsigned char *img = stbi_load_from_file(f, &w, &h, &channels, 0);
      CHECK(img != NULL && w == 4 && h == 4 && channels == 3, "img:from_file");
      if (img) stbi_image_free(img);
      fclose(f);
    }
  }

  // stbi_load_from_callbacks with a memory-backed io_callback.
  if (data) {
    memio_t m = { data, len, 0 };
    stbi_io_callbacks cb;
    cb.read = memio_read; cb.skip = memio_skip; cb.eof = memio_eof;
    unsigned char *img = stbi_load_from_callbacks(&cb, &m, &w, &h, &channels, 0);
    CHECK(img != NULL && w == 4 && h == 4 && channels == 3, "img:from_callbacks");
    if (img) {
      // (0,0) must be black, matching the fixed asset.
      CHECK(img[0] == 0 && img[1] == 0 && img[2] == 0, "img:cb_pixel");
      stbi_image_free(img);
    }
  }

  // stbi_load_16: 8-bit PNG upsampled to 16-bit; (0,0) black stays 0x0000.
  if (data) {
    int w16, h16, c16;
    stbi_us *img16 = stbi_load_16("test_image.png", &w16, &h16, &c16, 0);
    CHECK(img16 != NULL && w16 == 4 && h16 == 4 && c16 == 3, "img:load16");
    if (img16) {
      CHECK(img16[0] == 0 && img16[1] == 0 && img16[2] == 0, "img:load16_black");
      // 8-bit value 64 -> 16-bit 64*257=16448 (R of pixel (1,0)).
      CHECK(img16[3] == (stbi_us)(64 * 257), "img:load16_upsample");
      stbi_image_free(img16);
    }
  }

  if (data) free(data);
  return 1;
}

// ---------------- memory-based explicit channels -------------------------------

static int test_memory_channels(void) {
  int w, h, channels, len;
  unsigned char *file_data = read_file("test_image.png", &len);
  CHECK(file_data != NULL, "img:mem_file_read");
  if (!file_data) return 1;

  // req==0: keep source channel count (the asset is RGB -> 3).
  unsigned char *img = stbi_load_from_memory(file_data, len, &w, &h, &channels, 0);
  CHECK(img != NULL && w == 4 && h == 4 && channels == 3, "img:mem_req0_src3");
  if (img) stbi_image_free(img);

  // req==1..4: output uses exactly req channels; verify pixel layout of the
  // first pixel for each requested count.
  for (int req = 1; req <= 4; req++) {
    img = stbi_load_from_memory(file_data, len, &w, &h, &channels, req);
    char d[48];
    snprintf(d, sizeof(d), "(req=%d)", req);
    t_check(img != NULL && w == 4 && h == 4, "img:mem_req_dim", d);
    if (req == 4 && img) {
      // RGBA: first pixel (0,0,0) alpha must be opaque.
      t_check(img[3] == 255, "img:mem_req4_alpha", d);
    } else if (req == 3 && img) {
      // RGB: first pixel must be pure black.
      t_check(img[0] == 0 && img[1] == 0 && img[2] == 0, "img:mem_req3_black", d);
    }
    if (img) stbi_image_free(img);
  }

  free(file_data);
  return 1;
}

int main(void) {
  printf("=== stb_image AUTOTEST ===\n");

  test_load_file();
  test_load_memory();
  test_io_variants();
  test_channel_requests();
  test_error();
  test_info();
  test_memory_channels();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
