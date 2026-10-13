//
// Created by zhangjiantao on 2026/5/9.
//

#ifndef TCMIPS_DEVKIT_KEYBOARD_H
#define TCMIPS_DEVKIT_KEYBOARD_H

#include <stdint.h>

// clang-format off

/**
 * @name Keyboard key‑code definitions
 * @brief Virtual key identifiers for TCMIPS keyboard driver
 * @{
 */
#define TCM_K_UP            1
#define TCM_K_RIGHT         2
#define TCM_K_DOWN          3
#define TCM_K_LEFT          4
#define TCM_K_PAGEUP        5
#define TCM_K_PAGEDOWN      6
#define TCM_K_HOME          7
#define TCM_K_END           8
#define TCM_K_TAB           9
#define TCM_K_ENTER         10
#define TCM_K_INSERT        11
#define TCM_K_DEL           12
#define TCM_K_BACKSPACE     13
#define TCM_K_ESC           14
#define TCM_K_LCTRL         15      // l-command
#define TCM_K_LSHIFT        16
#define TCM_K_LALT          17      // l-option
#define TCM_K_RCTRL         19      // r-command
#define TCM_K_RSHIFT        20
#define TCM_K_RALT          21      // r-option
#define TCM_K_CAPSLOCK      24

// Legacy aliases kept for source compatibility with older downstream programs
// that referenced the __TCM_KEY_CODE_* names. Prefer the TCM_K_* names above.
#define __TCM_KEY_CODE_UP           TCM_K_UP
#define __TCM_KEY_CODE_RIGHT        TCM_K_RIGHT
#define __TCM_KEY_CODE_DOWN         TCM_K_DOWN
#define __TCM_KEY_CODE_LEFT         TCM_K_LEFT
#define __TCM_KEY_CODE_PAGEUP       TCM_K_PAGEUP
#define __TCM_KEY_CODE_PAGEDOWN     TCM_K_PAGEDOWN
#define __TCM_KEY_CODE_HOME         TCM_K_HOME
#define __TCM_KEY_CODE_END          TCM_K_END
#define __TCM_KEY_CODE_TAB          TCM_K_TAB
#define __TCM_KEY_CODE_ENTER        TCM_K_ENTER
#define __TCM_KEY_CODE_INSERT       TCM_K_INSERT
#define __TCM_KEY_CODE_DEL          TCM_K_DEL
#define __TCM_KEY_CODE_BACKSPACE    TCM_K_BACKSPACE
#define __TCM_KEY_CODE_ESC          TCM_K_ESC
#define __TCM_KEY_CODE_LCTRL        TCM_K_LCTRL
#define __TCM_KEY_CODE_LSHIFT       TCM_K_LSHIFT
#define __TCM_KEY_CODE_LALT         TCM_K_LALT
#define __TCM_KEY_CODE_RCTRL        TCM_K_RCTRL
#define __TCM_KEY_CODE_RSHIFT       TCM_K_RSHIFT
#define __TCM_KEY_CODE_RALT         TCM_K_RALT
#define __TCM_KEY_CODE_CAPSLOCK     TCM_K_CAPSLOCK

#define __TCM_KEY_CODE_CTRL __TCM_KEY_CODE_LCTRL
/** @} */
// clang-format on

/**
 * @brief Keyboard event packet
 * @details Union for raw 32‑bit event access or decomposed bit‑field parsing
 */
typedef union {
  /**
   * @brief Raw 32‑bit keyboard event value, returned by
   * tcm_syscall_read_keyboard()
   */
  uint32_t raw_value;
  struct {
    /** @brief Non‑empty flag; non‑zero indicates valid event present */
    uint32_t not_empty : 8;
    /** @brief Key action flag: 1 = key‑down, 0 = key‑release */
    uint32_t is_press : 8;
    /** @brief Key identifier, mixed encoding: TCM_K_* control codes for
     *  non‑printing keys, otherwise the key's ASCII character code */
    uint32_t key_code : 16;
  } event;
} TcmKeyboardEvent;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Clear keyboard buffer and reset state
 */
void tcm_keyboard_clear(void);

/**
 * @brief Dequeue one pressed‑key ASCII code from keyboard queue
 *
 * Drains events from the keyboard until a key‑down arrives, returning its
 * ASCII code (control keys such as Shift are consumed to update internal state,
 * not returned). This is a stateful call: it updates the driver's internal
 * Shift state as it sees Shift press/release events, so the shifted mapping
 * returned for a later key reflects the most recently seen Shift.
 *
 * @return Shift‑processed ASCII character, or 0 when no key is available
 */
uint32_t tcm_keyboard_get_code(void);

#ifdef __cplusplus
}
#endif

#endif // TCMIPS_DEVKIT_KEYBOARD_H
