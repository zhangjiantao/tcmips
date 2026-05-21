//
// Created by zhangjiantao on 2026/5/11.
//

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>
#include <tcm_config.h>
#include <tcm_format.h>

#include <stdbool.h>
#include <stdint.h>

const uint32_t TEXT_ROW = 18;

/**
 * Bootloader abort: infinite debug trap loop
 */
__attribute__((noreturn)) void bootloader_abort(void) {
  __asm__ __volatile__("j 0x10\n\tnop\n\t");
  __builtin_unreachable();
}

/**
 * Draw boot logo
 */
void bootloader_draw_logo(void) {
#include "logo.inc"
}

/**
 * Draw progress bar border
 */
static void bootloader_draw_progressbar_border(void) {
#ifdef TCM_BOOTLOADER_SHOW_PROGRESS
  uint32_t row = TEXT_ROW;
  tcm_console_set_color(0xff, 0xff, 0xff, 0x22, 0x22, 0x22);

  for (uint32_t i = 0; i < 16; i++) {
    tcm_graphic_console_write(GRAPHIC_0, (row - 1) * 20 + 2 + i, 0xf,
                              0xc4c4c4c4);
    tcm_graphic_console_write(GRAPHIC_0, (row + 1) * 20 + 2 + i, 0xf,
                              0xc4c4c4c4);
  }

  tcm_graphic_console_write(GRAPHIC_0, (row) * 20 + 1, 0x8, 0xb3c4c4c4);
  tcm_graphic_console_write(GRAPHIC_0, (row - 1) * 20 + 1, 0x8, 0xdac4c4c4);
  tcm_graphic_console_write(GRAPHIC_0, (row - 1) * 20 + 1 + 17, 0x1,
                            0xc4c4c4bf);

  tcm_graphic_console_write(GRAPHIC_0, (row + 1) * 20 + 1, 0x8, 0xc0c4c4c4);
  tcm_graphic_console_write(GRAPHIC_0, (row + 1) * 20 + 1 + 17, 0x1,
                            0xc4c4c4d9);
  tcm_graphic_console_write(GRAPHIC_0, (row) * 20 + 1 + 17, 0x1, 0xc4c4c4b3);
#endif
}

/**
 * Draw progress bar frame
 */
static void bootloader_draw_progressbar_frame(uint32_t progress) {
#ifdef TCM_BOOTLOADER_SHOW_PROGRESS
  uint32_t row = TEXT_ROW;
  if (progress == 0)
    return;

  progress--;
  uint32_t pos = progress / 4 + 2;
  uint32_t rem = progress % 4;
  tcm_graphic_console_write(GRAPHIC_0, row * 20 + pos, 1u << rem, 0xdbdbdbdb);
#endif
}

/**
 * Load image header
 */
static void bootloader_load_header(volatile uint32_t *addr) {
  tcm_syscall_fileseek(0);
  for (uint32_t i = 0; i < sizeof(TCMImageHeader) / 8; i++, addr += 2) {
    *addr = tcm_syscall_fileread_64();
  }
}

/**
 * Update progress display
 */
static void bootloader_update_progress(uint32_t offset, uint32_t imgbase,
                                       uint32_t block) {
  bootloader_draw_progressbar_frame(((offset - imgbase) / block));
  tcm_seven_segment_display_lower_hexadecimal(offset);
}

/**
 * Load image body into memory
 */
