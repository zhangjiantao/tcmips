//
// Created by zhangjiantao on 2026/6/15.
//

#include "sys/unistd.h"

#include <dev/syscall.h>

#include <stdio.h>

#define SLEEP_TIME 100000

void move_src_dst(int src, int dst) {
  tcm_syscall_campaign_write_output(src, 0);
  usleep(SLEEP_TIME);
  tcm_syscall_campaign_write_output(5, 0);
  usleep(SLEEP_TIME);
  tcm_syscall_campaign_write_output(dst, 0);
  usleep(SLEEP_TIME);
  tcm_syscall_campaign_write_output(5, 0);

  printf("> move from %d to %d\n", src, dst);
}

void move(int disknr, int src, int dst, int spare) {
  if (disknr == 0) {
    move_src_dst(src, dst);
  } else {
    move(disknr - 1, src, spare, dst);
    move_src_dst(src, dst);
    move(disknr - 1, spare, dst, src);
  }
}

int main() {

  int disknr = (int)tcm_syscall_campaign_read_input();
  int src = (int)tcm_syscall_campaign_read_input();
  int dst = (int)tcm_syscall_campaign_read_input();
  int spare = (int)tcm_syscall_campaign_read_input();

  move(disknr, src, dst, spare);

  return 0;
}