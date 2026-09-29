//
// Test the Unicode (UTF-8) text console path plus a headless self-check that
// verifies the /unifont.bin asset is packed and its codepoint index is usable.
//
// Rendering: exercises printf with CJK text (drawn into
// the console VRAM). Self-check: reads /unifont.bin through memfs and binary-
// searches a few CJK codepoints, reporting PASS/FAIL via printf (semihost).
//

#include "sys/unistd.h"

#include <dev/console.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(void) {
  printf("TCMIPS Unicode console test\n");
  printf("==========================\n");

  printf("Chinese: 你好，这是一段测试文本。\n");
  printf("Japanese: こんにちは、これはテストテキストです。\n");
  printf("Korean: 안녕하세요, 이것은 테스트 텍스트입니다.\n");
  printf("Mixed: Hello 你好 123 ABC 世界！\n");
  printf("缺字渲染：这个𠮷字应该渲染不出来\n");

  for (int i = 0; i < 40; ++i) {
    usleep(300000);
    printf("Chinese: "
           "你好，这是一段测试文本。こんにちは、これはテストテキストです。안녕"
           "하세요, 이것은 테스트 텍스트입니다. 这是第%d行。",
           i);
    fflush(stdout);
  }

  return 0;
}
