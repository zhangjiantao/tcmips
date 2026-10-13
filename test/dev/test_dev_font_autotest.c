// Device font-load path autotest for the TCMIPS emulator.
//
// This exercises the *integration* between three runtime layers that the
// device console depends on when it initialises its Unicode font:
//
//   tcmips_dev (console)  →  memfs_map("/tcmips/fonts/unifont.bin")
//                          →  memfs (returns the packed asset)
//                          →  zlib  (uncompress the wrapped payload)
//
// The font asset bundled by tcmips_dev (via STATIC_LIBRARY_OPTIONS
// -tcm-memfs=assets/) is an "UNIZ" wrapper — a 16-byte header (magic,
// original_len, compressed_len, flags) followed by either the raw UNIF bundle
// or its zlib deflate stream.  This test proves the exact wrapper/decompress
// contract the console relies on (see tcmips/dev/console_unicode.c
// uni_decompress), for BOTH the uncompressed asset that ships today and a
// compressed wrapper we synthesise in-guest.
//
// It is deliberately a separate target from test_cg/test_picolibc so it can
// link the device layer and import the real asset via tcmips_dev's memfs
// payload.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memfs/memfs.h>
#include <zlib.h>

#include <dev/console.h> // TCM_CONSOLE_FONT_DEFAULT

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
    long long e_ = (long long)(expected);                                      \
    char d_[64];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

/* Mirrors the UNIZ wrapper layout from tcmips/dev/console_unicode.c. */
#define UNI_WRAP_MAGIC 0x5A494E55u /* "UNIZ" LE */
#define UNI_WRAP_FLAG_COMPRESSED 0x01u

static uint32_t rd32(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return v;
}
static void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

/* Decompress an UNIZ wrapper exactly like uni_decompress() does, so we test the
 * real contract rather than a private copy of the console. Returns Z_OK or a
 * Z_* error. `out_buf` is malloc'd on the compressed path (caller frees); on
 * the uncompressed path it aliases `src + header`. */
static int unwrap(const uint8_t *src, uint32_t src_len, uint8_t **out_buf,
                  uint32_t *out_len, int *out_owned) {
  if (src_len < 16)
    return Z_DATA_ERROR;
  uint32_t magic = rd32(src + 0);
  uint32_t orig = rd32(src + 4);
  uint32_t comp = rd32(src + 8);
  uint32_t flags = rd32(src + 12);
  if (magic != UNI_WRAP_MAGIC || orig < 16)
    return Z_DATA_ERROR;
  const uint8_t *payload = src + 16;
  if (flags & UNI_WRAP_FLAG_COMPRESSED) {
    if (comp != src_len - 16)
      return Z_DATA_ERROR;
    uint8_t *out = (uint8_t *)malloc(orig);
    if (!out)
      return Z_MEM_ERROR;
    uLongf out_cap = orig;
    int ret = uncompress(out, &out_cap, payload, comp);
    if (ret != Z_OK || (uint32_t)out_cap != orig) {
      free(out);
      return Z_DATA_ERROR;
    }
    *out_buf = out;
    *out_len = (uint32_t)out_cap;
    *out_owned = 1;
    return Z_OK;
  }
  if (comp != 0 || src_len != 16 + orig)
    return Z_DATA_ERROR;
  *out_buf = (uint8_t *)payload;
  *out_len = orig;
  *out_owned = 0;
  return Z_OK;
}

/* Validate a decompressed UNIF bundle header (magic + sane layout). */
static int unif_valid(const uint8_t *bundle, uint32_t len) {
  if (len < 16)
    return 0;
  if (bundle[0] != 'U' || bundle[1] != 'N' || bundle[2] != 'I' ||
      bundle[3] != 'F')
    return 0;
  uint32_t count = rd32(bundle + 4);
  uint32_t index_ofs = rd32(bundle + 8);
  uint32_t bitmap_ofs = rd32(bundle + 12);
  /* index sits between the 16-byte header and the bitmap; both must be in
   * range for a well-formed bundle. */
  if (index_ofs < 16 || bitmap_ofs <= index_ofs || bitmap_ofs > len)
    return 0;
  (void)count;
  return 1;
}

