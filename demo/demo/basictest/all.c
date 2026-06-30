//
// Created by zhangjiantao on 2026/5/11.
//

#include "stdlib.h"

#include <dev/console.h>

#include <stdio.h>

extern int test_sha256_main(int argc, char *argv[]);
extern int test_aes_main(int argc, char *argv[]);
extern int test_arithmetic_float_main(int argc, char *argv[]);
extern int test_arithmetic_builtin_main(int argc, char *argv[]);
extern int test_arithmetic_int64_main(int argc, char *argv[]);

extern int test_stdlib_malloc(int argc, char *argv[]);
extern int test_stdlib_malloc_stress(int argc, char *argv[]);
extern int test_stdlib_rand(int argc, char *argv[]);
extern int test_stdlib_regex(int argc, char *argv[]);
extern int test_stdlib_setjmp(int argc, char *argv[]);
extern int test_stdlib_strftime(int argc, char *argv[]);
extern int test_stdlib_time(int argc, char *argv[]);

#define test_assert(r)                                                         \
  {                                                                            \
    if (r) {                                                                   \
      printf("Assertion failed: %s %d\n", __FILE__, __LINE__);                 \
      exit(1);                                                                 \
    }                                                                          \
  }

int basictest(int argc, char *argv[]) {
  int r = 0;

  printf("\n\n===== Test Built-in Functions =====\n");
  r |= test_arithmetic_builtin_main(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Floating-Point Operations =====\n");
  r |= test_arithmetic_float_main(argc, argv);
  test_assert(r);

  printf("\n\n===== Test 64-bit Integer Operations =====\n");
  r |= test_arithmetic_int64_main(argc, argv);
  test_assert(r);

  printf("\n\n===== Test AES Algorithm =====\n");
  r |= test_aes_main(argc, argv);
  test_assert(r);

  printf("\n\n===== Test SHA256 Hash Algorithm =====\n");
  r |= test_sha256_main(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Malloc =====\n");
  r |= test_stdlib_malloc(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Malloc Stress =====\n");
  r |= test_stdlib_malloc_stress(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Rand =====\n");
  r |= test_stdlib_rand(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Regex");
  r |= test_stdlib_regex(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Setjmp =====\n");
  r |= test_stdlib_setjmp(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Strftime =====\n");
  r |= test_stdlib_strftime(argc, argv);
  test_assert(r);

  printf("\n\n===== Test Stdlib Time =====\n");
  r |= test_stdlib_time(argc, argv);
  test_assert(r);
  return r;
}
