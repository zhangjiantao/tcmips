//
// Created by zhangjiantao on 26-5-4.
//

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdbool.h>
#include <stdio.h>

#include "asserts/tcm_caixukun_mp4_render_code.inc"

void video_player(void) {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");

  tcm_syscall_console_set_base(GRAPHIC_0);

  bool quit = false;
  while (!quit) {
    uint32_t t_begin = tcm_syscall_get_timestamp_milli();
    uint32_t t1, t2;

    for (uint32_t i = 0; i < tcm_frame_count && !quit; i++) {
      quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';

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
  tcm_syscall_console_set_base(tcm_text_console_get_base());
  tcm_syscall_console_set_color(0, 0xffffff00);
  tcm_syscall_seven_segment_display_upper(0, 0);
  tcm_syscall_seven_segment_display_lower(0, 0);
  printf(">>> Exiting...\n");
  tcm_graphic_console_clear(GRAPHIC_0);
}