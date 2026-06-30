//
// Created by zhangjiantao on 2026/4/30.
//

#include "sys/unistd.h"

#include <dev/console.h>
#include <dev/seven_segment_display.h>


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/times.h>

#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

int main(int argc, char *argv[]) {
  while (true) {
    struct tms t;
    times(&t);
    uint32_t ts = tcm_syscall_get_timestamp();
    uint32_t ms = tcm_syscall_get_timestamp_milli();
    uint32_t mc = tcm_syscall_get_timestamp_micro();
    uint32_t nn = tcm_syscall_get_timestamp_nano();

    printf("\r ts %u - %12u - %12u - %12u      ", ts, ms, mc, nn);
    fflush(stdout);
    tcm_seven_segment_display_hexadecimal(ts, ms);
    usleep(1000);
  }
}
