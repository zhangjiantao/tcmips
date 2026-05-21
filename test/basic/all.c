//
// Created by zhangjiantao on 2026/5/11.
//

#include <dev/console.h>

#include <stdio.h>

extern int test_sha256_main(int argc, char *argv[]);
extern int test_aes_main(int argc, char *argv[]);
extern int test_arithmetic_float_main(int argc, char *argv[]);
extern int test_arithmetic_builtin_main(int argc, char *argv[]);
extern int test_arithmetic_int64_main(int argc, char *argv[]);

int main(int argc, char **argv) {
  int r = 0;

  printf("\n\n===== Test Built-in Functions =====\n");
  r |= test_arithmetic_builtin_main(argc, argv);

  printf("\n\n===== Test Floating-Point Operations =====\n");
  r |= test_arithmetic_float_main(argc, argv);

  printf("\n\n===== Test 64-bit Integer Operations =====\n");
  r |= test_arithmetic_int64_main(argc, argv);

  printf("\n\n===== Test AES Algorithm =====\n");
  r |= test_aes_main(argc, argv);

  printf("\n\n===== Test SHA256 Hash Algorithm =====\n");
  r |= test_sha256_main(argc, argv);

  return r;
}
