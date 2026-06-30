//
// Created by zhangjiantao on 2026/5/1.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// pixel mode

static tcm_console_mode_t pixel_console_mode;
static uint32_t pixel_console_resolution;

void *tcm_pixel_console_init(tcm_console_mode_t mode, uint32_t resolution) {
  if (mode != CONSOLE_MODE_PIXEL_8 && mode != CONSOLE_MODE_PIXEL_32)
    return NULL;

  if ((resolution & 3) || resolution > 1024 || resolution < 4)
    return NULL;

  pixel_console_mode = mode;
  pixel_console_resolution = resolution;
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_MODE, mode);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_DATA_OFFSET, TCM_VRAM_PIXEL_ADDR);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_RESOLUTION,
                            (pixel_console_resolution >> 2) - 1);

  return (void *)TCM_VRAM_PIXEL_ADDR;
}

void tcm_pixel_console_clear(void) {
  if (pixel_console_mode == CONSOLE_MODE_UNINITIALIZED)
    return;

  memset((void *)TCM_VRAM_PIXEL_ADDR, 0,
         pixel_console_resolution * ((pixel_console_resolution >> 2) * 3) *
             (pixel_console_mode == CONSOLE_MODE_PIXEL_8 ? 1 : 4));
}

// ascii mode

static uint8_t FG332 = 0xff;
static uint8_t BG332 = 0;

const uint32_t buf_base = TCM_VRAM_ASCII_ADDR;

#ifdef TCM_CONSOLE_CUSTOM_FONT_8X16
const uint32_t height = 30;
const uint32_t width = 80;
const uint32_t buf_count = 3;
const uint32_t buf_size = height * width * 8 * 16;
const uint32_t buf_end = buf_base + buf_size * buf_count;
const uint32_t row_size = width * 8 * 16;

static uint32_t curr_base = buf_base;
static uint32_t curr_pos = 0;

#elif defined(TCM_CONSOLE_CUSTOM_FONT_16X32)

const uint32_t height = 15;
const uint32_t width = 40;
const uint32_t buf_count = 3;
const uint32_t buf_size = height * width * 16 * 32;
const uint32_t buf_end = buf_base + buf_size * buf_count;
const uint32_t row_size = width * 16 * 32;

static uint32_t curr_base = buf_base;
static uint32_t curr_pos = 0;

#else

const uint32_t height = 40;
const uint32_t width = 96;
const uint32_t buf_count = 8;
const uint32_t buf_size = height * width * 4;
const uint32_t buf_end = buf_base + buf_size * buf_count;
const uint32_t row_size = width * 4;

static uint32_t curr_base = buf_base;
static uint32_t curr_pos = buf_base;

#endif

static bool mode_echo = true;
static bool mode_icanon = true;

#define MAX_CANON 4096
#define MAX_CANON_MASK (MAX_CANON - 1)

// static uint8_t icanon_buffer[MAX_CANON];
// static volatile uint32_t icanon_read_pos = 0;
// static volatile uint32_t icanon_write_pos = 0;
//
// static inline bool icanon_buffer_put(uint8_t c) {
//   uint32_t next_write_pos = (icanon_write_pos + 1) & MAX_CANON_MASK;
//
//   if (next_write_pos == icanon_read_pos)
//     return false;
//
//   icanon_buffer[icanon_write_pos] = c;
//   icanon_write_pos = next_write_pos;
//   return true;
// }
//
// static inline bool icanon_buffer_get(uint8_t *c) {
//   if (icanon_read_pos == icanon_write_pos)
//     return false;
//
//   *c = icanon_buffer[icanon_read_pos];
//   icanon_read_pos = (icanon_read_pos + 1) & MAX_CANON_MASK;
//   return true;
// }

