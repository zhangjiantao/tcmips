//
// Created by zhangjiantao on 2026/5/1.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/syscall.h>
#include <tcm_config.h>

#include <stdbool.h>
#include <stdint.h>

//
// Video Memory Layout:
// +-----------+-----------+----------------+--------------+---------------+
// | Start Pos | End Pos   | Start Byte Addr| End Byte Addr| Usage         |
// +-----------+-----------+----------------+--------------+---------------+
// | 0         | 480       | 0x0000         | 0x3C00       | Graphics Mode |
// | 480       | 960       | 0x3C00         | 0x7800       | Graphics Mode |
// | 960       | 1440      | 0x7800         | 0xB400       | Graphics Mode |
// | 1440      | 1920      | 0xB400         | 0xF000       | Graphics Mode |
// | 1920      | 7680      | 0xF000         | 0x3C000      | Text Mode     |
// +-----------+-----------+----------------+--------------+---------------+
//
// All ranges are [start, end), end address is exclusive.
//

TCM_API void tcm_console_set_color(uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                                   uint8_t fg_r, uint8_t fg_g, uint8_t fg_b) {
  tcm_syscall_console_set_color(BG_COLOR(bg_r, bg_g, bg_b),
                                FG_COLOR(fg_r, fg_g, fg_b));
}

void tcm_graphic_console_write(tcm_console_vram_base_t vram_base,
                               uint32_t offset, uint8_t bitmask,
                               uint32_t value) {
  tcm_syscall_console_write((bitmask << 16) | ((uint32_t)vram_base + offset),
                            value);
}

void tcm_graphic_console_clear(tcm_console_vram_base_t vram_base) {
  tcm_syscall_console_set_color(0, 0xffffff00);
  for (int i = 0; i < 480; ++i)
    tcm_graphic_console_write(vram_base, i, 0xf, 0);
}

// NOLINTBEGIN
const uint32_t __buf_base = TCM_TEXT_CONSOLE_VRAM;
const uint32_t __buf_count = 12;
const uint32_t __buf_size = 480; // in 256-bit
const uint32_t __buf_end = 7680;
const uint32_t __row_size = 20; // in 256-bit

static uint32_t __curr_base = __buf_base;
static uint32_t __curr_pos = __buf_base;
static uint8_t __curr_pos_rem = 0;
static bool __mode_echo = true;
static bool __mode_icanon = true;
// NOLINTEND

#define MAX_CANON 4096
#define MAX_CANON_MASK (MAX_CANON - 1)

static uint8_t icanon_buffer[MAX_CANON];
static volatile uint32_t icanon_read_pos = 0;
static volatile uint32_t icanon_write_pos = 0;

static inline bool icanon_buffer_put(uint8_t c) {
  uint32_t next_write_pos = (icanon_write_pos + 1) & MAX_CANON_MASK;

  if (next_write_pos == icanon_read_pos)
    return false;

  icanon_buffer[icanon_write_pos] = c;
  icanon_write_pos = next_write_pos;
  return true;
}

static inline bool icanon_buffer_get(uint8_t *c) {
  if (icanon_read_pos == icanon_write_pos)
    return false;

  *c = icanon_buffer[icanon_read_pos];
  icanon_read_pos = (icanon_read_pos + 1) & MAX_CANON_MASK;
  return true;
}

TCM_API void tcm_text_console_init(void) {
  __curr_base = __buf_base;
  __curr_pos = __curr_base;
  __curr_pos_rem = 0;
  icanon_read_pos = 0;
  icanon_write_pos = 0;
  tcm_syscall_console_set_color(0, 0xffffff00);
  tcm_syscall_console_set_base(__curr_base);
}

TCM_API void tcm_text_console_mode(bool echo, bool icanon) {
  __mode_echo = echo;
  __mode_icanon = icanon;
  if (!icanon) {
    // todo: impl
    icanon_read_pos = 0;
    icanon_write_pos = 0;
  }
}

TCM_API void tcm_text_console_reset(void) {
  tcm_syscall_console_set_color(0, 0xffffff00);
  for (uint32_t i = __buf_base; i < __buf_end; i++)
    tcm_syscall_console_write(0xf0000 | i, 0);
  tcm_text_console_init();
}

static void __scroll(void) { // NOLINT
  if (__curr_pos - __curr_base >= __buf_size)
    __curr_base += __row_size;
  if (__curr_base + __buf_size >= __buf_end)
    tcm_text_console_reset();
  else
    tcm_syscall_console_set_base(__curr_base);
}

TCM_API void tcm_text_console_clear(void) {
  __curr_base = __curr_pos + __row_size - (__curr_pos % __row_size);
  __curr_pos = __curr_base;
  __curr_pos_rem = 0;
  __scroll();
}

