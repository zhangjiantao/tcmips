//
// Created by zhangjiantao on 2026/5/1.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#ifdef TCM_CONSOLE_UNICODE_FONT

static bool mode_echo = true;
static bool mode_icanon = true;

static uint8_t FG332 = 0xff;
static uint8_t BG332 = 0;

static const uint32_t buf_base = TCM_VRAM_ASCII_ADDR;
static const uint32_t console_px_width = 640;

static const uint32_t height = 24;
static const uint32_t row_height = 20;
static const uint32_t width = 80;
static const uint32_t buf_count = 3;
static const uint32_t buf_size = height * width * 8 * row_height;
static const uint32_t buf_end = buf_base + buf_size * buf_count;
static const uint32_t row_size = width * 8 * row_height;

static uint32_t curr_base = buf_base;
static uint32_t curr_pos = 0;

/* ── Unicode (UTF-8) font bundle state ──────────────────────────────
 * Lazy-loaded once from the TCM_CONSOLE_FONT_DEFAULT asset via memfs.
 * The on-disk asset is zlib-compressed (see tools/font/bdf2unifont_bin.py);
 * it is decompressed on load into the bundle layout documented there.
 */

#include <tcm_memfs.h>

static uint8_t *s_uni_buf = NULL;   /* full bundle, malloc'd once       */
static uint32_t s_uni_count = 0;    /* glyph entry count                */
static const uint8_t *s_uni_index;  /* index[] base                     */
static const uint8_t *s_uni_bitmap; /* bitmap[] base                    */

static void uni_load_font(void);

void tcm_text_console_init(void) {
  tcm_syscall_text_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_PIXEL_8);
  tcm_syscall_text_console(CONSOLE_CMD_SET_RESOLUTION,
                           (console_px_width >> 2) - 1);
  tcm_syscall_text_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);

  FG332 = 0xff;
  BG332 = 0;
  curr_base = buf_base;
  curr_pos = 0;

  uni_load_font();
}

void tcm_text_console_mode(bool echo, bool icanon) {
  mode_echo = echo;
  mode_icanon = icanon;
}

void tcm_text_console_reset(void) {
  void *base = (void *)buf_base;
  memset(base, 0, buf_end - buf_base);
  tcm_text_console_init();
}

void tcm_text_console_set_color(uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br,
                                uint8_t bg, uint8_t bb) {
  uint8_t r_3bit, g_3bit, b_2bit;
  r_3bit = (fr >> 5) & 0x07;
  g_3bit = (fg >> 5) & 0x07;
  b_2bit = (fb >> 6) & 0x03;
  FG332 = (r_3bit << 5) | (g_3bit << 2) | b_2bit;
  r_3bit = (br >> 5) & 0x07;
  g_3bit = (bg >> 5) & 0x07;
  b_2bit = (bb >> 6) & 0x03;
  BG332 = (r_3bit << 5) | (g_3bit << 2) | b_2bit;
}

/* ── Unicode renderer: UTF-8 decode + unifont mixed-width glyphs ─────
 * Glyphs are laid out as 16 px tall cells; narrow glyphs are 8 px wide and wide
 * glyphs 16 px wide, so a wide glyph occupies exactly two narrow columns and
 * the whole line stays aligned on the 8 px grid. */

/* Filesystem asset wrapper: `magic` + lengths + compression flag, followed by
 * either the raw bundle or its zlib deflate stream (see bdf2unifont_bin.py). */
#define UNI_WRAP_MAGIC 0x5A494E55u /* "UNIZ" (bytes 55 4E 49 5A) LE */
#define UNI_WRAP_FLAG_COMPRESSED 0x01

/* Load the unifont bundle stored inside the asset buffer `src` (size
 * `src_len`: asset header + payload). On success returns a pointer to the
 * bundle start in *`out_buf` and its size in *`out_len`. *`out_storage_owned`
 * is set to 1 when the bundle is a fresh allocation the caller must release
 * (compressed path), or 0 when the bundle aliases the caller's `src` buffer
 * (uncompressed path, where `src` is typically a persistent mapped asset).
 * Returns 0 on success or a Z_* error code. */
