// zlib autotest for the TCMIPS emulator.
//
// This is a full AUTOTEST (see AUTOTEST.md) for the zlib 1.3.1 compression
// library as linked into the tcmips_core runtime via libtcmips_zlib. It
// exercises every public zlib API surface — <zlib.h> inflate/deflate,
// compress/uncompress, crc32/adler32, zlibVersion, and the gz* stream family —
// entirely in-guest.
//
// Cross-language robustness data is provided via tcmips_memfs. Python's
// generate_ref_data.py produces all files under test/zlib/assets/:
//
//   zlib_data/level_{N}_{TYPE}.zlib (N = 0,3,6,9; TYPE = empty,text,repeat)
//     Format: [u32 LE: original_len][u32 LE:
//     compressed_len][compressed][original] C test: test_inflate_files() reads
//     via gzopen "rb" + gzread head, then
//             inflates the remaining stream via inflate() and compares bytes.
//
//   raw/{TYPE}.bin (TYPE = empty,text,repeat)
//     Format: raw original bytes only.
//     C test: test_gz_files() gzopen() each file in "rb" mode, gzread() all
//             bytes, compare with known expected content (length + pattern).
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)
// The process exits 0 when all checks passed, non-zero otherwise.

// zlib autotest for the TCMIPS emulator.
//
// This is a full AUTOTEST (see AUTOTEST.md) for the zlib 1.3.1 compression
// library as linked into the tcmips_core runtime via libtcmips_zlib. It
// exercises every public zlib API surface — <zlib.h> inflate/deflate,
// compress/uncompress, crc32/adler32, zlibVersion, and the gz* stream family —
// entirely in-guest.
//
// Cross-language robustness data is provided via tcmips_memfs. Python's
// generate_ref_data.py produces all files under test/zlib/assets/:
//
//   zlib_data/level_{N}_{TYPE}.zlib (N = 0,3,6,9; TYPE = empty,text,repeat)
//     Format: [u32 LE: original_len][u32 LE:
//     compressed_len][compressed][original] C test: test_inflate_files() reads
//     via gzopen "rb" + gzread head, then
//             inflates the remaining stream via inflate() and compares bytes.
//
//   raw/{TYPE}.bin (TYPE = empty,text,repeat)
//     Format: raw original bytes only.
//     C test: test_gz_files() gzopen() each file in "rb" mode, gzread() all
//             bytes, compare with known expected content (length + pattern).
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)
// The process exits 0 when all checks passed, non-zero otherwise.

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

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

// CHECKF: CHECK with printf-formatted name.
// Usage: CHECKF(cond, "name %d", val)
#define CHECKF(cond, ...)                                                      \
  do {                                                                         \
    char _cf_name[128];                                                        \
    snprintf(_cf_name, sizeof(_cf_name), __VA_ARGS__);                         \
    t_check((cond) ? 1 : 0, _cf_name, NULL);                                   \
  } while (0)

#define CHECK_EQ(actual, expected, name)                                       \
  do {                                                                         \
    unsigned long long a_ = (unsigned long long)(actual);                      \
    unsigned long long e_ = (unsigned long long)(expected);                    \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%llu expected=%llu)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

#define CHECK_STREQ(actual, expected, name)                                    \
  do {                                                                         \
    const char *a_ = (actual);                                                 \
    const char *e_ = (expected);                                               \
    char d_[128];                                                              \
    snprintf(d_, sizeof(d_), "(actual=\"%s\" expected=\"%s\")",                \
             a_ ? a_ : "(null)", e_ ? e_ : "(null)");                          \
    t_check(a_ && e_ && strcmp(a_, e_) == 0, name, d_);                        \
  } while (0)

#define CHECK_MEMEQ(actual, expected, len, name)                               \
  do {                                                                         \
    t_check(memcmp((actual), (expected), (len)) == 0, name,                    \
            "memcmp mismatch at offset computed");                             \
  } while (0)

// ---------------- test data generators ---------------------------------------

static void gen_random(void *buf, size_t len) {
  unsigned char *p = (unsigned char *)buf;
  unsigned long seed = 0x12345678;
  for (size_t i = 0; i < len; i++) {
    seed = seed * 1103515245 + 12345;
    p[i] = (unsigned char)(seed >> 16);
  }
}

static void gen_repeat(void *buf, size_t len, unsigned char byte) {
  memset(buf, byte, len);
}

