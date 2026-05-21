// Created by zhangjiantao on 2026/4/30.
//

#ifndef TCMIPS_DEVKIT_SYSCALL_H
#define TCMIPS_DEVKIT_SYSCALL_H

#include <stdint.h>

// clang-format off
#define __NR_tcm_syscall_get_tickcount          0
#define __NR_tcm_syscall_get_timestamp          1

#define __NR_tcm_syscall_console                3
#define __NR_tcm_syscall_read_keyboard          4
#define __NR_tcm_syscall_playsound              5
#define __NR_tcm_syscall_file                   6
#define __NR_tcm_syscall_seven_segment_display  7
// clang-format on

/**
 * @brief Get system hardware tick count (monotonic increasing counter)
 * @return Current hardware tick count
 */
uint32_t tcm_syscall_get_tickcount(void);

/**
 * @brief Get Unix timestamp in seconds
 * @return Current Unix timestamp (seconds since 1970-01-01)
 */
uint32_t tcm_syscall_get_timestamp(void);

/**
 * @brief Get timestamp in milliseconds
 * @return Lower 32 bits of timestamp in milliseconds
 */
uint32_t tcm_syscall_get_timestamp_milli(void);

/**
 * @brief Get timestamp in microseconds
 * @return Lower 32 bits of timestamp in microseconds
 */
uint32_t tcm_syscall_get_timestamp_micro(void);

/**
 * @brief Sets the video memory mapping address of the console
 * @param a0 Console mapping position
 */
void tcm_syscall_console_set_base(uint32_t a0);

/**
 * @brief Set console palette color
 * @param a0 Background color, format 0x00RRGGBB
 * @param a1 Foreground color, format 0xRRGGBB00
 */
void tcm_syscall_console_set_color(uint32_t a0, uint32_t a1);

/**
 * @brief Writes data to video memory
 * @param a0 [15:0]  Video memory position to write to
 *           [19:16] Write enable mask
 * @param a1 Four characters to write, represented by four bytes from LSB to MSB
 */
void tcm_syscall_console_write(uint32_t a0, uint32_t a1);

/**
 * @brief Read keyboard key code
 * @return Key code (0 if no key pressed)
 */
uint32_t tcm_syscall_read_keyboard(void);

/**
 * @brief Set file read position (offset)
 * @param a0 Target file position (byte offset)
 */
void tcm_syscall_fileseek(uint32_t a0);

/**
 * @brief Read 4-byte data
 * @return Low 32 bits of read data
 */
uint32_t tcm_syscall_fileread_32(void);

/**
 * @brief Read 8-byte data (implicit memory write)
 * @details Reads 8-byte from current file position:
 *          - Low 32 bits returned by function.
 *          - HIGH 32 bits are IMPLICITLY written to memory at (position + 4).
 *          - File position automatically increments by 8 after read
 * @return Low 32 bits of read data
 */
uint32_t tcm_syscall_fileread_64(void);

/**
 * @brief Play sound with given note and pitch
 * @param a0 Note
 * @param a1 Pitch (signed)
 */
void tcm_syscall_playsound(uint32_t a0, uint32_t a1);

/**
 * @brief Reset audio controller and play sound
 * @param a0 Note
 * @param a1 Pitch (signed)
 */
void tcm_syscall_reset_and_playsound(uint32_t a0, uint32_t a1);

/**
 * @brief Stop all ongoing sound playback
 */
void tcm_syscall_stop_playsound(void);

/**
 * @brief Output data to the upper 7-segment display
 * @param a0 Left four digits
 * @param a1 Right four digits
 */
void tcm_syscall_seven_segment_display_upper(uint32_t a0, uint32_t a1);

/**
 * @brief Output data to the lower 7-segment display
 * @param a0 Left four digits
 * @param a1 Right four digits
 */
void tcm_syscall_seven_segment_display_lower(uint32_t a0, uint32_t a1);

#endif