static int uni_decompress(const uint8_t *src, uint32_t src_len,
                          uint8_t **out_buf, uint32_t *out_len,
                          bool *out_storage_owned) {
  struct {
    uint32_t magic;
    uint32_t original_len;
    uint32_t compressed_len;
    uint32_t flags;
  } w;
  if (src_len < sizeof(w))
    return Z_DATA_ERROR;

  memcpy(&w.magic, src + 0, 4);
  memcpy(&w.original_len, src + 4, 4);
  memcpy(&w.compressed_len, src + 8, 4);
  memcpy(&w.flags, src + 12, 4);

  if (w.magic != UNI_WRAP_MAGIC)
    return Z_DATA_ERROR;
  if (w.original_len < 16) /* need at least the UNIF bundle header */
    return Z_DATA_ERROR;

  const uint8_t *payload = src + sizeof(w);
  if (w.flags & UNI_WRAP_FLAG_COMPRESSED) {
    if (w.compressed_len != src_len - sizeof(w))
      return Z_DATA_ERROR;
    uint8_t *out = (uint8_t *)malloc(w.original_len);
    if (!out)
      return Z_MEM_ERROR;
    uLong out_len_est = w.original_len;
    int ret = uncompress(out, &out_len_est, payload, w.compressed_len);
    if (ret != Z_OK) {
      free(out);
      return ret;
    }
    if ((uint32_t)out_len_est != w.original_len) {
      free(out);
      return Z_DATA_ERROR;
    }
    *out_buf = out;
    *out_len = (uint32_t)out_len_est;
    *out_storage_owned = true; /* caller owns this allocated bundle */
  } else {
    /* Uncompressed payload aliases the caller's buffer (e.g. a mapped asset):
     * hand it back without copying; the caller keeps `src`. */
    if (w.compressed_len != 0 || src_len != sizeof(w) + w.original_len)
      return Z_DATA_ERROR;
    *out_buf = (uint8_t *)payload;
    *out_len = w.original_len;
    *out_storage_owned = false; /* bundle shares `src` storage */
  }
  return Z_OK;
}

static void uni_load_font(void) {
  if (s_uni_buf)
    return;

  size_t fsize = 0;
  const uint8_t *img =
      (const uint8_t *)memfs_map(TCM_CONSOLE_FONT_DEFAULT, &fsize);
  if (!img || fsize == 0)
    return; /* no packed font asset mapped */

  uint8_t *bundle = NULL;
  uint32_t bundle_len = 0;
  bool bundle_owned = false;
  int ret =
      uni_decompress(img, (uint32_t)fsize, &bundle, &bundle_len, &bundle_owned);
  if (ret != Z_OK)
    return;

  /* Valid uncompressed bundle must start with UNIF magic (0x46494E55 LE). */
  if (bundle_len < 16 || bundle[0] != 'U' || bundle[1] != 'N' ||
      bundle[2] != 'I' || bundle[3] != 'F') {
    if (bundle_owned)
      free(bundle); /* compressed bundle is a fresh allocation; uncompressed
                       one aliases the mapped asset and needs no release */
    return;
  }

  /* header: magic(4) count(4) index_ofs(4) bitmap_ofs(4) */
  uint32_t count, index_ofs, bitmap_ofs;
  memcpy(&count, bundle + 4, 4);
  memcpy(&index_ofs, bundle + 8, 4);
  memcpy(&bitmap_ofs, bundle + 12, 4);

  s_uni_buf = bundle;
  s_uni_count = count;
  s_uni_index = bundle + index_ofs;
  s_uni_bitmap = bundle + bitmap_ofs;
}

/* Binary-search the sorted codepoint index. Returns the glyph index (>=0) or
 * -1. */
static int32_t uni_find(uint32_t cp) {
  uint32_t lo = 0, hi = s_uni_count;
  while (lo < hi) {
    uint32_t mid = (lo + hi) >> 1;
    uint32_t cur;
    memcpy(&cur, s_uni_index + mid * 8, 4);
    if (cur == cp)
      return (int32_t)mid;
    if (cur < cp)
      lo = mid + 1;
    else
      hi = mid;
  }
  return -1;
}

/* If a glyph of width w doesn't fit on the current line, advance cursor to
 * the start of the next line before drawing. */
