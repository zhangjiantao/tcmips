//
// Created by zhangjiantao on 2026/5/9.
//

#ifndef TCMIPS_DEVKIT_KEYBOARD_H
#define TCMIPS_DEVKIT_KEYBOARD_H

#include <stdint.h>

// clang-format off
#define __TCM_KEY_CODE_BACKSPACE        4
#define __TCM_KEY_CODE_ENTER            5
#define __TCM_KEY_CODE_DEL              8
// clang-format on

/**
 * @brief Clear keyboard buffer and reset state
 */
void tcm_keyboard_clear(void);

/**
 * @brief Get raw keyboard scan code
 * @return Raw keyboard scan code (32-bit)
 */
uint32_t tcm_keyboard_get_code(void);

#endif // TCMIPS_DEVKIT_KEYBOARD_H
