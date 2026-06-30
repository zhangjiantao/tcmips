//
// Created by zhangjiantao on 2026/4/30.
//

#ifndef TCMIPS_DEVKIT_SEVEN_SEGMENT_DISPLAY_H
#define TCMIPS_DEVKIT_SEVEN_SEGMENT_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Output hexadecimal value to upper 7-segment display
 * @param hex32 32-bit hexadecimal value to display
 */
void tcm_seven_segment_display_upper_hexadecimal(uint32_t hex32);

/**
 * @brief Output hexadecimal value to lower 7-segment display
 * @param hex32 32-bit hexadecimal value to display
 */
void tcm_seven_segment_display_lower_hexadecimal(uint32_t hex32);

/**
 * @brief Output hexadecimal values to both upper and lower 7-segment displays
 * @param hi 32-bit value for upper display
 * @param lo 32-bit value for lower display
 */
void tcm_seven_segment_display_hexadecimal(uint32_t hi, uint32_t lo);

/**
 * @brief Output decimal value to upper 7-segment display
 * @param dec32 32-bit decimal value to display
 */
void tcm_seven_segment_display_upper_decimal(uint32_t dec32);

/**
 * @brief Output decimal value to lower 7-segment display
 * @param dec32 32-bit decimal value to display
 */
void tcm_seven_segment_display_lower_decimal(uint32_t dec32);

/**
 * @brief Output 32-bit decimal value to 7-segment displays
 * @param dec32 32-bit decimal value to display
 */
void tcm_seven_segment_display_decimal_32(uint32_t dec32);

/**
 * @brief Output 64-bit decimal value to 7-segment displays
 * @param dec64 64-bit decimal value to display
 */
void tcm_seven_segment_display_decimal_64(uint64_t dec64);

/**
 * @brief Clear all 7-segment displays (turn off all segments)
 */
void tcm_seven_segment_display_clear(void);

#ifdef __cplusplus
}
#endif

#endif // TCMIPS_DEVKIT_SEVEN_SEGMENT_DISPLAY_H