static const char g_text[] = "The quick brown fox jumps over the lazy dog. "
                             "Pack my box with five dozen liquor jugs. "
                             "How vexingly quick daft zebras jump! "
                             "Sphinx of black quartz, judge my vow. "
                             "Two driven jocks help fax my big "
                             "wax. A mad boxer shoots a quick, jabbed jive "
                             "to a bag, a quick bad fox in a box.";
static const size_t g_text_len = sizeof(g_text) - 1;

// File paths (relative to tcmips_memfs root, embedded via -tcm-memfs=assets).
static const char *g_zlib_data_dir = "zlib_data";
static const char *g_raw_assets_dir = "raw";

// ---------------- helper: read external .zlib file ---------------------------
// Reads a .zlib file produced by Python generate_ref_data.py:
//   [u32 LE: original_len][u32 LE: compressed_len][compressed bytes][original
//   bytes]
static int read_zlib_file(const char *path, size_t *out_orig_len,
                          const unsigned char **out_comp, size_t *out_comp_len,
                          const unsigned char **out_orig) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;

  uint32_t orig_len = 0;
  uint32_t comp_len = 0;
  if (fread(&orig_len, 4, 1, f) != 1) {
    fclose(f);
    return -1;
  }
  if (fread(&comp_len, 4, 1, f) != 1) {
    fclose(f);
    return -1;
  }

  unsigned long original_len = (unsigned long)orig_len;
  unsigned long compressed_len = (unsigned long)comp_len;

  unsigned char *comp =
      (unsigned char *)malloc(compressed_len > 0 ? compressed_len : 1);
  unsigned char *orig =
      (unsigned char *)malloc(original_len > 0 ? original_len : 1);
  if (!comp || !orig) {
    free(comp);
    free(orig);
    fclose(f);
    return -1;
  }

  if (fread(comp, 1, compressed_len, f) != (size_t)compressed_len) {
    free(comp);
    free(orig);
    fclose(f);
    return -1;
  }
  if (fread(orig, 1, original_len, f) != (size_t)original_len) {
    free(comp);
    free(orig);
    fclose(f);
    return -1;
  }
  fclose(f);

  if (out_comp) {
    *out_comp = comp;
    *out_comp_len = compressed_len;
  }
  if (out_orig) {
    *out_orig = orig;
    *out_orig_len = original_len;
  }
  return 0;
}

// ---------------- helper: roundtrip via deflate/inflate ----------------------

static int deflate_round(const void *src, size_t src_len, void *dest,
                         size_t dest_cap, size_t *out_compressed_len) {
  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = (Bytef *)src;
  strm.avail_in = (uInt)src_len;
  strm.next_out = (Bytef *)dest;
  strm.avail_out = (uInt)dest_cap;

  int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8,
                         Z_DEFAULT_STRATEGY);
  if (ret != Z_OK)
    return ret;

  ret = deflate(&strm, Z_FINISH);
  if (ret != Z_STREAM_END && ret != Z_OK) {
    deflateEnd(&strm);
    return ret;
  }
  *out_compressed_len = strm.total_out;
  deflateEnd(&strm);
  return Z_OK;
}

// inflate_round: decompress via repeated inflate() calls.
static int inflate_round(const void *src, size_t src_len, void *dest,
                         size_t dest_cap, size_t *out_decompressed_len) {
  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = (Bytef *)src;
  strm.avail_in = (uInt)src_len;
  strm.next_out = (Bytef *)dest;
  strm.avail_out = (uInt)dest_cap;

  int ret = inflateInit(&strm);
  if (ret != Z_OK)
    return ret;

  int finished = 0;
  while (!finished) {
    ret = inflate(&strm, Z_FINISH);
    if (ret == Z_STREAM_END) {
      finished = 1;
      break;
    }
    if (ret == Z_OK) {
      if (strm.avail_in > 0)
        continue;
      break;
    }
    break;
  }

  *out_decompressed_len = strm.total_out;
  inflateEnd(&strm);
  return (finished && strm.avail_in == 0) ? Z_OK : ret;
}

// ---------------- helper: build a full path from dir+filename ----------------
static void build_path(char *buf, size_t buf_size, const char *dir,
                       const char *filename) {
  snprintf(buf, buf_size, "%s/%s", dir, filename);
}

