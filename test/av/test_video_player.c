//
// Created by zhangjiantao on 26-5-4.
//

#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdbool.h>

#ifdef TCM_HOST_WIN32
#include "D:\code\tcmips-tools\mp4_to_tcm_render_code\tcm_caixukun_mp4_render_code.inc"
#elif defined(TCM_HOST_APPLE)
#include "/Users/zhangjiantao/py-text-bmp/mp4_to_tcm_render_code/tcm_caixukun_mp4_render_code.inc"
#endif

int main(int argc, char *argv[]) {
  while (true) {
    uint32_t t_begin = tcm_syscall_get_timestamp_milli();
    uint32_t t1, t2;
    for (uint32_t i = 0; i < tcm_frame_count; i++) {
      t1 = tcm_syscall_get_timestamp_milli();
      tcm_frame_fn[i]();
      t2 = tcm_syscall_get_timestamp_milli();

      tcm_seven_segment_display_upper_decimal(i);
      tcm_seven_segment_display_lower_decimal(t2 - t1);
      t2 = tcm_syscall_get_timestamp_milli();

      if (t2 - t_begin < i * tcm_frame_interval)
        while (t2 - t1 < tcm_frame_interval)
          t2 = tcm_syscall_get_timestamp_milli();
    }
  }
}