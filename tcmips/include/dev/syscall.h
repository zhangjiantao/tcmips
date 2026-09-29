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
// Note: syscall number 6 is currently unused and must not be allocated
// without updating the switch-case in peripheral.c accordingly.
#define __NR_tcm_syscall_semihost               7
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read one input word from the game campaign/level controller.
 *
 * Blocks until the campaign host supplies a single 32-bit word (e.g., array
 * size, seed value, disk count, or target identifier). The guest program
 * typically calls this repeatedly to fetch all inputs for a campaign level.
 *
 * @return A 32-bit input word provided by the campaign host.
 */
uint32_t tcm_syscall_campaign_read_input(void);

/**
 * @brief Write one output word to the game campaign/level controller.
 *
 * Sends a single 32-bit output (e.g., a computation step, sort element, or
 * movement command) to the campaign host. The second parameter is unused
 * (always 0 in practice) and reserved for future protocol extensions.
 *
 * @param a0 Output word to send to the campaign host.
 * @param a1 Reserved, must be 0.
 */
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
 * @brief Text character console operation
 * @param a0 Console command code
 * @param a1 Command parameter value
 */
void tcm_syscall_text_console(uint32_t a0, uint32_t a1);

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

/**
 * @brief Initialize a palette entry with a pixel-format colour value.
 *
 * Configures the index-to-colour mapping used by the pixel console. The a1
 * parameter carries the raw colour data; its encoding includes R, G, and B
 * components and may vary depending on the colour model (e.g. ARGB8888 or
 * RGB332). The caller iterates palette indices 0-N to set up the full
 * hardware palette before rendering begins.
 *
 * @param a0 Palette index (0-based).
 * @param a1 Colour value in the color buffer's byte order.
 */
void tcm_syscall_palette_init(uint32_t a0, uint32_t a1);

/**
 * @brief Upload packed pixel data to the pixel console VRAM.
 *
 * Transfers colour-indexed pixels to the target destination set by the most
 * recent tcm_syscall_palette_set_offset(). Both parameters carry packed pixel
 * words; the caller hides the word count inside the syscall instruction.
 *
 * @param a0 Low word of the packed pixel data.
 * @param a1 High word of the packed pixel data.
 */
void tcm_syscall_palette_upload(uint32_t a0, uint32_t a1);

/**
 * @brief Set the pixel console VRAM base-offset for the next transfer.
 *
 * Specifies the destination address within pixel-console VRAM for subsequent
 * palette_upload() calls. The address is relative to the pixel console's
 * screen buffer (TCM_VRAM_PIXEL_ADDR) and must be 8-byte aligned.
 *
 * @param a0 VRAM byte-offset from the screen buffer base.
 */
void tcm_syscall_palette_set_offset(uint32_t a0);

/**
 * @brief Read data from the host emulator's stdin via the semihosting bridge.
 *
 * Forwards the read request to the host simulator/runtime through a dedicated
 * syscall. When the host supports semihosting, this provides a fast I/O path
 * that bypasses the text console.
 *
 * @param a0 Guest buffer address (cast to uint32_t).
 * @param a1 Maximum number of bytes to read.
 * @return Number of bytes actually read on success; 0 on EOF (backing input
 *         exhausted); 0xFFFFFFFF when no backing stdin file is wired up (the
 *         caller must fall back to the text console).
 */
uint32_t tcm_syscall_semihost_stdin(uint32_t a0, uint32_t a1);

/**
 * @brief Write data to the host emulator's stdout via the semihosting bridge.
 *
 * Forwards the write request to the host simulator/runtime. The caller
 * dispatches based on POSIX fd (fd == 1 → stdout, fd == 2 → stderr). The
 * return value is available but current callers use this as fire-and-forget
 * (stdout/stderr output also falls through to the text console).
 *
 * @param a0 Guest buffer address (cast to uint32_t).
 * @param a1 Number of bytes to write.
 * @return Number of bytes written on success (may be ignored by the caller),
 *         0 or negative on failure.
 */
uint32_t tcm_syscall_semihost_stdout(uint32_t a0, uint32_t a1);

/**
 * @brief Write data to the host emulator's stderr via the semihosting bridge.
 *
 * Semantics are identical to tcm_syscall_semihost_stdout(), but the host
 * directs the output to its stderr stream instead of stdout. The return
 * value is available but current callers treat this as fire-and-forget.
 *
 * @param a0 Guest buffer address (cast to uint32_t).
 * @param a1 Number of bytes to write.
 * @return Number of bytes written on success (may be ignored by the caller),
 *         0 or negative on failure.
 */
uint32_t tcm_syscall_semihost_stderr(uint32_t a0, uint32_t a1);

#ifdef __cplusplus
}
#endif

#endif