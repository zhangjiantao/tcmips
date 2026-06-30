// Created by zhangjiantao on 2026/4/30.
//

#ifndef TCMIPS_DEVKIT_SYSCALL_H
#define TCMIPS_DEVKIT_SYSCALL_H

#include <stdint.h>

// clang-format off
#define __NR_tcm_syscall_campaign               0
#define __NR_tcm_syscall_get_timestamp          1
#define __NR_tcm_syscall_seven_segment_display  2
#define __NR_tcm_syscall_console                3
#define __NR_tcm_syscall_read_keyboard          4
#define __NR_tcm_syscall_palette                5
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

uint32_t tcm_syscall_campaign_read_input(void);
void tcm_syscall_campaign_write_output(uint32_t a0, uint32_t a1);

/**
 * @brief Get Unix timestamp in seconds
 * @return Current Unix timestamp (seconds since 1970-01-01)
 */
uint32_t tcm_syscall_get_timestamp(void);

/**
 * @brief Get millisecond part of current Unix timestamp (modulo value)
 * @return Modulo millisecond part of current time (0 ~ 999)
 */
uint32_t tcm_syscall_get_timestamp_milli(void);

/**
 * @brief Get microsecond part of current Unix timestamp (modulo value)
 * @return Modulo microsecond part of current time (0 ~ 999999)
 */
uint32_t tcm_syscall_get_timestamp_micro(void);

/**
 * @brief Get nanosecond part of current Unix timestamp (modulo value)
 * @return Modulo nanosecond part of current time (0 ~ 999999999)
 */
uint32_t tcm_syscall_get_timestamp_nano(void);

/**
 * @brief Graphic pixel console operation
 * @param a0 Console command code
 * @param a1 Command parameter value
 */
void tcm_syscall_pixel_console(uint32_t a0, uint32_t a1);

/**
 * @brief ASCII character console operation
 * @param a0 Console command code
 * @param a1 Command parameter value
 */
void tcm_syscall_ascii_console(uint32_t a0, uint32_t a1);

/**
 * @brief Read keyboard key code
 * @return Key code (0 if no key pressed)
 */
uint32_t tcm_syscall_read_keyboard(void);

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

void tcm_syscall_palette_init(uint32_t a0, uint32_t a1);
void tcm_syscall_palette_upload(uint32_t a0, uint32_t a1);
void tcm_syscall_palette_set_offset(uint32_t a0);

#ifdef __cplusplus
}
#endif

#endif