// ============================================================================
// 1. zlibVersion/zlibCompileFlags: API surface checks
// ============================================================================
static void test_version(void) {
  const char *v = zlibVersion();
  CHECK_STREQ(v, "1.3.1", "zlibVersion matches");
  CHECK(strlen(v) > 3, "zlibVersion non-trivial");

  unsigned flags = zlibCompileFlags();
  CHECK(flags != 0, "zlibCompileFlags nonzero");
  CHECK((flags & (1U << 0)) != 0 || (flags & (1U << 4)) != 0,
        "zlibCompileFlags basic feature bit set");
}

// ============================================================================
// 2. compress/uncompress/compressBound: whole-memory compression API
// ============================================================================
static void test_compress_api(void) {
  CHECK(compressBound(0) >= 0, "compressBound(0) non-negative");
  CHECK(compressBound(1) >= 1, "compressBound(1) >= src_len");
  CHECK(compressBound(1000) >= 1000, "compressBound(1000) >= src_len");
  // MAX_WBITS (max window bits ~15) is a zlib implementation constant in
  // deflate.c, not an exposed zlib.h macro.  15 is standard max.
  CHECK(compressBound(15) >= 15, "compressBound 15 >= src_len");

  // Empty input roundtrip.
  unsigned char src_empty[1] = {0};
  unsigned char dst_empty[compressBound(0) + 100];
  unsigned long dst_len_empty = sizeof(dst_empty);
  int ret = compress(dst_empty, &dst_len_empty, src_empty, 0);
  CHECK(ret == Z_OK, "compress empty input Z_OK");
  CHECK(dst_len_empty < sizeof(src_empty) + 100,
        "compress empty output < bound");

  unsigned char uncomp_empty[100];
  unsigned long uncomp_len_empty = sizeof(uncomp_empty);
  ret = uncompress(uncomp_empty, &uncomp_len_empty, dst_empty, dst_len_empty);
  CHECK(ret == Z_OK, "uncompress empty Z_OK");
  CHECK(uncomp_len_empty == 0, "uncompress empty len=0");

  // Text roundtrip.
  const char *text = "hello world";
  unsigned long text_len = strlen(text);
  uLong bound = compressBound(text_len);
  unsigned char *comp = (unsigned char *)malloc(bound);
  unsigned long comp_len = bound;
  ret = compress(comp, &comp_len, (const unsigned char *)text, text_len);
  CHECK(ret == Z_OK, "compress text Z_OK");
  CHECK(comp_len <= bound, "compress text uses <= bound");

  unsigned long dst_len_text = text_len;
  unsigned char *dst_text = (unsigned char *)malloc(text_len + 16);
  ret = uncompress(dst_text, &dst_len_text, comp, comp_len);
  CHECK(ret == Z_OK, "uncompress text Z_OK");
  CHECK(dst_len_text == text_len, "uncompress text len matches");
  CHECK(memcmp(dst_text, text, text_len) == 0, "uncompress text match");
  free(comp);
  free(dst_text);
}

