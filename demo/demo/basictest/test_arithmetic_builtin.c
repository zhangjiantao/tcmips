//
// Created by zhangjiantao on 26-5-10.
//

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

// Test result macros
#define TEST_PASS(fmt, ...)                                                    \
  do {                                                                         \
    tcm_ascii_console_set_color(0, 0, 0, 0, 0xff, 0);                          \
    printf("[PASS] " fmt "\n", ##__VA_ARGS__);                                 \
    tcm_ascii_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);                    \
  } while (0)
#define TEST_FAIL(fmt, ...)                                                    \
  do {                                                                         \
    tcm_ascii_console_set_color(0, 0, 0, 0xff, 0, 0);                          \
    printf("[FAIL] " fmt "\n", ##__VA_ARGS__);                                 \
    tcm_ascii_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);                    \
  } while (0)

#ifdef TESTALL
int test_arithmetic_builtin_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
  argc--;

  printf("===== Arithmetic Built-in Functions Auto Test =====\n\n");

  // ==============================================
  // Test 1: Bit Count Operations
  // ==============================================
  printf("[Test 1] Bit Count Built-ins\n");

  uint32_t v32 = 0x0000F000;
  int clz32 = __builtin_clz(v32 + argc);
  if (clz32 == 16)
    TEST_PASS("__builtin_clz(0x%08X) = %d", v32, clz32);
  else
    TEST_FAIL("__builtin_clz expected 16, got %d", clz32);

  int ctz = __builtin_ctz(0x100 + argc);
  if (ctz == 8)
    TEST_PASS("__builtin_ctz(0x100) = %d", ctz);
  else
    TEST_FAIL("__builtin_ctz expected 8, got %d", ctz);

  int pop = __builtin_popcount(0xFF + argc);
  if (pop == 8)
    TEST_PASS("__builtin_popcount(0xFF) = %d", pop);
  else
    TEST_FAIL("__builtin_popcount expected 8, got %d", pop);

  uint64_t v64 = 0xFFFFFFFFFFFFFFFFULL;
  int clz64 = __builtin_clzll(v64 + argc);
  int pop64 = __builtin_popcountll(v64 + argc);
  if (clz64 == 0)
    TEST_PASS("__builtin_clzll(64bit all 1) = %d", clz64);
  else
    TEST_FAIL("__builtin_clzll expected 0, got %d", clz64);
  if (pop64 == 64)
    TEST_PASS("__builtin_popcountll(64bit all 1) = %d", pop64);
  else
    TEST_FAIL("__builtin_popcountll expected 64, got %d", pop64);

  // ==============================================
  // Test 2: Overflow Check Built-ins
  // ==============================================
  printf("\n[Test 2] Overflow Check Built-ins\n");

  int res;
  // Normal add (no overflow)
  int add_ret = __builtin_sadd_overflow(100 + argc, 200 + argc, &res);
  if (!add_ret && res == 300)
    TEST_PASS("__builtin_sadd_overflow(100+200) correct");
  else
    TEST_FAIL("__builtin_sadd_overflow add error");

  // Overflow add
  add_ret = __builtin_sadd_overflow(INT_MAX, 1 + argc, &res);
  if (add_ret)
    TEST_PASS("__builtin_sadd_overflow(INT_MAX+1) detected overflow");
  else
    TEST_FAIL("__builtin_sadd_overflow failed to detect overflow");

  // Overflow sub
  int sub_ret = __builtin_ssub_overflow(INT_MIN, 1 + argc, &res);
  if (sub_ret)
    TEST_PASS("__builtin_ssub_overflow(INT_MIN-1) detected overflow");
  else
    TEST_FAIL("__builtin_ssub_overflow failed to detect overflow");

  // Overflow mul
  int mul_ret = __builtin_smul_overflow(INT_MAX, 2 + argc, &res);
  if (mul_ret)
    TEST_PASS("__builtin_smul_overflow(INT_MAX*2) detected overflow");
  else
    TEST_FAIL("__builtin_smul_overflow failed to detect overflow");

  // ==============================================
  // Test 3: Byte Swap (Endian)
  // ==============================================
  printf("\n[Test 3] Byte Swap Built-ins\n");

  uint16_t b16 = __builtin_bswap16(0x1234 + argc);
  uint32_t b32 = __builtin_bswap32(0x12345678 + argc);
  uint64_t b64 = __builtin_bswap64(0x1122334455667788ULL + argc);

  if (b16 == 0x3412)
    TEST_PASS("__builtin_bswap16 correct: 0x%04X", b16);
  else
    TEST_FAIL("__builtin_bswap16 wrong: 0x%04X", b16);

  if (b32 == 0x78563412)
    TEST_PASS("__builtin_bswap32 correct: 0x%08X", b32);
  else
    TEST_FAIL("__builtin_bswap32 wrong: 0x%08X", b32);

  if (b64 == 0x8877665544332211ULL)
    TEST_PASS("__builtin_bswap64 correct: 0x%016" PRIx64, b64);
  else
    TEST_FAIL("__builtin_bswap64 wrong");

  // ==============================================
  // Test 4: Parity Check
  // ==============================================
  printf("\n[Test 4] Parity Check\n");

  int p1 = __builtin_parity(0xFF + argc); // 8 ones → even parity → 0
  int p2 = __builtin_parity(0x0F + argc); // 4 ones → even parity → 0
  if (p1 == 0 && p2 == 0)
    TEST_PASS("__builtin_parity correct");
  else
    TEST_FAIL("__builtin_parity wrong");

  printf("\n===== All Arithmetic Tests Completed =====\n");
  return 0;
}
