//
// Created by zhangjiantao on 2026/5/9.
//

#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <stdbool.h>
#include <stdint.h>

static bool shift = false;

void tcm_keyboard_clear(void) {
  bool not_empty = true;
  while (not_empty) {
    uint32_t k = tcm_syscall_read_keyboard();
    not_empty = !__TCM_KEYBOARD_EMPTY(k);
  }
}

static char shift_convert_ascii(char original_ascii) {
  if (!shift)
    return original_ascii;

  if (original_ascii >= 'a' && original_ascii <= 'z')
    return (char)(original_ascii - 32);

  switch (original_ascii) {
  case '0':
    return ')';
  case '1':
    return '!';
  case '2':
    return '@';
  case '3':
    return '#';
  case '4':
    return '$';
  case '5':
    return '%';
  case '6':
    return '^';
  case '7':
    return '&';
  case '8':
    return '*';
  case '9':
    return '(';

  case '-':
    return '_';
  case '=':
    return '+';
  case '[':
    return '{';
  case ']':
    return '}';
  case '\\':
    return '|';
  case ';':
    return ':';
  case '\'':
    return '"';
  case ',':
    return '<';
  case '.':
    return '>';
  case '/':
    return '?';
  case '`':
    return '~';
  default:
    break;
  }

  return original_ascii;
}

uint32_t tcm_keyboard_get_code(void) {
  bool is_empty = false;
  bool is_press = false;
  uint32_t code;
  while (!is_empty) {
    uint32_t k = tcm_syscall_read_keyboard();
    is_press = __TCM_KEYBOARD_PRESS(k);
    is_empty = __TCM_KEYBOARD_EMPTY(k);
    code = __TCM_KEYBOARD_KCODE(k);
    if (code == __TCM_KEY_CODE_LSHIFT || code == __TCM_KEY_CODE_RSHIFT)
      shift = is_press;

    if (is_press)
      return shift_convert_ascii((char)code);
  }

  return 0;
}