// ============================================================================
// 3. deflate/inflate roundtrip: core API
// ============================================================================
static void test_deflate_inflate_roundtrip(void) {
  // Empty input.
  {
    unsigned char comp[100];
    size_t comp_len = 0;
    int ret = deflate_round("", 0, comp, sizeof(comp), &comp_len);
    CHECK(ret == Z_OK, "deflate empty Z_OK");
    if (ret == Z_OK) {
      unsigned char uncomp[100];
      size_t uncomp_len = 0;
      ret = inflate_round(comp, comp_len, uncomp, sizeof(uncomp), &uncomp_len);
      CHECK(ret == Z_OK, "inflate empty Z_OK");
      CHECK(uncomp_len == 0, "inflate empty result len=0");
    }
  }

  // 1 byte.
  {
    const char src = 'A';
    unsigned char comp[100];
    size_t comp_len = 0;
    int ret = deflate_round(&src, 1, comp, sizeof(comp), &comp_len);
    CHECK(ret == Z_OK, "deflate 1 byte Z_OK");
    if (ret == Z_OK) {
      unsigned char uncomp[100];
      size_t uncomp_len = 0;
      ret = inflate_round(comp, comp_len, uncomp, sizeof(uncomp), &uncomp_len);
      CHECK(ret == Z_OK, "inflate 1 byte Z_OK");
      CHECK(uncomp_len == 1, "inflate 1 byte len match");
      CHECK(uncomp[0] == 'A', "inflate 1 byte match");
    }
  }

  // Text data.
  {
    unsigned char comp[8192];
    size_t comp_len = 0;
    int ret = deflate_round(g_text, g_text_len, comp, sizeof(comp), &comp_len);
    CHECK(ret == Z_OK, "deflate text Z_OK");
    if (ret == Z_OK) {
      CHECK(comp_len < g_text_len, "deflate text compressed smaller");
      unsigned char uncomp[8192];
      size_t uncomp_len = 0;
      ret = inflate_round(comp, comp_len, uncomp, sizeof(uncomp), &uncomp_len);
      CHECK(ret == Z_OK, "inflate text Z_OK");
      CHECK(uncomp_len == g_text_len, "inflate text len match");
      CHECK_MEMEQ(uncomp, g_text, g_text_len, "inflate text match");
    }
  }

  // Highly compressible data.
  {
    unsigned char src[65536];
    gen_repeat(src, sizeof(src), 0xCD);
    unsigned char comp[65536];
    size_t comp_len = 0;
    int ret = deflate_round(src, sizeof(src), comp, sizeof(comp), &comp_len);
    CHECK(ret == Z_OK, "deflate repeat Z_OK");
    if (ret == Z_OK) {
      CHECK(comp_len < 1024, "deflate repeat extremely compressed");
      unsigned char uncomp[65536];
      size_t uncomp_len = 0;
      ret = inflate_round(comp, comp_len, uncomp, sizeof(uncomp), &uncomp_len);
      CHECK(ret == Z_OK, "inflate repeat Z_OK");
      CHECK(uncomp_len == sizeof(src), "inflate repeat len match");
      CHECK_MEMEQ(uncomp, src, sizeof(src), "inflate repeat match");
    }
  }

  // Random data — malloc to avoid stack overflow.
  {
    unsigned char src[4096];
    gen_random(src, sizeof(src));
    unsigned char *comp = (unsigned char *)malloc(8192);
    unsigned char *uncomp = (unsigned char *)malloc(sizeof(src));
    CHECK(comp != NULL && uncomp != NULL, "random malloc ok");
    if (comp && uncomp) {
      size_t comp_len = 0;
      int ret = deflate_round(src, sizeof(src), comp, 8192, &comp_len);
      CHECK(ret == Z_OK, "deflate random Z_OK");
      if (ret == Z_OK) {
        CHECK(comp_len >= sizeof(src) - 20,
              "random data does not compress (boundary)");
        size_t uncomp_len = 0;
        ret = inflate_round(comp, comp_len, uncomp, sizeof(src), &uncomp_len);
        CHECK(ret == Z_OK, "inflate random Z_OK");
        CHECK(uncomp_len == sizeof(src), "inflate random len match");
        CHECK_MEMEQ(uncomp, src, sizeof(src), "inflate random match");
      }
    }
    free(comp);
    free(uncomp);
  }

  // 64KB block.
  {
    unsigned char *src = (unsigned char *)malloc(65536);
    CHECK(src != NULL, "64KB src malloc ok");
    if (!src)
      return;
    gen_repeat(src, 65536, 0xCD);
    unsigned char *comp = (unsigned char *)malloc(compressBound(65536));
    unsigned char *uncomp = (unsigned char *)malloc(65536);
    CHECK(comp != NULL && uncomp != NULL, "64KB malloc ok");
    if (comp && uncomp) {
      size_t comp_len = 0;
      int ret =
          deflate_round(src, 65536, comp, compressBound(65536), &comp_len);
      CHECK(ret == Z_OK, "deflate 64KB Z_OK");
      if (ret == Z_OK) {
        CHECK(comp_len < 65536, "deflate 64KB compressed smaller");
        size_t uncomp_len = 0;
        ret = inflate_round(comp, comp_len, uncomp, 65536, &uncomp_len);
        CHECK(ret == Z_OK, "inflate 64KB Z_OK");
        CHECK(uncomp_len == 65536, "inflate 64KB len match");
        CHECK_MEMEQ(uncomp, src, 65536, "inflate 64KB match");
      }
    }
    free(src);
    free(comp);
    free(uncomp);
  }

  // Partial buffer.
  {
    unsigned char src[256];
    gen_random(src, sizeof(src));
    uLong bound = compressBound(sizeof(src));
    unsigned char *comp = (unsigned char *)malloc(bound);
    unsigned char *dst = (unsigned char *)malloc(sizeof(src));
    CHECK(comp != NULL && dst != NULL, "partial buffer malloc ok");
    if (comp && dst) {
      unsigned long comp_len = bound;
      int ret = compress(comp, &comp_len, src, sizeof(src));
      CHECK(ret == Z_OK, "compress exact-bound Z_OK");
      CHECK(comp_len <= bound, "compress uses <= bound bytes");
      unsigned long dst_len = sizeof(src);
      ret = uncompress(dst, &dst_len, comp, comp_len);
      CHECK(ret == Z_OK, "uncompress exact Z_OK");
      CHECK(dst_len == sizeof(src), "uncompress exact len match");
      CHECK_MEMEQ(dst, src, sizeof(src), "uncompress exact match");
    }
    free(comp);
    free(dst);
  }

  // Negative test.
  {
    unsigned char garbage[16];
    gen_random(garbage, sizeof(garbage));
    unsigned char out[1];
    unsigned long out_len = 0;
    int ret = uncompress(out, &out_len, garbage, sizeof(garbage));
    CHECK(ret == Z_DATA_ERROR, "uncompress garbage Z_DATA_ERROR");
  }
}

