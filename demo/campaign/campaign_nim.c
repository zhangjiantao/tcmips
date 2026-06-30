//
// Created by zhangjiantao on 2026/6/15.
//

#include "sys/unistd.h"

#include <dev/console.h>
#include <dev/syscall.h>

#include <stdio.h>

int main() {
  int cards_count = tcm_syscall_campaign_read_input();
  while (cards_count) {
    usleep(1000000);
    int step = ((cards_count % 4) - 1) & 3;
    tcm_syscall_campaign_write_output(step, 0);
    cards_count = tcm_syscall_campaign_read_input();
  }

  return 0;
}