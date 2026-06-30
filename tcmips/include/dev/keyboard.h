//
// Created by zhangjiantao on 2026/5/9.
//

#ifndef TCMIPS_DEVKIT_KEYBOARD_H
#define TCMIPS_DEVKIT_KEYBOARD_H

#include <stdint.h>

// clang-format off
#define __TCM_KEY_CODE_UP               1
#define __TCM_KEY_CODE_RIGHT            2
#define __TCM_KEY_CODE_DOWN             3
#define __TCM_KEY_CODE_LEFT             4

#define __TCM_KEY_CODE_TAB              9

#define __TCM_KEY_CODE_ENTER            10
#define __TCM_KEY_CODE_DEL              12
#define __TCM_KEY_CODE_BACKSPACE        13

#define __TCM_KEY_CODE_CTRL             15
#define __TCM_KEY_CODE_LSHIFT           16
#define __TCM_KEY_CODE_RSHIFT           20

// clang-format on

#define __TCM_KEYBOARD_EMPTY(x) (((x) & 0xff) == 0)
#define __TCM_KEYBOARD_PRESS(x) ((((x) >> 8) & 0xff) == 1)
#define __TCM_KEYBOARD_KCODE(x) ((x) >> 16)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Clear keyboard buffer and reset state
 */
void tcm_keyboard_clear(void);

/**
 * @brief Get raw keyboard scan code
 * @return Raw keyboard scan code (32-bit)
 */
uint32_t tcm_keyboard_get_code(void);

#ifdef __cplusplus
}
#endif

#endif // TCMIPS_DEVKIT_KEYBOARD_H
