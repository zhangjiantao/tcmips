#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <stdbool.h>
#include <stdio.h>

int main(int argc, char *argv[]) {

  puts("-----------------\nstdio test");

  char buf[32] = {0};

  for (uint32_t i = 0; i < 3; i++) {
    printf("test input: ");
    scanf("%s", buf);
    printf("buffer content: [%s]\n", buf);
  }

  fflush(stdout);

  tcm_text_console_write_string("-----------------\nconsole test\n");
  for (uint32_t i = 0; i < 3; i++) {
    tcm_text_console_write_string("test input: ");
    tcm_text_console_read_string(buf, 32);
    tcm_text_console_write_string("buffer content: ");
    tcm_console_set_color(0, 0, 0xaa, 0xff, 0, 0);
    tcm_text_console_write_string(buf);
    tcm_console_set_color(0, 0, 0, 0xff, 0xff, 0xff);
    tcm_text_console_write_char('\n');
  }
  return 0;
}
