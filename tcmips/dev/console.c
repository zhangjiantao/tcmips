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

// pixel mode

static tcm_console_mode_t pixel_console_mode;
static uint32_t pixel_console_width;

void *tcm_pixel_console_init(tcm_console_mode_t mode, uint32_t width) {
  if (mode != CONSOLE_MODE_PIXEL_8 && mode != CONSOLE_MODE_PIXEL_32)
    return NULL;

  if ((width & 3) || width > TCM_SCREEN_MAX_W || width < 4)
    return NULL;

  pixel_console_mode = mode;
  pixel_console_width = width;
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_MODE, mode);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_DATA_OFFSET, TCM_VRAM_PIXEL_ADDR);
  tcm_syscall_pixel_console(CONSOLE_CMD_SET_RESOLUTION,
                            (pixel_console_width >> 2) - 1);

  return (void *)TCM_VRAM_PIXEL_ADDR;
}

void tcm_pixel_console_clear(void) {
  if (pixel_console_mode == CONSOLE_MODE_UNINITIALIZED)
    return;

  memset((void *)TCM_VRAM_PIXEL_ADDR, 0,
         pixel_console_width * ((pixel_console_width >> 2) * 3) *
             (pixel_console_mode == CONSOLE_MODE_PIXEL_8 ? 1 : 4));
}

// text mode

#if !defined(TCM_CONSOLE_ASCII_FONT) && !defined(TCM_CONSOLE_UNICODE_FONT)

static bool mode_echo = true;
static bool mode_icanon = true;

static uint8_t FG332 = 0xff;
static uint8_t BG332 = 0;

static const uint32_t buf_base = TCM_VRAM_ASCII_ADDR;

static const uint32_t height = 40;
static const uint32_t width = 96;
static const uint32_t buf_count = 8;
static const uint32_t buf_size = height * width * 4;
static const uint32_t buf_end = buf_base + buf_size * buf_count;
static const uint32_t row_size = width * 4;

static uint32_t curr_base = buf_base;
static uint32_t curr_pos = buf_base;

void tcm_text_console_init(void) {
  tcm_syscall_text_console(CONSOLE_CMD_SET_MODE, CONSOLE_MODE_ASCII_32);
  tcm_syscall_text_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);

  FG332 = 0xff;
  BG332 = 0;
  curr_base = buf_base;
  curr_pos = curr_base;
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

static void auto_scroll(void) {
  if (curr_pos - curr_base >= buf_size)
    curr_base += row_size;
  if (curr_base + buf_size >= buf_end)
    tcm_text_console_reset();
  else
    tcm_syscall_text_console(CONSOLE_CMD_SET_DATA_OFFSET, curr_base);
}

void tcm_text_console_clear(void) {
  curr_base = curr_pos + row_size - ((curr_pos - curr_base) % row_size);
  curr_pos = curr_base;
  auto_scroll();
}

void tcm_text_console_write_char(char c) {
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
    if (curr_pos > curr_base) {
      *(uint32_t *)curr_pos = 0;
      curr_pos -= 4;
      *(uint32_t *)curr_pos = 0;
    }
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

#endif
