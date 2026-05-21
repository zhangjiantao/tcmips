//
// Created by zhangjiantao on 2026/4/30.
//

#include <dev/console.h>
#include <dev/keyboard.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

int main(int argc, char *argv[]) {
  printf("keyboard test, press any key...\n");
  while (true) {
    uint32_t code = tcm_keyboard_get_code();
    if (code == 0 || code & 0x100)
      continue;

    tcm_syscall_playsound(code % 112, 0);
    tcm_text_console_write_char(code == __TCM_KEY_CODE_ENTER ? '\n'
                                                             : (char)code);
  }
}