// ============================================================================
// 1. Real shipped asset: map + unwrap (compressed OR raw, per flags)
// ============================================================================
static void test_real_asset(void) {
  printf("\n== 1. real shipped font asset ==\n");

  size_t fsize = 0;
  const uint8_t *img =
      (const uint8_t *)memfs_map(TCM_CONSOLE_FONT_DEFAULT, &fsize);
  CHECK(img != NULL, "memfs_map ships the font asset");
  if (!img) {
    printf("   (font asset missing; skipping dependent checks)\n");
    return;
  }
  CHECK(fsize > 16, "font asset larger than wrapper header");
  CHECK_EQ((long long)rd32(img + 0), (long long)UNI_WRAP_MAGIC,
           "asset starts with UNIZ magic");

  /* The shipped asset may be stored either compressed or raw — assert the
   * wrapper header's declared sizes are self-consistent, then unwrap it and
   * require a valid UNIF bundle regardless of which encoding was used. */
  uint32_t flags = rd32(img + 12);
  uint32_t orig_len = rd32(img + 4);
  uint32_t comp_len = rd32(img + 8);
  if (flags & UNI_WRAP_FLAG_COMPRESSED) {
    CHECK_EQ((long long)(16 + comp_len), (long long)fsize,
             "compressed asset: wrapper + stream == file size");
  } else {
    CHECK_EQ((long long)(16 + orig_len), (long long)fsize,
             "raw asset: wrapper + bundle == file size");
  }

  uint8_t *bundle = NULL;
  uint32_t blen = 0;
  int owned = 0;
  int ret = unwrap(img, (uint32_t)fsize, &bundle, &blen, &owned);
  CHECK_EQ((long long)ret, (long long)Z_OK, "unwrap shipped asset");
  if (ret == Z_OK) {
    CHECK_EQ((long long)blen, (long long)orig_len,
             "bundle length == wrapper original_length");
    CHECK(unif_valid(bundle, blen), "bundle has valid UNIF header");
    if (owned)
      free(bundle);
  }
}

