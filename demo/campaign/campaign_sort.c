//
// Created by zhangjiantao on 2026/6/15.
//

#include "sys/unistd.h"

#include <dev/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmpfn(const void *a, const void *b) {
  return *(uint8_t *)a - *(uint8_t *)b;
}

int main() {
  uint8_t array[16];
  printf("input:\n");
  for (int i = 0; i < 16; i++) {
    array[i] = (uint8_t)tcm_syscall_campaign_read_input();
    printf("%3d ", array[i]);
    fflush(stdout);
  }

  qsort(array, 16, 1, cmpfn);

  printf("\noutput:\n");
  for (int i = 0; i < 16; i++) {
    printf("%3d ", array[i]);
    fflush(stdout);
    if (i == 15)
      usleep(1000000);
    tcm_syscall_campaign_write_output(array[i], 0);
  }

  return 0;
}