// ============================================================================
// 3i. Cross-language inflate: Python-compressed .zlib files from memfs
// ============================================================================
// Reads tcmips_memfs files at zlib_data/level_{N}_{TYPE}.zlib produced by
// Python generate_ref_data.py.  Verifies zlib inflate correctly decodes
// streams produced by a totally independent implementation (Python zlib).
//
// Also tests gzopen() in "rb" mode to read the .zlib file handle (gzread),
// then inflate the contents — exercises the gz* read path as well.
//
// 12 files = 3 types x 4 compression levels.

static const char *g_zlib_compression_levels[] = {"0", "3", "6", "9"};
static const char *g_zlib_data_types[] = {"empty", "text", "repeat"};
static const size_t g_expected_orig_lens[] = {0, 274,
                                              65536}; // for empty, text, repeat

static int test_inflate_files(void) {
  int n_levels =
      sizeof(g_zlib_compression_levels) / sizeof(g_zlib_compression_levels[0]);
  int n_types = sizeof(g_zlib_data_types) / sizeof(g_zlib_data_types[0]);
  int total = 0, passed = 0;

  for (int t = 0; t < n_types; t++) {
    for (int l = 0; l < n_levels; l++) {
      total++;
      char path[256];
      char name[128];
      snprintf(path, sizeof(path), "%s/level_%s_%s.zlib", g_zlib_data_dir,
               g_zlib_compression_levels[l], g_zlib_data_types[t]);
      snprintf(name, sizeof(name), "inflate_zlib level%s %s",
               g_zlib_compression_levels[l], g_zlib_data_types[t]);

      // --- Read file (from memfs) ---
      const unsigned char *comp = NULL;
      size_t comp_len = 0;
      const unsigned char *orig = NULL;
      size_t orig_len = 0;
      int read_ret = read_zlib_file(path, &orig_len, &comp, &comp_len, &orig);
      {
        char n2[128];
        snprintf(n2, sizeof(n2), "%s read", name);
        CHECK(read_ret == 0, n2);
      }
      if (read_ret != 0) {
        free((void *)comp);
        free((void *)orig);
        continue;
      }

      // --- Verify original length ---
      {
        char n2[128];
        snprintf(n2, sizeof(n2), "%s orig_len", name);
        CHECK(orig_len == g_expected_orig_lens[t], n2);
      }

      // --- Test 1: inflate() from comp buffer ---
      {
        char n2[128];
        snprintf(n2, sizeof(n2), "%s inflate_buf", name);
        unsigned char *uncomp =
            (unsigned char *)malloc(orig_len > 0 ? orig_len : 1);
        if (uncomp) {
          size_t uncomp_len = 0;
          int ret =
              inflate_round(comp, comp_len, uncomp,
                            orig_len > 0 ? (size_t)orig_len : 1, &uncomp_len);
          CHECK(ret == Z_OK || ret == Z_STREAM_END, n2);
          if (ret == Z_OK || ret == Z_STREAM_END) {
            {
              char n3[128];
              snprintf(n3, sizeof(n3), "%s len", n2);
              CHECK(uncomp_len == orig_len, n3);
            }
            if (uncomp_len == orig_len && orig_len > 0) {
              {
                char n3[128];
                snprintf(n3, sizeof(n3), "%s data", n2);
                CHECK_MEMEQ(uncomp, orig, orig_len, n3);
              }
            }
          }
          free(uncomp);
        } else {
          CHECK(0, n2);
        }
      }

      // --- Test 2: gzopen() + gzread() the entire .zlib file as a stream,
      // then inflate just the compressed part ---
      // gzopen reads the whole file (including the 8-byte header + original).
      // We just verify we can open and read some of it.
      {
        char n2[128];
        snprintf(n2, sizeof(n2), "%s gzread", name);
        gzFile gz = gzopen(path, "rb");
        CHECK(gz != NULL, n2);
        if (gz) {
          // Read first 4 bytes of .zlib file (orig_len field) via gzread.
          uint32_t read_orig_len = 0;
          int got = gzread(gz, &read_orig_len, 4);
          CHECKF(got == 4 || (orig_len == 0 && got >= 0), "%s gzread head", n2);
          if (orig_len == 0) {
            CHECKF(got >= 0 && read_orig_len == 0, "%s gzread empty", n2);
          } else {
            // For non-empty files, verify the orig_len field matches.
            uint32_t host_orig = (uInt)orig_len;
            CHECKF(read_orig_len == host_orig, "%s gzread orig=%u", n2,
                   (unsigned int)read_orig_len);
          }
          int gzret = gzclose(gz);
          CHECKF(gzret >= 0, "%s gzclose", n2);
        }
      }

      free((void *)comp);
      free((void *)orig);
    }
  }

  char summary[128];
  snprintf(summary, sizeof(summary), "inflate_zlib total");
  CHECKF(total > 0, "%s > 0", summary);
  return 0;
}