void tcm_ascii_console_init(void) {
#if defined(TCM_CONSOLE_CUSTOM_FONT_8X16) ||                                   \
    defined(TCM_CONSOLE_CUSTOM_FONT_16X32)
  tcm_syscall_ascii_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_PIXEL_8);
  tcm_syscall_ascii_console(CONSOLE_CMD_SET_RESOLUTION, (640 >> 2) - 1);
  tcm_syscall_ascii_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);

  FG332 = 0xff;
  BG332 = 0;
  curr_base = buf_base;
  curr_pos = 0;

#else
  tcm_syscall_ascii_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_ASCII_32);
  tcm_syscall_ascii_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);

  FG332 = 0xff;
  BG332 = 0;
  curr_base = buf_base;
  curr_pos = curr_base;

#endif

  // icanon_write_pos = 0;
  // icanon_read_pos = 0;
}

void tcm_ascii_console_mode(bool echo, bool icanon) {
  mode_echo = echo;
  mode_icanon = icanon;
  if (!icanon) {
    // todo: impl
    // icanon_read_pos = 0;
    // icanon_write_pos = 0;
  }
}

void tcm_ascii_console_reset(void) {
  void *base = (void *)buf_base;
  memset(base, 0, buf_end - buf_base);
  tcm_ascii_console_init();
}

void tcm_ascii_console_set_color(uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br,
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

#ifdef TCM_CONSOLE_CUSTOM_FONT_8X16

static void auto_scroll(void) {
  if (curr_pos > width * height) {
    curr_base += row_size;
    curr_pos -= width;
  }
  if (curr_base + buf_size >= buf_end)
    tcm_ascii_console_reset();
  else
    tcm_syscall_ascii_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);
}

void tcm_ascii_console_clear(void) {
  curr_base = curr_base + (curr_pos + width - (curr_pos % width)) * 8 * 16;
  curr_pos = 0;
  auto_scroll();
}

#include "font/font8x16.inc"

static void custom_write_char(uint8_t ch) {
  uint32_t cx = curr_pos % width;
  uint32_t cy = curr_pos / width;

  uint32_t base_px = cx * 8;
  uint32_t base_py = cy * 16;

  const uint8_t *glyph = (const uint8_t *)font8x16 + ch * 16;

  for (int r = 0; r < 16; r++) {
    uint8_t *fb_line = (uint8_t *)curr_base + (base_py + r) * 640 + base_px;
    unsigned char bits = glyph[r];
    for (int b = 0; b < 8; b++) {
      fb_line[b] = (bits & (0x80 >> b)) ? FG332 : BG332;
    }
  }
}

void tcm_ascii_console_write_char(char c) {
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
    custom_write_char(0);
    curr_pos--;
    custom_write_char(0);
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

#elif defined(TCM_CONSOLE_CUSTOM_FONT_16X32)

static void auto_scroll(void) {
  if (curr_pos > width * height) {
    curr_base += row_size;
    curr_pos -= width;
  }
  if (curr_base + buf_size >= buf_end)
    tcm_ascii_console_reset();
  else
    tcm_syscall_ascii_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);
}

void tcm_ascii_console_clear(void) {
  curr_base = curr_base + (curr_pos + width - (curr_pos % width)) * 16 * 32;
  curr_pos = 0;
  auto_scroll();
}

#include "font/font_16x32_ibmvga.inc"

static void custom_write_char(uint8_t ch) {
  uint32_t cx = curr_pos % width;
  uint32_t cy = curr_pos / width;

  uint32_t base_px = cx * 16;
  uint32_t base_py = cy * 32;

  const uint8_t *glyph = (const uint8_t *)font16x32 + ch * 64;

  for (int r = 0; r < 64; r += 2) {
    uint8_t *fb_line = (uint8_t *)curr_base + (base_py + r / 2) * 640 + base_px;
    unsigned char bits = glyph[r];
    for (int b = 0; b < 8; b++) {
      fb_line[b] = (bits & (0x80 >> b)) ? FG332 : BG332;
    }
    unsigned char bits2 = glyph[r + 1];
    for (int b = 0; b < 8; b++) {
      fb_line[b + 8] = (bits2 & (0x80 >> b)) ? FG332 : BG332;
    }
  }
}

