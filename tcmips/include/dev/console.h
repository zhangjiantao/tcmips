//
// Created by zhangjiantao on 2026/5/6.
//

#ifndef TCMIPS_DEVKIT_CONSOLE_H
#define TCMIPS_DEVKIT_CONSOLE_H

#include <tcm_config.h>

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CONSOLE_MODE_ASCII_8 = 0, // DO NOT USE THIS MODE!
  CONSOLE_MODE_ASCII_32 = 1,
  CONSOLE_MODE_PIXEL_8 = 2,
  CONSOLE_MODE_PIXEL_32 = 3,

  CONSOLE_MODE_UNINITIALIZED = -1,
} tcm_console_mode_t;

typedef enum {
  CONSOLE_CMD_SET_MODE = 0,
  CONSOLE_CMD_SET_DATA_OFFSET = 1,

  // CONSOLE_MODE_ASCII_8
  CONSOLE_CMD_SET_FG_COLOR = 2,
  CONSOLE_CMD_SET_BG_COLOR = 3,
  CONSOLE_CMD_SET_FONT = 4,

  // CONSOLE_MODE_PIXEL_8/32
  CONSOLE_CMD_SET_RESOLUTION = 2, // 4 x (n + 1) : 3 x (n + 1)
} tcm_console_cmd_t;

#ifdef __cplusplus
extern "C" {
#endif

void *tcm_pixel_console_init(tcm_console_mode_t mode, uint32_t resolution);

void tcm_pixel_console_clear(void);

void tcm_ascii_console_init(void);

void tcm_ascii_console_clear(void);

void tcm_ascii_console_reset(void);

void tcm_ascii_console_set_color(uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br,
                                 uint8_t bg, uint8_t bb);

/**
 * @brief Set text console mode
 * @param echo Enable/disable input echo
 * @param icanon Enable/disable canonical mode
 */
void tcm_ascii_console_mode(bool echo, bool icanon);

/**
 * @brief Write a single character to the text console
 * @param c Character to be displayed (supports ASCII, \r, \n, \t)
 */
void tcm_ascii_console_write_char(char c);

/**
 * @brief Write a null-terminated string to the text console
 * @param str Pointer to the null-terminated ASCII string to display
 * @return Actual number of characters write (excluding the null terminator)
 */
uint32_t tcm_ascii_console_write_string(const char *str);

/**
 * @brief Read a character from the text console
 * @return Character including printable characters or '\n'
 */
char tcm_ascii_console_read_char(void);

/**
 * @brief Read a string from the text console input
 * @param buffer Pointer to the storage buffer
 * @param length Maximum number of bytes to read (including null terminator)
 * @return Actual number of characters read (excluding the null terminator)
 */
uint32_t tcm_ascii_console_read_string(char *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif // TCMIPS_DEVKIT_CONSOLE_H
