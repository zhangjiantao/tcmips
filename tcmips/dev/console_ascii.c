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

#ifdef TCM_CONSOLE_ASCII_FONT

static bool mode_echo = true;
static bool mode_icanon = true;

static uint8_t FG332 = 0xff;
static uint8_t BG332 = 0;

static const uint32_t buf_base = TCM_VRAM_ASCII_ADDR;
static const uint32_t console_px_width = 640;

static const uint32_t height = 30;
static const uint32_t width = 80;
static const uint32_t buf_count = 3;
static const uint32_t buf_size = height * width * 8 * 16;
static const uint32_t buf_end = buf_base + buf_size * buf_count;
static const uint32_t row_size = width * 8 * 16;

static uint32_t curr_base = buf_base;
static uint32_t curr_pos = 0;

void tcm_text_console_init(void) {
  tcm_syscall_text_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_PIXEL_8);
  tcm_syscall_text_console(CONSOLE_CMD_SET_RESOLUTION,
                           (console_px_width >> 2) - 1);
  tcm_syscall_text_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);

  FG332 = 0xff;
  BG332 = 0;
  curr_base = buf_base;
  curr_pos = 0;
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

#include "font/font8x16.inc"

static void custom_write_char(uint8_t ch) {
  uint32_t cx = curr_pos % width;
  uint32_t cy = curr_pos / width;

  uint32_t base_px = cx * 8;
  uint32_t base_py = cy * 16;

  const uint8_t *glyph = (const uint8_t *)font8x16 + ch * 16;

  for (int r = 0; r < 16; r++) {
    uint8_t *fb_line =
        (uint8_t *)curr_base + (base_py + r) * console_px_width + base_px;
    unsigned char bits = glyph[r];
    for (int b = 0; b < 8; b++)
      fb_line[b] = (bits & (0x80 >> b)) ? FG332 : BG332;
  }
}

/* The text VRAM is one contiguous region able to hold `buf_count` screens.
 * `curr_base` is an ABSOLUTE vram address: the start of the active (visible)
 * window. `curr_pos` is a RELATIVE offset in character cells, counted from the
 * current `curr_base`. Scrolling never moves data — it only advances
 * `curr_base`.
 *
 * `auto_scroll` mixes the two coordinate systems on purpose:
 *   - the first `if` uses the RELATIVE `curr_pos` to decide whether one more
 *     row of text overflowed the visible window (then slide the window up one
 *     `row_size` line);
 *   - the second `if` uses the ABSOLUTE `curr_base` to decide whether the
 *     sliding window is about to run past the end of the contiguous VRAM, in
 *     which case `reset()` wipes the WHOLE region and rewinds to `buf_base`.
 */
static void auto_scroll(void) {
  if (curr_pos >= width * height) {
    curr_base += row_size;
    curr_pos -= width;
  }
  if (curr_base + buf_size >= buf_end)
    tcm_text_console_reset();
  else
    tcm_syscall_text_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);
}

/* Clear == scroll the occupied content out of the window so that the first row
 * is a fresh (empty) one, then let `auto_scroll` converge (wipe via `reset()`
 * once the window would overrun the VRAM).
 *
 * With `curr_pos = R * width + C` (R = row within the window, C = column,
 * C == curr_pos % width), the advance amount expands to:
 *
 *     (curr_pos + width - (curr_pos % width)) * 8 * 16
 *   = (R*width + C + width - C) * 128
 *   = (R + 1) * width * 128
 *   = (R + 1) * row_size
 *
 * i.e. it pushes the occupied R+1 lines (up to and including the one under the
 * cursor) out of the window, leaving the cursor at the start of the next,
 * still-empty line. This is equivalent to the no-font variant's
 * `curr_base = curr_pos + row_size - ((curr_pos - curr_base) % row_size)`.
 *
 * `curr_pos = 0` (relative) makes `auto_scroll`'s first `if` (~overflow one
 * line) a no-op here — correct, since clearing does not need an extra
 * one-line scroll; the advance is already expressed by the `curr_base`
 * increment, and the absolute-address convergence in `auto_scroll` still runs.
 */
void tcm_text_console_clear(void) {
  curr_base = curr_base + (curr_pos + width - (curr_pos % width)) * 8 * 16;
  curr_pos = 0;
  auto_scroll();
}

void tcm_text_console_write_char(char c) {
  switch (c) {
  case '\r':
    curr_pos = curr_pos - (curr_pos % width);
    break;
  case '\n':
    curr_pos = curr_pos + width - (curr_pos % width);
    break;
  case '\t':
    // todo: write space
    curr_pos = (curr_pos + 3) & (~3);
    break;
  case '\b': {
    if (curr_pos > 0) {
      /* erase the blinking cursor at the current cell, then step back and erase
       * the previous glyph */
      custom_write_char(0);
      curr_pos--;
      custom_write_char(0);
    }
  } break;
  default: {
    custom_write_char(c);
    curr_pos++;
  } break;
  }
  auto_scroll();
}

static void update_cursor(void) {
  uint8_t val = 0xdb;
  if (tcm_syscall_get_timestamp() % 2 == 0)
    val = 0;
  custom_write_char(val);
}

static void clear_cursor(void) { custom_write_char(0); }

uint32_t tcm_text_console_write_string(const char *str) {
  const char *p = str;
  while (*p)
    tcm_text_console_write_char(*p++);
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
#endif // TCM_CONSOLE_ASCII_FONT