void tcm_ascii_console_write_char(char c) {
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
    custom_write_char(0);
    curr_pos--;
    custom_write_char(0);
  } break;
  default: {
    custom_write_char(c);
    curr_pos++;
  } break;
  }
  auto_scroll();
}

static void update_cursor(void) {
  uint8_t val = 95;
  if (tcm_syscall_get_timestamp() % 2 == 0)
    val = 0;
  custom_write_char(val);
}

static void clear_cursor(void) { custom_write_char(0); }

#else

static void auto_scroll(void) {
  if (curr_pos - curr_base >= buf_size)
    curr_base += row_size;
  if (curr_base + buf_size >= buf_end)
    tcm_ascii_console_reset();
  else
    tcm_syscall_ascii_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);
}

void tcm_ascii_console_clear(void) {
  curr_base = curr_pos + row_size - ((curr_pos - curr_base) % row_size);
  curr_pos = curr_base;
  auto_scroll();
}

void tcm_ascii_console_write_char(char c) {
  switch (c) {
  case '\r':
    curr_pos = curr_pos - (curr_pos - curr_base) % row_size;
    break;
  case '\n':
    curr_pos = curr_pos + row_size - (curr_pos - curr_base) % row_size;
    break;
  case '\t':
    // todo: write space
    curr_pos = (curr_pos + 15) & (~15);
    break;
  case '\b': {
    *(uint32_t *)curr_pos = 0;
    curr_pos -= 4;
    *(uint32_t *)curr_pos = 0;
  } break;
  default: {
    uint32_t val = (BG332 << 16) | (FG332 << 8) | (uint8_t)c;
    *(uint32_t *)curr_pos = val;
    curr_pos += 4;
  } break;
  }
  auto_scroll();
}

static void update_cursor(void) {
  uint32_t val = 0x0000ffdb;
  if (tcm_syscall_get_timestamp() % 2 == 0)
    val = 0;
  *(uint32_t *)curr_pos = val;
}

static void clear_cursor(void) { *(uint32_t *)curr_pos = 0; }

#endif

uint32_t tcm_ascii_console_write_string(const char *str) {
  const char *p = str;
  while (*p)
    tcm_ascii_console_write_char(*p++);
  return p - str;
}

#define is_printable(c) ((c) >= 32 && (c) <= 126)

char tcm_ascii_console_read_char(void) {
  while (true) {
    update_cursor();
    uint32_t code = tcm_keyboard_get_code();
    if (!code)
      continue;

    if (code == __TCM_KEY_CODE_ENTER) {
      clear_cursor();
      if (mode_echo)
        tcm_ascii_console_write_char('\n');
      return '\n';
    }

    if (code == __TCM_KEY_CODE_BACKSPACE) {
      clear_cursor();
      return '\b';
    }

    if (!is_printable(code))
      continue;

    char c = (char)code;
    if (mode_echo)
      tcm_ascii_console_write_char(c);
    return c;
  }
}

uint32_t tcm_ascii_console_read_string(char *buffer, uint32_t length) {
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

    if ((code == __TCM_KEY_CODE_BACKSPACE || code == __TCM_KEY_CODE_DEL)) {
      if (size) {
        buffer[--size] = '\0';
        tcm_ascii_console_write_char('\b');
      } else {
        // todo
      }
      continue;
    }

    if (code == __TCM_KEY_CODE_ENTER) {
      buffer[size] = '\0';
      clear_cursor();
      if (mode_echo)
        tcm_ascii_console_write_char('\n');
      return size;
    }

    if (!is_printable(code))
      continue;

    char c = (char)code;
    if (mode_echo)
      tcm_ascii_console_write_char(c);
    buffer[size] = c;
    size++;

    if (size == length - 1) {
      clear_cursor();
      if (mode_echo)
        tcm_ascii_console_write_char('\n');
      buffer[size] = '\0';
      return size;
    }
  }
}