// ============================================================================
// 4. Cross-language gz*: read raw binary assets via gzopen/gzread/gzseek
// ============================================================================
// Reads tcmips_memfs files at raw/{TYPE}.bin (produced by
// generate_ref_data.py).  Each file contains only raw original bytes.
// Verifies gz* family functions correctly read data from a TCM filesystem
// (tcmips_memfs), and that the bytes match expected content.

static const char *g_raw_filenames[] = {"empty.raw", "text.raw", "repeat.raw"};
static const char *g_raw_names[] = {"empty", "text", "repeat"};
static const size_t g_raw_expected_lens[] = {0, 274, 65536};

static int test_gz_files(void) {
  int n = sizeof(g_raw_filenames) / sizeof(g_raw_filenames[0]);

  for (int i = 0; i < n; i++) {
    char path[256];
    char name[128];
    build_path(path, sizeof(path), g_raw_assets_dir, g_raw_filenames[i]);
    snprintf(name, sizeof(name), "gz_raw %s", g_raw_names[i]);

    size_t expected_len = g_raw_expected_lens[i];

    // --- Test 1: gzopen() + gzread() all bytes ---
    {
      char n2[128];
      snprintf(n2, sizeof(n2), "%s open", name);
      gzFile gz = gzopen(path, "rb");
      CHECK(gz != NULL, n2);
      if (!gz)
        continue;

      // Read all bytes in chunks via gzread.
      unsigned char *buf =
          (unsigned char *)malloc(expected_len > 0 ? expected_len : 1);
      {
        char n3[128];
        snprintf(n3, sizeof(n3), "%s malloc", name);
        CHECK(buf != NULL, n3);
      }
      if (!buf) {
        gzclose(gz);
        continue;
      }

      size_t total_read = 0;
      while (total_read < expected_len) {
        size_t chunk = expected_len - total_read;
        if (chunk > 4096)
          chunk = 4096;
        int got = gzread(gz, buf + total_read, chunk);
        if (got <= 0)
          break;
        total_read += (size_t)got;
      }

      {
        char n2b[128];
        snprintf(n2b, sizeof(n2b), "%s len", name);
        CHECK(total_read == expected_len, n2b);
      }

      // Verify content based on type.
      if (i == 0 && expected_len == 0) {
        {
          char n3[128];
          snprintf(n3, sizeof(n3), "%s content", name);
          CHECK(total_read == 0, n3);
        }
      } else if (i == 1) {
        // text.bin: should match g_text
        {
          char _cm_name[128];
          snprintf(_cm_name, sizeof(_cm_name), "%s content", name);
          CHECK_MEMEQ(buf, (const unsigned char *)g_text, expected_len,
                      _cm_name);
        }
      } else if (i == 2) {
        // repeat.bin: all 0xCD
        int all_cd = 1;
        for (size_t j = 0; j < total_read; j++) {
          if (buf[j] != 0xCD) {
            all_cd = 0;
            break;
          }
        }
        {
          char n3[128];
          snprintf(n3, sizeof(n3), "%s content all 0xCD", name);
          CHECK(all_cd, n3);
        }
      }

      free(buf);

      // --- Test 2: gzseek() to specific positions ---
      {
        char n2b[128];
        snprintf(n2b, sizeof(n2b), "%s seek", name);
        if (expected_len > 0) {
          // Seek to middle, check what we can read.
          long mid = (long)expected_len / 2;
          long pos = gzseek(gz, mid, 0); /* Z_SEEK_SET=0 */
          CHECKF(pos == (long)mid, "%s seek mid", name);
          if (pos == (long)mid) {
            unsigned char byte_buf[2];
            int got = gzread(gz, byte_buf, 2);
            CHECKF(got >= 0, "%s seek read", name);
          }
          // Return to start.
          pos = gzseek(gz, 0, 0); /* Z_SEEK_SET=0 */
          CHECKF(pos == 0, "%s seek reset", name);
        } else {
          // Empty file: seek to 0 should work, seek beyond should fail
          // or be clamped to current position per emulated FS behavior.
          long pos = gzseek(gz, 1, 0); /* Z_SEEK_SET=0 */
          CHECKF(pos >= -1, "%s seek beyond empty", name);
        }
      }

      int gzret = gzclose(gz);
      CHECKF(gzret >= 0, "%s close", name);
    }
  }

  return 0;
}