static void uni_wrap(uint32_t w) {
  uint32_t col = curr_pos % console_px_width;
  if (col + w > console_px_width)
    curr_pos = curr_pos - col + console_px_width;
}

static void uni_write_glyph(uint32_t idx) {
  uint16_t w, h;
  memcpy(&w, s_uni_index + idx * 8 + 4, 2);
  memcpy(&h, s_uni_index + idx * 8 + 6, 2);
  if (h != 16 || (w != 8 && w != 16))
    return;

  const uint8_t *bmp = s_uni_bitmap + idx * 32;
  uint32_t col = curr_pos % console_px_width;
  uint32_t row = curr_pos / console_px_width;
  uint32_t base_px = col;
  uint32_t base_py = row * row_height;

  for (int r = 0; r < 16; r++) {
    uint16_t bits = (uint16_t)((bmp[r * 2] << 8) | bmp[r * 2 + 1]);
    for (int b = 0; b < w; b++) {
      /* unifont stores narrow (8px) glyph data in low byte (bits 7..0),
       * wide (16px) glyph uses high byte for left half (bits 15..8) */
      uint16_t mask = (uint16_t)((w == 8 ? 0x0080 : 0x8000) >> b);
      uint8_t *fb =
          (uint8_t *)curr_base + (base_py + r) * console_px_width + base_px + b;
      *fb = (bits & mask) ? FG332 : BG332;
    }
  }
}

/* Render one decoded codepoint (already have font loaded). */
static void uni_write_codepoint(uint32_t cp) {
  int32_t idx = uni_find(cp);
  if (idx < 0) {
    /* missing glyph: render U+2612 (☒ BALLOT BOX WITH X) as indicator */
    int32_t miss_idx = uni_find(0x2612);
    if (miss_idx >= 0) {
      uint16_t w;
      memcpy(&w, s_uni_index + miss_idx * 8 + 4, 2);
      uint32_t gw = w ? w : 8; /* width actually drawn */
      uni_wrap(gw);
      uni_write_glyph(miss_idx);
      curr_pos += gw;
    } else {
      uni_wrap(8);
      curr_pos += 8;
    }
    return;
  }
  uint16_t w;
  memcpy(&w, s_uni_index + idx * 8 + 4, 2);
  uint32_t gw = w ? w : 8; /* width actually drawn */
  uni_wrap(gw);
  uni_write_glyph(idx);
  curr_pos += gw;
}

/* UTF-8 sequence length (in bytes) for a leading byte. Returns 1 for ASCII
 * or an invalid/continuation leading byte. */
static int uni_utf8_len(uint8_t lead) {
  if ((lead & 0xF8) == 0xF0)
    return 4; /* 4-byte sequence */
  if ((lead & 0xF0) == 0xE0)
    return 3; /* 3-byte sequence */
  if ((lead & 0xE0) == 0xC0)
    return 2; /* 2-byte sequence */
  return 1;   /* ASCII or invalid leading byte */
}

/* Decode one UTF-8 codepoint from `str` (advancing `*s`). Returns the codepoint
 * or 0 for ASCII control handling; `*consumed` is the number of bytes eaten. */
static uint32_t uni_decode(const char **s, int *consumed) {
  const uint8_t *p = (const uint8_t *)*s;
  uint8_t c0 = *p;
  if (c0 < 0x80) {
    *consumed = 1;
    *s += 1;
    return c0;
  }
  int len = uni_utf8_len(c0);
  if (len == 1) { /* invalid leading byte */
    *consumed = 1;
    *s += 1;
    return 0xFFFD;
  }
  uint32_t cp;
  uint32_t min_cp; /* shortest sequence for this length: rejects overlong */
  if (len == 2) {
    cp = c0 & 0x1F;
    min_cp = 0x80;
  } else if (len == 3) {
    cp = c0 & 0x0F;
    min_cp = 0x800;
  } else {
    cp = c0 & 0x07; /* len == 4 */
    min_cp = 0x10000;
  }
  for (int i = 1; i < len; i++) {
    if ((p[i] & 0xC0) != 0x80) {
      *consumed = i;
      *s += i;
      return 0xFFFD;
    }
    cp = (cp << 6) | (p[i] & 0x3F);
  }
  /* Reject overlong encodings, UTF-16 surrogates and out-of-range code points.
   */
  if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
    *consumed = len;
    *s += len;
    return 0xFFFD;
  }
  *consumed = len;
  *s += len;
  return cp;
}