static void bootloader_load_body(uint32_t imgbase, uint32_t imgsize) {
  tcm_seven_segment_display_upper_hexadecimal(imgsize);
  tcm_console_set_color(0xff, 0xff, 0xff, 0x48, 0xa8, 0x68);

  uint32_t offset = imgbase;
  uint32_t block = (imgsize - imgbase) / 64;
  uint32_t bitmask = (1U << (31 - __builtin_clz(block - 1))) - 1;
  tcm_syscall_fileseek(offset);

  // Image size 256-byte aligned by linker
  if (block >= 0x100) {
    for (; offset < imgsize; offset += 256) {
      volatile uint32_t *addr = (volatile uint32_t *)offset;
      addr[0] = tcm_syscall_fileread_64();
      addr[2] = tcm_syscall_fileread_64();
      addr[4] = tcm_syscall_fileread_64();
      addr[6] = tcm_syscall_fileread_64();
      addr[8] = tcm_syscall_fileread_64();
      addr[10] = tcm_syscall_fileread_64();
      addr[12] = tcm_syscall_fileread_64();
      addr[14] = tcm_syscall_fileread_64();
      addr[16] = tcm_syscall_fileread_64();
      addr[18] = tcm_syscall_fileread_64();
      addr[20] = tcm_syscall_fileread_64();
      addr[22] = tcm_syscall_fileread_64();
      addr[24] = tcm_syscall_fileread_64();
      addr[26] = tcm_syscall_fileread_64();
      addr[28] = tcm_syscall_fileread_64();
      addr[30] = tcm_syscall_fileread_64();
      addr[32] = tcm_syscall_fileread_64();
      addr[34] = tcm_syscall_fileread_64();
      addr[36] = tcm_syscall_fileread_64();
      addr[38] = tcm_syscall_fileread_64();
      addr[40] = tcm_syscall_fileread_64();
      addr[42] = tcm_syscall_fileread_64();
      addr[44] = tcm_syscall_fileread_64();
      addr[46] = tcm_syscall_fileread_64();
      addr[48] = tcm_syscall_fileread_64();
      addr[50] = tcm_syscall_fileread_64();
      addr[52] = tcm_syscall_fileread_64();
      addr[54] = tcm_syscall_fileread_64();
      addr[56] = tcm_syscall_fileread_64();
      addr[58] = tcm_syscall_fileread_64();
      addr[60] = tcm_syscall_fileread_64();
      addr[62] = tcm_syscall_fileread_64();

      if ((offset & bitmask) == 0)
        bootloader_update_progress(offset, imgbase, block);
    }
  }

  else if (block >= 0x80) {
    for (; offset < imgsize; offset += 128) {
      volatile uint32_t *addr = (volatile uint32_t *)offset;
      addr[0] = tcm_syscall_fileread_64();
      addr[2] = tcm_syscall_fileread_64();
      addr[4] = tcm_syscall_fileread_64();
      addr[6] = tcm_syscall_fileread_64();
      addr[8] = tcm_syscall_fileread_64();
      addr[10] = tcm_syscall_fileread_64();
      addr[12] = tcm_syscall_fileread_64();
      addr[14] = tcm_syscall_fileread_64();
      addr[16] = tcm_syscall_fileread_64();
      addr[18] = tcm_syscall_fileread_64();
      addr[20] = tcm_syscall_fileread_64();
      addr[22] = tcm_syscall_fileread_64();
      addr[24] = tcm_syscall_fileread_64();
      addr[26] = tcm_syscall_fileread_64();
      addr[28] = tcm_syscall_fileread_64();
      addr[30] = tcm_syscall_fileread_64();

      if ((offset & bitmask) == 0)
        bootloader_update_progress(offset, imgbase, block);
    }
  }

  else if (block >= 0x40) {
    for (; offset < imgsize; offset += 64) {
      volatile uint32_t *addr = (volatile uint32_t *)offset;
      addr[0] = tcm_syscall_fileread_64();
      addr[2] = tcm_syscall_fileread_64();
      addr[4] = tcm_syscall_fileread_64();
      addr[6] = tcm_syscall_fileread_64();
      addr[8] = tcm_syscall_fileread_64();
      addr[10] = tcm_syscall_fileread_64();
      addr[12] = tcm_syscall_fileread_64();
      addr[14] = tcm_syscall_fileread_64();

      if ((offset & bitmask) == 0)
        bootloader_update_progress(offset, imgbase, block);
    }
  }

  else if (block >= 0x20) {
    for (; offset < imgsize; offset += 32) {
      volatile uint32_t *addr = (volatile uint32_t *)offset;
      addr[0] = tcm_syscall_fileread_64();
      addr[2] = tcm_syscall_fileread_64();
      addr[4] = tcm_syscall_fileread_64();
      addr[6] = tcm_syscall_fileread_64();

      if ((offset & bitmask) == 0)
        bootloader_update_progress(offset, imgbase, block);
    }
  }

  else {
    for (; offset < imgsize; offset += 8) {
      volatile uint32_t *addr = (volatile uint32_t *)offset;
      addr[0] = tcm_syscall_fileread_64();

      if ((offset & bitmask) == 0)
        bootloader_update_progress(offset, imgbase, block);
    }
  }

  bootloader_draw_progressbar_frame(64);
  tcm_seven_segment_display_hexadecimal(0x88888888, 0x88888888);
  tcm_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);
}

