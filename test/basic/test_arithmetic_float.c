//
// Created by zhangjiantao on 26-5-10.
//

#include <dev/console.h>

#include <float.h>
#include <math.h>
#include <stdio.h>

// Test result macros
#define PASS(fmt, ...)                                                         \
  do {                                                                         \
    tcm_console_set_color(0, 0, 0, 0, 0xff, 0);                                \
    printf("[PASS] " fmt "\n", ##__VA_ARGS__);                                 \
    tcm_console_set_color(0, 0, 0, 0xff, 0xff, 0xff);                          \
  } while (0)
#define FAIL(fmt, ...)                                                         \
  do {                                                                         \
    tcm_console_set_color(0, 0, 0, 0xff, 0, 0);                                \
    printf("[FAIL] " fmt "\n", ##__VA_ARGS__);                                 \
    tcm_console_set_color(0, 0, 0, 0xff, 0xff, 0xff);                          \
  } while (0)

// Floating-point comparison with epsilon tolerance
#define EPS 1e-9
static int is_equal(double a, double b) { return fabs(a - b) < EPS; }

// ==============================================
// Volatile global variables (prevent compiler optimization)
// These values CANNOT be folded or optimized away
// ==============================================
volatile static double g_a = 10.5;
volatile static double g_b = 2.0;
volatile static double g_val = 2.7;
volatile static double g_sqrt_input = 16.0;
volatile static double g_pow_base = 2.0;
volatile static double g_pow_exp = 8.0;
volatile static double g_neg_num = -3.14;
volatile static double g_zero = 0.0;
volatile static double g_one = 1.0;

volatile static float g_flt_min = FLT_MIN;
volatile static double g_dbl_max = DBL_MAX;

#ifdef TESTALL
int test_arithmetic_float_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
  printf("===== Floating-Point Arithmetic Validation Test =====\n");

  // ========================================================================
  // Test 1: Basic floating-point arithmetic (add, sub, mul, div)
  // ========================================================================
  printf("[Test 1] Basic Arithmetic Operations\n");

  double add_res = g_a + g_b;
  double sub_res = g_a - g_b;
  double mul_res = g_a * g_b;
  double div_res = g_a / g_b;

  if (is_equal(add_res, 12.5))
    PASS("g_a + g_b = 12.5");
  else
    FAIL("Addition failed");

  if (is_equal(sub_res, 8.5))
    PASS("g_a - g_b = 8.5");
  else
    FAIL("Subtraction failed");

  if (is_equal(mul_res, 21.0))
    PASS("g_a * g_b = 21.0");
  else
    FAIL("Multiplication failed");

  if (is_equal(div_res, 5.25))
    PASS("g_a / g_b = 5.25");
  else
    FAIL("Division failed");

  // ========================================================================
  // Test 2: Standard math functions
  // ========================================================================
  printf("\n[Test 2] Standard Math Library Functions\n");

  double sqrt_res = sqrt(g_sqrt_input);
  double pow_res = pow(g_pow_base, g_pow_exp);
  double exp_res = exp(g_zero);
  double log_res = log(g_one);
  double fabs_res = fabs(g_neg_num);

  if (is_equal(sqrt_res, 4.0))
    PASS("sqrt(16.0) = 4.0");
  else
    FAIL("sqrt failed");

  if (is_equal(pow_res, 256.0))
    PASS("pow(2,8) = 256.0");
  else
    FAIL("pow failed");

  if (is_equal(exp_res, 1.0))
    PASS("exp(0) = 1.0");
  else
    FAIL("exp failed");

  if (is_equal(log_res, 0.0))
    PASS("log(1) = 0.0");
  else
    FAIL("log failed");

  if (is_equal(fabs_res, 3.14))
    PASS("fabs(-3.14) = 3.14");
  else
    FAIL("fabs failed");

  // ========================================================================
  // Test 3: Rounding functions
  // ========================================================================
  printf("\n[Test 3] Rounding Operations\n");

  double ceil_res = ceil(g_val);
  double floor_res = floor(g_val);
  double round_res = round(g_val);

  if (is_equal(ceil_res, 3.0))
    PASS("ceil(2.7) = 3.0");
  else
    FAIL("ceil failed");

  if (is_equal(floor_res, 2.0))
    PASS("floor(2.7) = 2.0");
  else
    FAIL("floor failed");

  if (is_equal(round_res, 3.0))
    PASS("round(2.7) = 3.0");
  else
    FAIL("round failed");

  // ========================================================================
  // Test 4: Special float values (Infinity, NaN)
  // ========================================================================
  printf("\n[Test 4] Special Floating-Point Values\n");

  double inf_res = g_one / g_zero;
  double nan_res = g_zero / g_zero;

  if (isinf(inf_res))
    PASS("1.0 / 0.0 = Infinity");
  else
    FAIL("Infinity test failed");

  if (isnan(nan_res))
    PASS("0.0 / 0.0 = NaN");
  else
    FAIL("NaN test failed");

  // ========================================================================
  // Test 5: Floating-point limits validation
  // ========================================================================
  printf("\n[Test 5] Floating-Point Limits Validation\n");

  if ((float)g_flt_min > 0.0f)
    PASS("FLT_MIN is positive and valid");
  else
    FAIL("FLT_MIN invalid");

  if (g_dbl_max > 1e300)
    PASS("DBL_MAX is valid");
  else
    FAIL("DBL_MAX invalid");

  printf("\n===== All Floating-Point Tests Finished =====\n");
  return 0;
}
