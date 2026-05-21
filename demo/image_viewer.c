//
// Created by zhangjiantao on 26-5-4.
//

#include "stdio.h"

#include <dev/console.h>
#include <dev/keyboard.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdbool.h>
#include <unistd.h>

#ifdef TCM_HOST_WIN32
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_doraemon2_png_code.inc"
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_kiki_png_code.inc"
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_misaka_png_code.inc"
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_slumdunk2_png_code.inc"
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_slumdunk_png_code.inc"
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_totoro2_png_code.inc"
#include "D:\code\tcmips-tools\img_to_tcm_render_code\tcm_totoro_png_code.inc"
#elif defined(TCM_HOST_APPLE)
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_doraemon2_png_code.inc"
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_kiki_png_code.inc"
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_misaka_png_code.inc"
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_slumdunk2_png_code.inc"
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_slumdunk_png_code.inc"
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_totoro2_png_code.inc"
#include "/Users/zhangjiantao/py-text-bmp/img_to_tcm_render_code/tcm_totoro_png_code.inc"
#endif

#include <sys/unistd.h>

void image_viewer(void) {
  printf(">>> Ok, let's go\n");
  printf(">>> Running... Press Q to exit.\n");

  bool quit = false;
  do {
    tcm_draw_image_misaka_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
    tcm_draw_image_doraemon2_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
    tcm_draw_image_totoro_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
    tcm_draw_image_totoro2_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
    tcm_draw_image_slumdunk_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
    tcm_draw_image_slumdunk2_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
    tcm_draw_image_kiki_png();
    usleep(3000000);
    quit = tcm_keyboard_get_code() == 'q' || tcm_keyboard_get_code() == 'Q';
    if (quit)
      break;
  } while (false);

  tcm_syscall_console_set_color(0, 0xffffff00);
  tcm_syscall_seven_segment_display_upper(0, 0);
  tcm_syscall_seven_segment_display_lower(0, 0);
  printf(">>> Exiting...\n");
  tcm_graphic_console_clear(GRAPHIC_0);
  tcm_graphic_console_clear(GRAPHIC_1);
  tcm_graphic_console_clear(GRAPHIC_2);
  tcm_graphic_console_clear(GRAPHIC_3);
}
