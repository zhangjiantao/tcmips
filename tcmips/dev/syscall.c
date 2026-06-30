//
// Created by zhangjiantao on 2026/4/30.
//

#include <dev/syscall.h>
#include <tcm_config.h>

/*
 * 0x20000 + 0x1400 + 0x20, where 0x20000 represents the instruction rs field
 * encoded as \$a0, 0x1400 represents the instruction rt field encoded as \$a1,
 * and 0x20 represents the instruction rd field encoded as 1. The same rule
 * applies to other cases.
 */

uint32_t tcm_syscall_campaign_read_input(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(__NR_tcm_syscall_campaign)
      :);
  return __v0;
}

void tcm_syscall_campaign_write_output(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x20 + __NR_tcm_syscall_campaign)
      : "v0", "memory");
}

uint32_t tcm_syscall_get_timestamp(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(__NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

uint32_t tcm_syscall_get_timestamp_milli(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x20 + __NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

uint32_t tcm_syscall_get_timestamp_micro(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x40 + __NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

uint32_t tcm_syscall_get_timestamp_nano(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x60 + __NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

void tcm_syscall_pixel_console(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0),
        "r"(__a1), [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_console)
      : "v0", "memory");
}

void tcm_syscall_ascii_console(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x20 + __NR_tcm_syscall_console)
      : "v0", "memory");
}

uint32_t tcm_syscall_read_keyboard(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(__NR_tcm_syscall_read_keyboard)
      :);
  return __v0;
}

void tcm_syscall_seven_segment_display_upper(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_seven_segment_display)
      : "v0", "memory");
}

void tcm_syscall_seven_segment_display_lower(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x20 +
                   __NR_tcm_syscall_seven_segment_display)
      : "v0", "memory");
}

void tcm_syscall_palette_init(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0),
        "r"(__a1), [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_palette)
      : "v0", "memory");
}

void tcm_syscall_palette_upload(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x20 + __NR_tcm_syscall_palette)
      : "v0", "memory");
}

void tcm_syscall_palette_set_offset(uint32_t a0) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), [code] "i"(0x20000 + 0x1400 + 0x40 +
                              __NR_tcm_syscall_palette)
      : "v0", "memory");
}