/**
 * Simple strlen
 */
static uint32_t bootloader_strlen(const char *str) {
  const char *p = str;
  while (*p)
    p++;
  return p - str;
}

/**
 * Simple strcpy
 */
static void bootloader_strcpy(char *dst, const char *src) {
  while (*src)
    *dst++ = *src++;
}

/**
 * Copy TCM image header
 */
static void bootloader_copy_header(volatile uint32_t *dst,
                                   const volatile uint32_t *src) {
  for (uint32_t i = 0; i < sizeof(TCMImageHeader) / 4; i++, src++, dst++)
    *dst = *src;
}

/**
 * Draw status text
 */
static void bootloader_draw_text(uint32_t col, const char *str, uint32_t cnt,
                                 bool error) {
  uint32_t row = TEXT_ROW;

  if (error)
    tcm_console_set_color(0xff, 0xff, 0xff, 0xff, 0, 0);
  else
    tcm_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);

  for (uint32_t i = 0; i < cnt; ++i) {
    uint32_t data = *((uint32_t *)str + i);
    tcm_graphic_console_write(GRAPHIC_0, row * 20 + col + i, 0xf, data);
  }
}

/**
 * Main bootloader logic
 */
static void bootloader(void) {
  tcm_syscall_console_set_base(GRAPHIC_0);

  bootloader_draw_logo();

  TCMImageHeader self_hdr;
  volatile TCMImageHeader *image = (volatile TCMImageHeader *)0;
  bootloader_copy_header((uint32_t *)&self_hdr, (const uint32_t *)image);

  bootloader_load_header((volatile uint32_t *)image);
  uint32_t imgbase = image->w_imagebase;

  if (image->w_magic != TCM_MAGIC || imgbase < self_hdr.w_imagesize) {
    bootloader_draw_text(8, " Format Error!  ", 4, true);
    bootloader_copy_header((uint32_t *)image, (const uint32_t *)&self_hdr);
    bootloader_abort();
  }

  static char text[64] = "Loading ";
  bootloader_strcpy(text + 8, (const char *)image->filename);
  uint32_t cnt = bootloader_strlen(text) / 4 + 1;
  uint32_t col = 10 - bootloader_strlen(text) / 8;

  bootloader_draw_progressbar_border();
  bootloader_draw_text(col, text, cnt, false);

  tcm_syscall_fileseek(-1);
  uint32_t imgsize = tcm_syscall_fileread_32();
  bootloader_load_body(imgbase, imgsize);

  // Clear screen and jump to image
  tcm_graphic_console_clear(GRAPHIC_0);
  __asm__ __volatile__("j 0\n\tnop\n\t");
  __builtin_unreachable();
}

/**
 * Bootloader entry point (naked, no stack setup)
 */
__attribute__((naked)) void TCM_SYMBOL_START(void) {
#if TCM_MEMORY_SIZE & 0xFFFF
#error "TCM_MEMORY_SIZE must be 64KB aligned"
#endif

#define TCM_STACK_TOP_HI16 ((TCM_MEMORY_SIZE >> 16) & 0xFFFF)
  __asm__ __volatile__(
      ".set push\n\t"
      ".set noreorder\n\t"
      "jr  %[BOOT]\n\t"
      "lui $sp, %[STACK]\n\t"
      ".set pop\n\t"
      :
      : [BOOT] "r"(&bootloader), [STACK] "i"(TCM_STACK_TOP_HI16));
#undef TCM_STACK_TOP_HI16
}