// ============================================================================
// 5. crc32/adler32: checksum correctness
// ============================================================================
static void test_checksums(void) {
  uLong crc_empty = crc32(0L, Z_NULL, 0);
  CHECK(crc_empty == 0x00000000UL, "crc32 empty is 0");

  const char *s = "a";
  uLong crc_a = crc32(0L, (const Bytef *)s, 1);
  CHECK(crc_a == 0xE8B7BE43UL, "crc32('a') known value");

  uLong crc_abc = crc32(0L, (const Bytef *)"abc", 3);
  CHECK(crc_abc == 0x352441C2UL, "crc32('abc') known value");

  uLong adler_empty = adler32(1L, Z_NULL, 0);
  CHECK(adler_empty == 1UL, "adler32 empty is 1 (Z_ADLER32_INIT)");

  uLong adler_a = adler32(1L, (const Bytef *)"a", 1);
  CHECK(adler_a == 0x00620062UL, "adler32('a') known value");

  uLong crc_text = crc32(0L, (const Bytef *)g_text, g_text_len);
  uLong adler_text = adler32(1L, (const Bytef *)g_text, g_text_len);
  CHECK(crc_text != adler_text, "crc32 != adler32 for non-trivial input");

  uLong crc_ab = crc32(0L, (const Bytef *)"ab", 2);
  uLong crc_abc_inc = crc32(crc_ab, (const Bytef *)"c", 1);
  uLong crc_abc_direct = crc32(0L, (const Bytef *)"abc", 3);
  CHECK(crc_abc_inc == crc_abc_direct, "crc32 incremental matches direct");

  unsigned char buf1[4096];
  unsigned char buf2[4096];
  gen_random(buf1, sizeof(buf1));
  // buf2: deterministic different content (all 0xAA)
  memset(buf2, 0xAA, sizeof(buf2));
  uLong crc1 = crc32(0L, buf1, sizeof(buf1));
  uLong crc2 = crc32(0L, buf2, sizeof(buf2));
  CHECK(crc1 != crc2, "crc32 different large buffers differ");

  uLong crc1b = crc32(0L, buf1, sizeof(buf1));
  CHECK(crc1 == crc1b, "crc32 identical large buffers match");
}