TCM_API uint32_t tcm_text_console_get_base(void) { return __curr_base; }

void tcm_text_console_write_char(char c) {
  switch (c) {
  case '\r':
    __curr_pos = __curr_pos - __curr_pos % __row_size;
    __curr_pos_rem = 0;
    break;
  case '\n':
    __curr_pos = __curr_pos + __row_size - __curr_pos % __row_size;
    __curr_pos_rem = 0;
    break;
  case '\t':
    if (__curr_pos_rem) {
      uint32_t mask = (0xf0000 << __curr_pos_rem) & 0xf0000;
      tcm_syscall_console_write(mask | __curr_pos, 0x20202020);
    } else {
      tcm_syscall_console_write(0xf0000 | __curr_pos, 0x20202020);
    }
    __curr_pos++;
    __curr_pos_rem = 0;
    break;
  case '\b': {
    if (__curr_pos_rem) {
      __curr_pos_rem--;
      uint32_t mask = 1u << (__curr_pos_rem + 16);
      tcm_syscall_console_write(mask | __curr_pos, 0);
    } else {
      __curr_pos--;
      __curr_pos_rem = 3;
      uint32_t mask = 1u << (__curr_pos_rem + 16);
      tcm_syscall_console_write(mask | __curr_pos, 0);
    }
  } break;
  default: {
    uint32_t mask = 1u << (__curr_pos_rem + 16);
    tcm_syscall_console_write(mask | __curr_pos,
                              (uint32_t)c << (__curr_pos_rem * 8));
    __curr_pos_rem = (__curr_pos_rem + 1) % 4;
    if (__curr_pos_rem == 0)
      __curr_pos++;
  } break;
  }
  __scroll();
}

TCM_API uint32_t tcm_text_console_write_string(const char *str) {
  const char *p = str;
  while (*p)
    tcm_text_console_write_char(*p++);
  return p - str;
}

static void __update_cursor(void) { // NOLINT
  uint32_t mask = 1u << (__curr_pos_rem + 16);
  uint32_t c = 0xdbdbdbdb;
  if (tcm_syscall_get_timestamp() % 2 == 0)
    c = 0x20202020;
  tcm_syscall_console_write(mask | __curr_pos, c);
}

static void __clear_cursor(void) { // NOLINT
  uint32_t mask = 1u << (__curr_pos_rem + 16);
  tcm_syscall_console_write(mask | __curr_pos, 0x20202020);
}

#define __is_printable(c) ((c) >= 32 && (c) <= 126) // NOLINT

TCM_API char tcm_text_console_read_char(void) {
  while (true) {
    __update_cursor();
    uint32_t code = tcm_keyboard_get_code();
    if (!code || (code & 0x100) != 0)
      continue;

    if (code == __TCM_KEY_CODE_ENTER) {
      __clear_cursor();
      if (__mode_echo)
        tcm_text_console_write_char('\n');
      return '\n';
    }

    if (code == __TCM_KEY_CODE_BACKSPACE) {
      __clear_cursor();
      return '\b';
    }

    if (!__is_printable(code))
      continue;

    char c = (char)code;
    if (__mode_echo)
      tcm_text_console_write_char(c);
    return c;
  }
}

TCM_API uint32_t tcm_text_console_read_string(char *buffer, uint32_t length) {
  uint32_t size = 0;
  if (length == 0)
    return 0;

  if (length == 1) {
    buffer[0] = '\0';
    return 0;
  }

  while (true) {
    __update_cursor();
    uint32_t code = tcm_keyboard_get_code();

    if (!code || (code & 0x100) != 0)
      continue;

    if ((code == __TCM_KEY_CODE_BACKSPACE || code == __TCM_KEY_CODE_DEL)) {
      if (size) {
        buffer[--size] = '\0';
        uint32_t mask = 1u << (__curr_pos_rem + 16);
        tcm_syscall_console_write(mask | __curr_pos, 0x20202020);
        if (__curr_pos_rem) {
          __curr_pos_rem--;
        } else {
          __curr_pos--;
          __curr_pos_rem = 3;
        }
      } else {
        tcm_syscall_playsound(72, 0);
      }
      continue;
    }

    if (code == __TCM_KEY_CODE_ENTER) {
      buffer[size] = '\0';
      __clear_cursor();
      if (__mode_echo)
        tcm_text_console_write_char('\n');
      return size;
    }

    if (!__is_printable(code))
      continue;

    char c = (char)code;
    if (__mode_echo)
      tcm_text_console_write_char(c);
    buffer[size] = c;
    size++;

    if (size == length - 1) {
      __clear_cursor();
      if (__mode_echo)
        tcm_text_console_write_char('\n');
      buffer[size] = '\0';
      return size;
    }
  }
}
