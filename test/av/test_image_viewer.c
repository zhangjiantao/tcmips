//
// Created by zhangjiantao on 26-5-4.
//

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

int main(int argc, char *argv[]) {
  while (true) {
    tcm_draw_image_misaka_png();
    usleep(3000000);
    tcm_draw_image_doraemon2_png();
    usleep(3000000);
    tcm_draw_image_totoro_png();
    usleep(3000000);
    tcm_draw_image_totoro2_png();
    usleep(3000000);
    tcm_draw_image_slumdunk_png();
    usleep(3000000);
    tcm_draw_image_slumdunk2_png();
    usleep(3000000);
    tcm_draw_image_kiki_png();
    usleep(3000000);
  }

  return 0;
}