#include "font/font8x16.inc"

/* Paint an 8px-wide ASCII glyph from font8x16 at the current pixel column.
 * curr_pos is a pixel-column index (NOT a character index) in this mode. */
static void uni_write_ascii(uint8_t ch) {
#define USE_VGA_ASCII_FONT
#ifdef USE_VGA_ASCII_FONT
  uint32_t col = curr_pos % console_px_width;
  uint32_t row = curr_pos / console_px_width;
  uint32_t base_px = col;
  uint32_t base_py = row * row_height;

  const uint8_t *glyph = (const uint8_t *)font8x16 + ch * 16;

  for (int r = 0; r < 16; r++) {
    uint8_t *fb_line =
        (uint8_t *)curr_base + (base_py + r) * console_px_width + base_px;
    unsigned char bits = glyph[r];
    for (int b = 0; b < 8; b++)
      fb_line[b] = (bits & (0x80 >> b)) ? FG332 : BG332;
  }
#else
  uni_write_glyph(ch);
#endif
}

/* Scroll logic in pixel-column coordinates, mirroring console_ascii.c.
 * `curr_base` is an ABSOLUTE vram address (start of the active window);
 * `curr_pos` is a RELATIVE offset in pixel columns against `curr_base`:
 * row = curr_pos / console_px_width, col = curr_pos % console_px_width.
 */
static void auto_scroll(void) {
  uint32_t rows = curr_pos / console_px_width;
  if (rows >= height) {
    /* scroll up one 20 px line */
    uint32_t scroll_px = console_px_width; /* one full row of pixel columns */
    curr_base += row_size;
    curr_pos -= scroll_px;
  }
  if (curr_base + buf_size >= buf_end)
    tcm_text_console_reset();
  else
    tcm_syscall_text_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);
}

static void uni_mb_reset(void);

/* Clear == scroll the occupied content out of the window so the first row is a
 * fresh (empty) one, then let `auto_scroll` converge (wipe via `reset()` once
 * the window would overrun the contiguous VRAM). Same model as console_ascii.c.
 *
 * `curr_pos` is a RELATIVE offset in pixel columns against `curr_base`
 * (row = curr_pos / console_px_width). R = row under the cursor; pushing the
 * occupied R+1 rows out advances `curr_base` by (R + 1) * row_size bytes.
 */
void tcm_text_console_clear(void) {
  uni_mb_reset();
  curr_base += (curr_pos / console_px_width + 1) * row_size;
  curr_pos = 0;
  auto_scroll();
}

/* Paint/erase an 8px-wide, 16px-tall cursor block at the current pixel column.
 */
static void uni_cursor_block(uint8_t color) {
  uint32_t col = curr_pos % console_px_width;
  uint32_t row = curr_pos / console_px_width;
  uint32_t base_px = col;
  uint32_t base_py = row * row_height;
  for (int r = 0; r < 16; r++) {
    uint8_t *fb =
        (uint8_t *)curr_base + (base_py + r) * console_px_width + base_px;
    for (int b = 0; b < 8; b++)
      fb[b] = color;
  }
}

/* UTF-8 multi-byte byte buffer: enables tcm_text_console_write_char to
 * handle byte-by-byte input (e.g. from semihost stdout) by reconstructing
 * complete UTF-8 codepoints before rendering. */
static uint8_t uni_mb_pending[4];
static int uni_mb_count = 0;

static void uni_mb_reset(void) { uni_mb_count = 0; }