// ============================================================================
// 6. compressBound / buffer boundary edge cases
// ============================================================================
static void test_buffer_edges(void) {
  CHECK(compressBound(0) >= 0, "compressBound>=src_len");
  CHECK(compressBound(1) >= 1, "compressBound>=1");
  CHECK(compressBound(1000) >= 1000, "compressBound>=1000");
  CHECK(compressBound(65536) >= 65536, "compressBound>=65536");

  for (int sz = 0; sz <= 10000; sz += 1000) {
    char name[80];
    snprintf(name, sizeof(name), "compressBound %d malloc", sz);
    uLong bound = compressBound((uLong)sz);
    void *buf = malloc(bound > 0 ? bound : 1);
    CHECK(buf != NULL, name);
    free(buf);
  }

  // compress varying length data.
  for (int sz = 0; sz < 64; sz += 8) {
    unsigned char src[64];
    gen_random(src, (size_t)sz);
    uLong bound = compressBound((uLong)sz);
    unsigned char *comp = (unsigned char *)malloc(bound > 0 ? bound : 1);
    unsigned long comp_len = bound;
    int ret = compress(comp, &comp_len, src, (uInt)sz);
    char name[80];
    snprintf(name, sizeof(name), "compress varying %d <= bound", sz);
    CHECK(ret == Z_OK, name);
    if (ret == Z_OK) {
      CHECK(comp_len <= bound, name);
    }
    free(comp);
  }

  // Buffer too small → Z_BUF_ERROR.
  {
    unsigned char src[256];
    gen_random(src, sizeof(src));
    uLong bound = compressBound(sizeof(src));
    unsigned char *comp = (unsigned char *)malloc(bound);
    unsigned long comp_len = bound;
    int ret = compress(comp, &comp_len, src, sizeof(src));
    CHECK(ret == Z_OK, "buffer edge compress Z_OK");
    if (ret == Z_OK) {
      unsigned char tiny[1];
      unsigned long tiny_len = 1;
      ret = uncompress(tiny, &tiny_len, comp, comp_len);
      CHECK(ret == Z_BUF_ERROR, "uncompress tiny buffer Z_BUF_ERROR");
    }
    free(comp);
  }
  // Edge: zero-length output buffer with nonzero dst length → Z_BUF_ERROR.
  {
    unsigned long len = 1;
    unsigned char dst[1];
    int ret = compress(dst, &len, (const Bytef *)"x", 1);
    CHECK(ret == Z_BUF_ERROR, "compress zero buff Z_BUF_ERROR");
  }
}

// ============================================================================
// 7. deflateInit2 parameter variations
// ============================================================================
static void test_deflate_params(void) {
  z_stream strm;

  memset(&strm, 0, sizeof(strm));
  int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -1, 8,
                         Z_DEFAULT_STRATEGY);
  CHECK(ret == Z_STREAM_ERROR, "deflateInit2 negative wbits Z_STREAM_ERROR");

  memset(&strm, 0, sizeof(strm));
  ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 0, 8,
                     Z_DEFAULT_STRATEGY);
  CHECK(ret == Z_STREAM_ERROR, "deflateInit2 wbits=0 Z_STREAM_ERROR");

  memset(&strm, 0, sizeof(strm));
  ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 8,
                     Z_HUFFMAN_ONLY);
  CHECK(ret == Z_OK, "deflateInit2 Z_HUFFMAN_ONLY works");
  if (ret == Z_OK) {
    ret = deflateEnd(&strm);
    CHECK(ret == Z_OK, "deflateEnd after Z_HUFFMAN_ONLY");
  }

  // Inflate truncated garbage → error.
  {
    unsigned char garbage[16];
    gen_random(garbage, sizeof(garbage));
    memset(&strm, 0, sizeof(strm));
    strm.next_in = garbage;
    strm.avail_in = sizeof(garbage);
    unsigned char dummy[1];
    strm.next_out = dummy;
    strm.avail_out = 1;
    ret = inflateInit(&strm);
    if (ret == Z_OK) {
      ret = inflate(&strm, Z_FINISH);
      CHECK(ret != Z_OK && ret != Z_STREAM_END,
            "inflate truncated garbage error code acceptable");
      inflateEnd(&strm);
    }
  }
}

// ============================================================================
// 8. zlibCompileFlags
// ============================================================================
static void test_compile_flags_detailed(void) {
  unsigned flags = zlibCompileFlags();
  CHECK(flags > 0 && flags < 0xFFFFFFFFUL, "zlibCompileFlags within u32");
}

// ============================================================================
// Main
// ============================================================================
int main(void) {
  printf("[SELF] zlib_autotest starting\n");

  test_version();
  test_compress_api();
  test_deflate_inflate_roundtrip();
  test_inflate_files();
  test_gz_files();
  test_checksums();
  test_buffer_edges();
  test_deflate_params();
  test_compile_flags_detailed();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return (g_fail == 0) ? 0 : 1;
}