//
// Created by zhangjiantao on 2026/5/18.
//

#include "stdio.h"

#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdint.h>
#include <string.h>

__attribute__((used)) char buf1[0x400000];
__attribute__((used)) char buf2[0x400000];

__attribute__((noinline)) __attribute__((no_builtin("memcpy"))) void *
my_memcpy(void *__restrict dst0, const void *__restrict src0, size_t len0) {
  uint32_t *dst = (uint32_t *)dst0;
  uint32_t *src = (uint32_t *)src0;
  while (len0 >= sizeof(uint32_t) * 16) {
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    len0 -= sizeof(uint32_t) * 16;
  }
  while (len0 >= sizeof(uint32_t) * 8) {
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    len0 -= sizeof(uint32_t) * 8;
  }
  while (len0 >= sizeof(uint32_t) * 4) {
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    len0 -= sizeof(uint32_t) * 4;
  }
  while (len0 >= sizeof(uint32_t)) {
    *dst++ = *src++;
    len0 -= sizeof(uint32_t);
  }
  uint8_t *dst1 = (uint8_t *)dst;
  uint8_t *src1 = (uint8_t *)src;
  while (len0--)
    *dst1++ = *src1++;

  return dst0;
}

void *my_memset(void *m, int c, size_t n) {
  uint32_t c32 =
      (uint8_t)c | (uint32_t)c << 8 | (uint32_t)c << 16 | (uint32_t)c << 24;
  uint32_t *dst = (uint32_t *)m;
  while (n >= sizeof(uint32_t) * 16) {
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    n -= sizeof(uint32_t) * 16;
  }
  while (n >= sizeof(uint32_t) * 8) {
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    n -= sizeof(uint32_t) * 8;
  }
  while (n >= sizeof(uint32_t) * 4) {
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    *dst++ = c32;
    n -= sizeof(uint32_t) * 8;
  }
  while (n >= sizeof(uint32_t)) {
    *dst++ = c32;
    n -= sizeof(uint32_t);
  }
  uint8_t *dst1 = (uint8_t *)dst;
  while (n--)
    *dst1++ = (uint8_t)c;
  return m;
}

int main() {
  printf("memcpy test\n");
  uint32_t t1 = tcm_syscall_get_timestamp_milli();
  memcpy(buf1, buf2, sizeof(buf2));
  uint32_t t2 = tcm_syscall_get_timestamp_milli();
  my_memcpy(buf1, buf2, sizeof(buf2));
  uint32_t t3 = tcm_syscall_get_timestamp_milli();

  tcm_seven_segment_display_upper_decimal(t2 - t1);
  tcm_seven_segment_display_lower_decimal(t3 - t2);

  printf("press any key continue\n");
  while (getchar() != '\n')
    ;

  t1 = tcm_syscall_get_timestamp_milli();
  memset(buf1, 1, sizeof(buf1));
  t2 = tcm_syscall_get_timestamp_milli();
  my_memset(buf1, 1, sizeof(buf2));
  t3 = tcm_syscall_get_timestamp_milli();

  tcm_seven_segment_display_upper_decimal(t2 - t1);
  tcm_seven_segment_display_lower_decimal(t3 - t2);

  __builtin_debugtrap();
  return 0;
}