void tcm_text_console_write_char(char c) {
  switch (c) {
  case '\r':
    uni_mb_reset();
    curr_pos = curr_pos - (curr_pos % console_px_width);
    break;
  case '\n':
    uni_mb_reset();
    curr_pos = curr_pos + console_px_width - (curr_pos % console_px_width);
    break;
  case '\t':
    uni_mb_reset();
    /* tab to next 4-column (32 px) stop */
    curr_pos = ((curr_pos / 32) + 1) * 32;
    break;
  case '\b': {
    uni_mb_reset();
    if (curr_pos >= 8) {
      /* erase the blinking cursor at the current cell, then step back one
       * narrow cell and erase the previous glyph */
      uni_cursor_block(BG332);
      curr_pos -= 8;
      uni_cursor_block(BG332);
    }
  } break;
  default:
    if ((uint8_t)c < 0x80) {
      /* ASCII: flush pending UTF-8 bytes (shouldn't happen, but be safe),
       * then render immediately. */
      uni_mb_reset();
      /* ASCII: use 8px font8x16 path (mixed-width display) */
      uni_wrap(8);
      uni_write_ascii((uint8_t)c);
      curr_pos += 8;
    } else {
      /* Multi-byte UTF-8: Buffer until we have a complete sequence. */
      uni_mb_pending[uni_mb_count++] = (uint8_t)c;
      int expected = uni_utf8_len(uni_mb_pending[0]);
      if (uni_mb_count >= expected) {
        /* Attempt decode */
        char tmp[5];
        memcpy(tmp, uni_mb_pending, expected);
        tmp[expected] = '\0';
        int consumed = 0;
        const char *p = tmp;
        uint32_t cp = uni_decode(&p, &consumed);
        if (cp != 0xFFFD) {
          /* Valid UTF-8 sequence */
          if (cp < 0x80) {
            uni_wrap(8);
            uni_write_ascii((uint8_t)cp);
            curr_pos += 8;
          } else {
            if (s_uni_buf)
              uni_write_codepoint(cp);
          }
        }
        /* Invalid or incomplete — discard */
        uni_mb_count = 0;
      }
    }
    break;
  }
  auto_scroll();
}

static void update_cursor(void) {
  /* blink on odd timestamps (foreground) and off on even (background), matching
   * the ASCII console's cursor blink behaviour */
  uni_cursor_block(tcm_syscall_get_timestamp() % 2 == 0 ? BG332 : FG332);
}

static void clear_cursor(void) { uni_cursor_block(BG332); }

uint32_t tcm_text_console_write_string(const char *str) {
  const char *p = str;
  while (*p) {
    int consumed = 0;
    uint32_t cp = uni_decode(&p, &consumed);
    if (cp < 0x80) {
      /* ASCII fast path preserves control-char semantics (\r \n \t \b) */
      tcm_text_console_write_char((char)cp);
    } else {
      if (s_uni_buf)
        uni_write_codepoint(cp);
      auto_scroll();
    }
  }
  return (uint32_t)(p - str);
}

#define is_printable(c) ((c) >= 32 && (c) <= 126)

char tcm_text_console_read_char(void) {
  while (true) {
    update_cursor();
    uint32_t code = tcm_keyboard_get_code();
    if (!code)
      continue;

    if (code == TCM_K_ENTER) {
      clear_cursor();
      if (mode_echo)
        tcm_text_console_write_char('\n');
      return '\n';
    }

    if (code == TCM_K_BACKSPACE) {
      clear_cursor();
      return '\b';
    }

    if (!is_printable(code))
      continue;

    char c = (char)code;
    if (mode_echo)
      tcm_text_console_write_char(c);
    return c;
  }
}

uint32_t tcm_text_console_read_string(char *buffer, uint32_t length) {
  uint32_t size = 0;
  if (length == 0)
    return 0;

  if (length == 1) {
    buffer[0] = '\0';
    return 0;
  }

  while (true) {
    update_cursor();
    uint32_t code = tcm_keyboard_get_code();

    if (!code)
      continue;

    if ((code == TCM_K_BACKSPACE || code == TCM_K_DEL)) {
      if (size) {
        buffer[--size] = '\0';
        tcm_text_console_write_char('\b');
      } else {
        // todo
      }
      continue;
    }

    if (code == TCM_K_ENTER) {
      buffer[size] = '\0';
      clear_cursor();
      if (mode_echo)
        tcm_text_console_write_char('\n');
      return size;
    }

    if (!is_printable(code))
      continue;

    char c = (char)code;
    if (mode_echo)
      tcm_text_console_write_char(c);
    buffer[size] = c;
    size++;

    if (size == length - 1) {
      clear_cursor();
      if (mode_echo)
        tcm_text_console_write_char('\n');
      buffer[size] = '\0';
      return size;
    }
  }
}
#endif // TCM_CONSOLE_UNICODE_FONT
