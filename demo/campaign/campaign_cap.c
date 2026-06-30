//
// Created by zhangjiantao on 2026/6/15.
//

#include "string.h"

#include <dev/syscall.h>

#include <stdio.h>

char buffer[4096];

int main() {
  size_t p = 0;
  char c = 0;

  do {
    c = (char)tcm_syscall_campaign_read_input();
    buffer[p++] = c;
  } while (c != 0);

  printf("%s\n", buffer);

  buffer[0] ^= 0x20;

  for (size_t i = 1; i < strlen(buffer); i++) {
    if (buffer[i - 1] == 0x20)
      buffer[i] ^= 0x20;
  }

  printf("%s\n", buffer);

  for (size_t i = 0; i < strlen(buffer); i++) {
    tcm_syscall_campaign_write_output(buffer[i], 0);
  }
}