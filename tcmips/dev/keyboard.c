//
// Created by zhangjiantao on 2026/5/9.
//

#include <dev/keyboard.h>
#include <dev/syscall.h>

#include <stdint.h>

#define LOCAL_BUFFER_SIZE 16
#define BOARD_BUFFER_SIZE 16

static uint32_t _buffer[LOCAL_BUFFER_SIZE] = {0}; // NOLINT
static uint32_t _head = 0;                        // NOLINT
static uint32_t _tail = 0;                        // NOLINT

void tcm_keyboard_clear(void) {
  for (uint32_t i = 0; i < BOARD_BUFFER_SIZE; ++i)
    tcm_syscall_read_keyboard();
  _head = 0;
  _tail = 0;
  _buffer[_head] = 0;
}

static void flush_to_local_buffer() {
  for (uint32_t i = 0; i < BOARD_BUFFER_SIZE; ++i) {
    uint32_t key = tcm_syscall_read_keyboard();
    if (key) {
      _buffer[_head] = key;
      _head = (_head + 1) % LOCAL_BUFFER_SIZE;
      if (_head == _tail)
        _tail = (_tail + 1) % LOCAL_BUFFER_SIZE;
    }
  }
}

uint32_t tcm_keyboard_get_code(void) {
  while (_tail != _head) {
    uint32_t key = _buffer[_tail];
    _tail = (_tail + 1) % LOCAL_BUFFER_SIZE;
    if (key)
      return key;
  }

  flush_to_local_buffer();

  if (_head == _tail)
    return 0;
  uint32_t key = _buffer[_tail];
  _tail = (_tail + 1) % LOCAL_BUFFER_SIZE;
  return key;
}
