//
// Created by zhangjiantao on 2026/5/6.
//

#ifndef TCMIPS_DEVKIT_CONSOLE_H
#define TCMIPS_DEVKIT_CONSOLE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Graphic Console VRAM base addresses
 */
typedef enum {
  GRAPHIC_0 = 0,
  GRAPHIC_1 = 480,
  GRAPHIC_2 = 960,
  GRAPHIC_3 = 1440
} tcm_console_vram_base_t;

/**
 * @brief Text Console VRAM base addresses
 */
#define TCM_TEXT_CONSOLE_VRAM 1920

/**
 * @brief Foreground color composition (R<<24 | G<<16 | B<<8)
 */
#define FG_COLOR(r, g, b)                                                      \
  ((uint32_t)((uint8_t)(r) << 24 | (uint8_t)(g) << 16 | (uint8_t)(b) << 8))

/**
 * @brief Background color composition (R<<16 | G<<8 | B<<0)
 */
#define BG_COLOR(r, g, b)                                                      \
  ((uint32_t)((uint8_t)(r) << 16 | (uint8_t)(g) << 8 | (uint8_t)(b) << 0))

/**
 * @brief Set console palette color
 * @param bg_r Red component of background color (0-255)
 * @param bg_g Green component of background color (0-255)
 * @param bg_b Blue component of background color (0-255)
 * @param fg_r Red component of foreground color (0-255)
 * @param fg_g Green component of foreground color (0-255)
 * @param fg_b Blue component of foreground color (0-255)
 * @note Calls tcm_syscall_console_color with encoded BG/FG color format
 */
void tcm_console_set_color(uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                           uint8_t fg_r, uint8_t fg_g, uint8_t fg_b);

/**
 * @brief Write to graphic console VRAM
 * @param vram_base Base address
 * @param offset Address offset
 * @param bitmask Control mask
 * @param value Data to write
 */
void tcm_graphic_console_write(tcm_console_vram_base_t vram_base,
                               uint32_t offset, uint8_t bitmask,
                               uint32_t value);

/**
 * @brief Clear graphic console VRAM with black color
 * @param vram_base Target console VRAM base address
 */
void tcm_graphic_console_clear(tcm_console_vram_base_t vram_base);

/**
 * @brief Initialize text console subsystem
 * @details Reset buffer pointers, clear display state, and set up console
 * default configuration
 */
void tcm_text_console_init(void);

/**
 * @brief Clear text console screen
 */
void tcm_text_console_clear(void);

/**
 * @brief Reset text console to default state
 */
void tcm_text_console_reset(void);

/**
 * @brief Set text console mode
 * @param echo Enable/disable input echo
 * @param icanon Enable/disable canonical mode
 */
void tcm_text_console_mode(bool echo, bool icanon);

/**
 * @brief Get text console VRAM base address
 * @return Text console VRAM base address
 */
uint32_t tcm_text_console_get_base(void);

/**
 * @brief Write a single character to the text console
 * @param c Character to be displayed (supports ASCII, \r, \n, \t)
 */
void tcm_text_console_write_char(char c);

/**
 * @brief Write a null-terminated string to the text console
 * @param str Pointer to the null-terminated ASCII string to display
 * @return Actual number of characters write (excluding the null terminator)
 */
uint32_t tcm_text_console_write_string(const char *str);

/**
 * @brief Read a character from the text console
 * @return Character including printable characters or '\n'
 */
char tcm_text_console_read_char(void);

/**
 * @brief Read a string from the text console input
 * @param buffer Pointer to the storage buffer
 * @param length Maximum number of bytes to read (including null terminator)
 * @return Actual number of characters read (excluding the null terminator)
 */
uint32_t tcm_text_console_read_string(char *buffer, uint32_t length);

#endif // TCMIPS_DEVKIT_CONSOLE_H
