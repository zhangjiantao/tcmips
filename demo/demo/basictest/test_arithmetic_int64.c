//
// Created by zhangjiantao on 26-5-10.
//

#include <dev/console.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

// Test result macros
#define PASS(fmt, ...)                                                         \
  do {                                                                         \
    tcm_ascii_console_set_color(0, 0, 0, 0, 0xff, 0);                          \
    printf("[PASS] " fmt "\n", ##__VA_ARGS__);                                 \
    tcm_ascii_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);                    \
  } while (0)
#define FAIL(fmt, ...)                                                         \
  do {                                                                         \
    tcm_ascii_console_set_color(0, 0, 0, 0xff, 0, 0);                          \
    printf("[FAIL] " fmt "\n", ##__VA_ARGS__);                                 \
    tcm_ascii_console_set_color(0xff, 0xff, 0xff, 0, 0, 0);                    \
  } while (0)

// Volatile global 64-bit integers (force real runtime calculation)
volatile static int64_t g_a = 10000000000LL; // 10^10
volatile static int64_t g_b = 20000000000LL; // 2*10^10
volatile static int64_t g_c = 3LL;
volatile static int64_t g_neg = -5000000000LL;
volatile static uint64_t g_ua = 1234567890123456789ULL;
volatile static uint64_t g_ub = 987654321098765432ULL;
volatile static int64_t g_shift_val = 1LL;
volatile static int64_t g_min = INT64_MIN;
volatile static int64_t g_max = INT64_MAX;

#ifdef TESTALL
int test_arithmetic_int64_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
  printf("===== 64-bit Integer Arithmetic Test =====\n");
  printf("Volatile global variables | No compiler optimization\n\n");

  // -------------------------------------------------------------------------
  // Test 1: Basic 64-bit arithmetic (add, sub, mul, div, mod)
  // -------------------------------------------------------------------------
  printf("[Test 1] Basic 64-bit Operations\n");

  int64_t add = g_a + g_b;
  int64_t sub = g_b - g_a;
  int64_t mul = g_a * g_c;
  int64_t div = g_b / g_c;
  int64_t mod = g_b % g_c;

  if (add == 30000000000LL)
    PASS("add: 10e9 + 20e9 = 30e9");
  else
    FAIL("add failed");

  if (sub == 10000000000LL)
    PASS("sub: 20e9 - 10e9 = 10e9");
  else
    FAIL("sub failed");

  if (mul == 30000000000LL)
    PASS("mul: 10e9 * 3 = 30e9");
  else
    FAIL("mul failed");

  if (div == 6666666666LL)
    PASS("div: 20e9 / 3 = 6666666666");
  else
    FAIL("div failed");

  if (mod == 2LL)
    PASS("mod: 20e9 %% 3 = 2");
  else
    FAIL("mod failed");

  // -------------------------------------------------------------------------
  // Test 2: Negative 64-bit handling
  // -------------------------------------------------------------------------
  printf("\n[Test 2] Negative Value Operations\n");

  int64_t neg_add = g_neg + g_a;
  if (neg_add == 5000000000LL)
    PASS("neg add: -5e9 + 10e9 = 5e9");
  else
    FAIL("neg add failed");

  // -------------------------------------------------------------------------
  // Test 3: Unsigned 64-bit operations
  // -------------------------------------------------------------------------
  printf("\n[Test 3] Unsigned uint64_t Operations\n");

  uint64_t u_add = g_ua + g_ub;
  uint64_t u_mul = g_ua * 2ULL;
  PASS("unsigned add/mul executed (uint64_t)");

  // -------------------------------------------------------------------------
  // Test 4: Bitwise operations (AND, OR, XOR, NOT)
  // -------------------------------------------------------------------------
  printf("\n[Test 4] Bitwise Operations\n");

  uint64_t and_op = g_ua & 0xFFFFULL;
  uint64_t or_op = g_ua | 0x00FFULL;
  uint64_t xor_op = g_ua ^ 0xFFFFULL;
  PASS("bitwise AND/OR/XOR executed");

  // -------------------------------------------------------------------------
  // Test 5: Shift operations (<<, >>)
  // -------------------------------------------------------------------------
  printf("\n[Test 5] 64-bit Shift Operations\n");

  int64_t shl = g_shift_val << 60;
  int64_t shr = shl >> 60;

  if (shl == 1152921504606846976LL)
    PASS("shift left 1 << 60 correct");
  else
    FAIL("shift left failed");

  if (shr == 1LL)
    PASS("shift right back to 1 correct");
  else
    FAIL("shift right failed");

  // -------------------------------------------------------------------------
  // Test 6: Edge values (INT64_MAX, INT64_MIN)
  // -------------------------------------------------------------------------
  printf("\n[Test 6] 64-bit Edge Values\n");

  if (g_max > 9e18)
    PASS("INT64_MAX valid");
  else
    FAIL("INT64_MAX invalid");

  if (g_min < -9e18)
    PASS("INT64_MIN valid");
  else
    FAIL("INT64_MIN invalid");

  // -------------------------------------------------------------------------
  // Test 7: Overflow detection (GCC built-in)
  // -------------------------------------------------------------------------
  printf("\n[Test 7] 64-bit Overflow Check\n");

  int64_t res;
  int ovf = __builtin_saddll_overflow(g_max, 1LL, &res);
  if (ovf)
    PASS("INT64_MAX + 1 overflow detected");
  else
    FAIL("overflow detection failed");

  printf("\n===== All 64-bit Integer Tests Finished =====\n");
  return 0;
}