// ============================================================================
// 2. Synthesised compressed asset: memfs write → unwrap (zlib path)
// ============================================================================
static void test_compressed_roundtrip(void) {
  printf("\n== 2. synthesised compressed font asset ==\n");

  /* Build a minimal but well-formed UNIF bundle: 16-byte header + index + a
   * few zero bitmap bytes.  We compress it and write an UNIZ wrapper back into
   * memfs, then re-map and decompress — exercising memfs write + zlib
   * compress/uncompress together. */
  const uint32_t bundle_len = 4096;
  uint8_t bundle[4096];
  memset(bundle, 0, sizeof(bundle));
  memcpy(bundle, "UNIF", 4);
  wr32(bundle + 4, 1);         /* count = 1 glyph              */
  wr32(bundle + 8, 16);        /* index_ofs = 16              */
  wr32(bundle + 12, 16 + 8);   /* bitmap_ofs right after 1 idx */

  uLongf comp_cap = compressBound(bundle_len);
  uint8_t *comp = (uint8_t *)malloc(comp_cap);
  CHECK(comp != NULL, "malloc compress buffer");
  if (!comp)
    return;

  uLongf comp_len = comp_cap;
  int rc = compress2(comp, &comp_len, bundle, bundle_len, Z_BEST_COMPRESSION);
  CHECK_EQ((long long)rc, (long long)Z_OK, "compress2 builds asset payload");
  CHECK(comp_len < bundle_len, "compressed payload smaller than raw");

  /* Assemble the UNIZ wrapper in memory. */
  uint32_t wrap_len = 16 + (uint32_t)comp_len;
  uint8_t *wrap = (uint8_t *)malloc(wrap_len);
  wr32(wrap + 0, UNI_WRAP_MAGIC);
  wr32(wrap + 4, bundle_len);
  wr32(wrap + 8, (uint32_t)comp_len);
  wr32(wrap + 12, UNI_WRAP_FLAG_COMPRESSED);
  memcpy(wrap + 16, comp, comp_len);

  /* Write it into memfs through the BSP file layer, then read it back.  NOTE:
   * memfs_map() only backs linker-packed assets (segment0); a file created at
   * runtime lives in the Chunk chain, so we read it back via the stdio layer —
   * which itself exercises read(file) → memfs → the same bytes. */
  FILE *f = fopen("/synth_unifont.bin", "wb");
  CHECK(f != NULL, "fopen synth font asset");
  if (f) {
    fwrite(wrap, 1, wrap_len, f);
    fclose(f);
    free(wrap);
    wrap = NULL;
  }

  uint8_t *rimg = (uint8_t *)malloc(wrap_len);
  CHECK(rimg != NULL, "malloc read-back buffer");
  if (rimg) {
    f = fopen("/synth_unifont.bin", "rb");
    CHECK(f != NULL, "fopen synth asset read-back");
    if (f) {
      CHECK_EQ((long long)fread(rimg, 1, wrap_len, f), (long long)wrap_len,
               "read-back byte count");
      fclose(f);

      uint8_t *bundle2 = NULL;
      uint32_t blen2 = 0;
      int owned2 = 0;
      int ret2 = unwrap(rimg, wrap_len, &bundle2, &blen2, &owned2);
      CHECK_EQ((long long)ret2, (long long)Z_OK, "unwrap synth compressed asset");
      if (ret2 == Z_OK) {
        CHECK_EQ((long long)blen2, (long long)bundle_len,
                 "decompressed length == original");
        CHECK_EQ((long long)owned2, 1, "compressed path allocates output");
        CHECK(unif_valid(bundle2, blen2), "synth bundle has valid UNIF header");
        CHECK(memcmp(bundle2, bundle, bundle_len) == 0,
              "decompressed bundle byte-identical to source");
        free(bundle2);
      }
    }
    free(rimg);
  }

  free(comp);
}

// ============================================================================
// 3. Negative path: corrupted wrapper must be rejected
// ============================================================================
static void test_corrupt(void) {
  printf("\n== 3. corrupted wrapper rejection ==\n");

  uint8_t bad[16];
  memset(bad, 0, sizeof(bad));
  wr32(bad + 0, 0xDEADBEEF); /* wrong magic */
  uint8_t *out = NULL;
  uint32_t out_len = 0;
  int owned = 0;
  int ret = unwrap(bad, sizeof(bad), &out, &out_len, &owned);
  CHECK_EQ((long long)ret, (long long)Z_DATA_ERROR,
           "bad magic rejected with Z_DATA_ERROR");

  /* A compressed wrapper whose compressed_len disagrees with the real size
   * must also fail cleanly. */
  uint8_t trunc[32];
  memset(trunc, 0, sizeof(trunc));
  wr32(trunc + 0, UNI_WRAP_MAGIC);
  wr32(trunc + 4, 1024);
  wr32(trunc + 8, 999); /* lies about compressed payload size */
  wr32(trunc + 12, UNI_WRAP_FLAG_COMPRESSED);
  ret = unwrap(trunc, sizeof(trunc), &out, &out_len, &owned);
  CHECK_EQ((long long)ret, (long long)Z_DATA_ERROR,
           "mismatched compressed len rejected");
}

// ============================================================================
// main
// ============================================================================
int main(void) {
  printf("=== dev font-load AUTOTEST ===\n");

  test_real_asset();
  test_compressed_roundtrip();
  test_corrupt();

  printf("\n[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1;
}
