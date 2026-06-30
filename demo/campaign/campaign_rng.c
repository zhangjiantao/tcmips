//
// Created by zhangjiantao on 2026/6/15.
//

#include "stdio.h"

#include <dev/syscall.h>

int main() {
  uint32_t seed = tcm_syscall_campaign_read_input();
  while (1) {
    seed = seed ^ (seed >> 13);
    seed = seed ^ (seed << 17);
    seed = seed ^ (seed >> 5);
    tcm_syscall_campaign_write_output(seed, 0);
    printf("%d\n", seed);
  }
}