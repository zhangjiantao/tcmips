//
// Created by zhangjiantao on 2026/5/18.
//

#include <dev/seven_segment_display.h>

int overflow(int x, char *buf) {
  if (x < 0)
    return 0;
  char var[0x10];
  var[0] = buf[0];
  tcm_seven_segment_display_hexadecimal((uintptr_t)var, x);
  return overflow(x + 1, var);
}
int main(int argc, char **argv) {
  char var[0x10];
  return overflow(argc, var);
}
