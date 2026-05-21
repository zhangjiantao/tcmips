//
// Created by zhangjiantao on 2026/4/30.
//

#include <tcm_config.h>

#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

uint8_t seven_segment_display_table_upper[] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
    0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71,
};

// Lower 7-segment display is inverted, with reversed drive logic.
uint8_t seven_segment_display_table_lower[] = {
    0x3f, 0x30, 0x5b, 0x79, 0x74, 0x6d, 0x6f, 0x38,
    0x7f, 0x7d, 0x7e, 0x67, 0x0f, 0x73, 0x4f, 0x4e,
};

TCM_API void tcm_seven_segment_display_upper_hexadecimal(uint32_t hex) {
  uint32_t a0 = 0, a1 = 0;
  for (int i = 0; i < 4; i++) {
    a1 <<= 8;
    a1 |= seven_segment_display_table_upper[(hex & 0xf0000000) >> 28];
    a0 <<= 8;
    a0 |= seven_segment_display_table_upper[(hex & 0xf000) >> 12];
    hex <<= 4;
  }
  tcm_syscall_seven_segment_display_upper(a0, a1);
}

TCM_API void tcm_seven_segment_display_lower_hexadecimal(uint32_t hex) {
  uint32_t b0 = 0, b1 = 0;
  for (int i = 0; i < 4; i++) {
    b1 <<= 8;
    b1 |= seven_segment_display_table_lower[hex & 0xf];
    b0 <<= 8;
    b0 |= seven_segment_display_table_lower[(hex >> 16) & 0xf];
    hex >>= 4;
  }
  tcm_syscall_seven_segment_display_lower(b0, b1);
}

TCM_API void tcm_seven_segment_display_hexadecimal(uint32_t upper,
                                                   uint32_t lower) {
  uint32_t a0 = 0, a1 = 0, b0 = 0, b1 = 0;
  for (int i = 0; i < 4; i++) {
    a1 <<= 8;
    b1 <<= 8;
    a1 |= seven_segment_display_table_upper[(upper & 0xf0000000) >> 28];
    b1 |= seven_segment_display_table_lower[lower & 0xf];
    upper <<= 4;
    lower >>= 4;
  }
  for (int i = 0; i < 4; i++) {
    a0 <<= 8;
    b0 <<= 8;
    a0 |= seven_segment_display_table_upper[(upper & 0xf0000000) >> 28];
    b0 |= seven_segment_display_table_lower[lower & 0xf];
    upper <<= 4;
    lower >>= 4;
  }
  tcm_syscall_seven_segment_display_upper(a0, a1);
  tcm_syscall_seven_segment_display_lower(b0, b1);
}

TCM_API void tcm_seven_segment_display_upper_decimal(uint32_t dec) {
  uint32_t a0 = 0, a1 = 0;
  if (dec == 0) {
    a0 = seven_segment_display_table_upper[0];
  } else {
    int pos = 0;
    while (dec && pos < 4) {
      a0 |= (seven_segment_display_table_upper[dec % 10] << (pos++ * 8));
      dec /= 10;
    }
    pos = 0;
    while (dec && pos < 4) {
      a1 |= (seven_segment_display_table_upper[dec % 10] << (pos++ * 8));
      dec /= 10;
    }
  }

  tcm_syscall_seven_segment_display_upper(a0, a1);
}

TCM_API void tcm_seven_segment_display_lower_decimal(uint32_t dec) {
  uint32_t b0 = 0, b1 = 0;
  if (dec == 0) {
    b1 = seven_segment_display_table_lower[0] << 24;
  } else {
    int pos = 4;
    while (dec && pos > 0) {
      b1 |= (seven_segment_display_table_lower[dec % 10] << (--pos * 8));
      dec /= 10;
    }
    pos = 4;
    while (dec && pos > 0) {
      b0 |= (seven_segment_display_table_lower[dec % 10] << (--pos * 8));
      dec /= 10;
    }
  }

  tcm_syscall_seven_segment_display_lower(b0, b1);
}

TCM_API void tcm_seven_segment_display_decimal_32(uint32_t dec) {
  __attribute__((aligned(4))) uint8_t buffer[16] = {0};
  if (dec == 0) {
    buffer[7] = seven_segment_display_table_upper[0];
  } else {
    int pos = 0;
    while (dec && pos < 8) {
      buffer[7 - pos++] = seven_segment_display_table_lower[dec % 10];
      dec /= 10;
    }
    while (dec && pos < 16) {
      buffer[pos++] = seven_segment_display_table_upper[dec % 10];
      dec /= 10;
    }
  }

  uint32_t a0 = 0, a1 = 0, b0 = 0, b1 = 0;
  b0 = *(uint32_t *)(buffer + 0);
  b1 = *(uint32_t *)(buffer + 4);
  a0 = *(uint32_t *)(buffer + 8);
  a1 = *(uint32_t *)(buffer + 12);

  tcm_syscall_seven_segment_display_upper(a0, a1);
  tcm_syscall_seven_segment_display_lower(b0, b1);
}

TCM_API void tcm_seven_segment_display_decimal_64(uint64_t dec) {
  __attribute__((aligned(4))) uint8_t buffer[16] = {0};
  if (dec == 0) {
    buffer[7] = seven_segment_display_table_upper[0];
  } else {
    int pos = 0;
    while (dec && pos < 8) {
      buffer[7 - pos++] = seven_segment_display_table_lower[dec % 10];
      dec /= 10;
    }
    while (dec && pos < 16) {
      buffer[pos++] = seven_segment_display_table_upper[dec % 10];
      dec /= 10;
    }
  }

  uint32_t a0 = 0, a1 = 0, b0 = 0, b1 = 0;
  b0 = *(uint32_t *)(buffer + 0);
  b1 = *(uint32_t *)(buffer + 4);
  a0 = *(uint32_t *)(buffer + 8);
  a1 = *(uint32_t *)(buffer + 12);

  tcm_syscall_seven_segment_display_upper(a0, a1);
  tcm_syscall_seven_segment_display_lower(b0, b1);
}

TCM_API void tcm_seven_segment_display_clear(void) {
  tcm_syscall_seven_segment_display_upper(0, 0);
  tcm_syscall_seven_segment_display_lower(0, 0);
}
