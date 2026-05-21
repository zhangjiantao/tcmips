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

TCM_API uint32_t tcm_syscall_get_tickcount(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(__NR_tcm_syscall_get_tickcount)
      :);
  return __v0;
}

TCM_API uint32_t tcm_syscall_get_timestamp(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(__NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

TCM_API uint32_t tcm_syscall_get_timestamp_milli(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x20 + __NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

TCM_API uint32_t tcm_syscall_get_timestamp_micro(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x40 + __NR_tcm_syscall_get_timestamp)
      :);
  return __v0;
}

TCM_API void tcm_syscall_console_set_base(uint32_t a0) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_console)
      : "v0", "memory");
}

TCM_API void tcm_syscall_console_set_color(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x20 + __NR_tcm_syscall_console)
      : "v0", "memory");
}

TCM_API void tcm_syscall_console_write(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x80 + __NR_tcm_syscall_console)
      : "v0", "memory");
}

TCM_API uint32_t tcm_syscall_read_keyboard(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(__NR_tcm_syscall_read_keyboard)
      :);
  return __v0;
}

TCM_API uint32_t tcm_syscall_filesize(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_file)
      : "v0", "memory");
  return __v0;
}

TCM_API void tcm_syscall_fileseek(uint32_t a0) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __v0 __asm__("v0");      // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      : "=r"(__v0)
      : "r"(__a0), [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_file)
      : "v0", "memory");
}

TCM_API uint32_t tcm_syscall_fileread_32(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x20000 + 0x1400 + 0x20 + __NR_tcm_syscall_file)
      : "v0", "memory");
  return __v0;
}

TCM_API uint32_t tcm_syscall_fileread_64(void) {
  register uint32_t __v0 __asm__("v0"); // NOLINT
  __asm__ __volatile__(                 //
      "syscall %[code]\n"
      : "=r"(__v0)
      : [code] "i"(0x20000 + 0x1400 + 0x40 + __NR_tcm_syscall_file)
      : "v0", "memory");
  return __v0;
}

TCM_API void tcm_syscall_playsound(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x20 + __NR_tcm_syscall_playsound)
      : "v0", "memory");
}

TCM_API void tcm_syscall_reset_and_playsound(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + 0x40 + __NR_tcm_syscall_playsound)
      : "v0", "memory");
}

TCM_API void tcm_syscall_stop_playsound(void) {
  __asm__ __volatile__( //
      "syscall %[code]\n"
      :
      : [code] "i"(0x20000 + 0x1400 + 0x60 + __NR_tcm_syscall_playsound)
      : "v0", "memory");
}

TCM_API void tcm_syscall_seven_segment_display_upper(uint32_t a0, uint32_t a1) {
  register uint32_t __a0 __asm__("a0") = a0; // NOLINT
  register uint32_t __a1 __asm__("a1") = a1; // NOLINT
  __asm__ __volatile__(                      //
      "syscall %[code]\n"
      :
      : "r"(__a0), "r"(__a1),
        [code] "i"(0x20000 + 0x1400 + __NR_tcm_syscall_seven_segment_display)
      : "v0", "memory");
}

TCM_API void tcm_syscall_seven_segment_display_lower(uint32_t a0, uint32_t a1) {
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
