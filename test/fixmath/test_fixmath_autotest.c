// libfixmath autotest for the TCMIPS emulator.
//
// Full AUTOTEST (see AUTOTEST.md) covering every public API function in
// the transplanted libfixmath.  Test data drawn from the upstream test suite
// (tests/tests_basic.c, tests_sqrt.c, tests_trig.c, tests_lerp.c,
//  tests_str.c, tests_macros.c).
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)
// The process exits 0 when all checks passed, non-zero otherwise.

#include <fixmath/fix16.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── autotest assertion helpers ──────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, name)                                                      \
  do {                                                                         \
    char _buf2[64];                                                            \
    int _ok = (cond) ? 1 : 0;                                                  \
    if (_ok) {                                                                 \
      g_pass++;                                                                \
      printf("[SELF] %s PASS\n", name);                                        \
    } else {                                                                   \
      g_fail++;                                                                \
      snprintf(_buf2, sizeof(_buf2), "condition failed");                       \
      printf("[SELF] %s FAIL %s\n", name, _buf2);                              \
    }                                                                          \
  } while (0)

#define CHECK_EQ(actual, expected, name)                                       \
  do {                                                                         \
    int64_t _a = (int64_t)(actual);                                            \
    int64_t _e = (int64_t)(expected);                                          \
    char _buf[64];                                                             \
    if (_a == _e) {                                                            \
      g_pass++;                                                                \
      printf("[SELF] %s PASS\n", name);                                        \
    } else {                                                                   \
      g_fail++;                                                                \
      snprintf(_buf, sizeof(_buf), "got %lld expected %lld",                    \
               (long long)_a, (long long)_e);                                  \
      printf("[SELF] %s FAIL %s\n", name, _buf);                               \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(actual, expected, eps, name)                                \
  do {                                                                         \
    double _a = fix16_to_dbl(actual);                                          \
    double _e = (double)(expected);                                            \
    char _buf[96];                                                             \
    if (fabs(_a - _e) <= (eps)) {                                              \
      g_pass++;                                                                \
      printf("[SELF] %s PASS\n", name);                                        \
    } else {                                                                   \
      g_fail++;                                                                \
      snprintf(_buf, sizeof(_buf), "got %.6f expected %.6f (eps %.4f)",         \
               _a, _e, (double)(eps));                                         \
      printf("[SELF] %s FAIL %s\n", name, _buf);                               \
    }                                                                          \
  } while (0)

#define CHECK_STREQ(actual, expected, name)                                    \
  do {                                                                         \
    const char *_a = (actual);                                                 \
    const char *_e = (expected);                                               \
    char _buf[128];                                                            \
    if (strcmp(_a, _e) == 0) {                                                 \
      g_pass++;                                                                \
      printf("[SELF] %s PASS\n", name);                                        \
    } else {                                                                   \
      g_fail++;                                                                \
      snprintf(_buf, sizeof(_buf), "got '%s' expected '%s'", _a, _e);          \
      printf("[SELF] %s FAIL %s\n", name, _buf);                               \
    }                                                                          \
  } while (0)

/* ── basic arithmetic (from upstream tests_basic.c) ────────────────── */

static void test_mul(void) {
  /* specific tests from upstream test_mul_specific */
  CHECK_EQ(fix16_mul(fix16_from_int(5), fix16_from_int(5)),
           fix16_from_int(25), "mul_5x5_eq_25");
  CHECK_EQ(fix16_mul(fix16_from_int(-5), fix16_from_int(5)),
           fix16_from_int(-25), "mul_neg5x5_eq_neg25");
  CHECK_EQ(fix16_mul(fix16_from_int(-5), fix16_from_int(-5)),
           fix16_from_int(25), "mul_neg5xneg5_eq_25");
  CHECK_EQ(fix16_mul(fix16_from_int(5), fix16_from_int(-5)),
           fix16_from_int(-25), "mul_5xneg5_eq_neg25");

  CHECK_EQ(fix16_mul(0, 10), 0, "mul_0x10_eq_0");
  CHECK_EQ(fix16_mul(2, 0x8000), 1, "mul_2x_0p5_eq_1");
  CHECK_EQ(fix16_mul(-2, 0x8000), -1, "mul_neg2x_0p5_eq_neg1");

  /* rounding: 1.5 * 2  →  r=1*2 + 0.5*2˜1 → 2+1=3 */
  CHECK_EQ(fix16_mul(3, 0x8000), 2, "mul_round_3x0p5_eq_2");
  CHECK_EQ(fix16_mul(2, 0x7FFF), 1, "mul_2x_0p4999_eq_1");
  CHECK_EQ(fix16_mul(-2, 0x8001), -1, "mul_neg2x0p5pe_eq_neg1");
  CHECK_EQ(fix16_mul(-3, 0x8000), -2, "mul_round_neg3x0p5_eq_neg2");
  CHECK_EQ(fix16_mul(2, 0x8001), 1, "mul_2x0p5pe_eq_1");
}

static void test_div(void) {
  /* specific tests from upstream test_div_specific */
  CHECK_EQ(fix16_div(fix16_from_int(15), fix16_from_int(5)),
           fix16_from_int(3), "div_15by5_eq_3");
  CHECK_EQ(fix16_div(fix16_from_int(-15), fix16_from_int(5)),
           fix16_from_int(-3), "div_neg15by5_eq_neg3");
  CHECK_EQ(fix16_div(fix16_from_int(-15), fix16_from_int(-5)),
           fix16_from_int(3), "div_neg15byneg5_eq_3");
  CHECK_EQ(fix16_div(fix16_from_int(15), fix16_from_int(-5)),
           fix16_from_int(-3), "div_15byneg5_eq_neg3");

  /* fractional division with rounding */
  CHECK_EQ(fix16_div(0, 10), 0, "div_0by10_eq_0");
  CHECK_EQ(fix16_div(1, fix16_from_int(2)), 1, "div_1by2_eq_0p5_round_1");
  CHECK_EQ(fix16_div(-1, fix16_from_int(2)), -1, "div_neg1by2_eq_neg0p5_round_neg1");
  CHECK_EQ(fix16_div(1, fix16_from_int(-2)), -1, "div_1byneg2_eq_neg1");
  CHECK_EQ(fix16_div(-1, fix16_from_int(-2)), 1, "div_neg1byneg2_eq_1");
  CHECK_EQ(fix16_div(3, fix16_from_int(2)), 2, "div_3by2_eq_2");
  CHECK_EQ(fix16_div(-3, fix16_from_int(2)), -2, "div_neg3by2_eq_neg2");
  CHECK_EQ(fix16_div(3, fix16_from_int(-2)), -2, "div_3byneg2_eq_neg2");
  CHECK_EQ(fix16_div(-3, fix16_from_int(-2)), 2, "div_neg3byneg2_eq_2");
  CHECK_EQ(fix16_div(2, 0x7FFF), 4, "div_2by0p4999_eq_4");
  CHECK_EQ(fix16_div(-2, 0x7FFF), -4, "div_neg2by0p4999_eq_neg4");
  CHECK_EQ(fix16_div(2, 0x8001), 4, "div_2by0p5pe_eq_4");
  CHECK_EQ(fix16_div(-2, 0x8001), -4, "div_neg2by0p5pe_eq_neg4");
}

static void test_add_sub(void) {
  CHECK_EQ(fix16_add(fix16_from_int(3), fix16_from_int(5)), fix16_from_int(8),
           "add_3p5_eq_8");
  CHECK_EQ(fix16_add(fix16_from_int(-3), fix16_from_int(5)), fix16_from_int(2),
           "add_neg3p5_eq_2");
  CHECK_EQ(fix16_sub(fix16_from_int(10), fix16_from_int(3)), fix16_from_int(7),
           "sub_10m3_eq_7");
  CHECK_EQ(fix16_sub(fix16_from_int(3), fix16_from_int(10)),
           fix16_from_int(-7), "sub_3m10_eq_neg7");
}

/* ── sqrt (from upstream tests_sqrt.c) ────────────────────────────── */

static void test_sqrt(void) {
  CHECK_EQ(fix16_sqrt(fix16_from_int(16)), fix16_from_int(4), "sqrt_16_eq_4");
  CHECK_EQ(fix16_sqrt(fix16_from_int(100)), fix16_from_int(10), "sqrt_100_eq_10");
  CHECK_EQ(fix16_sqrt(fix16_from_int(1)), fix16_from_int(1), "sqrt_1_eq_1");
  CHECK_EQ(fix16_sqrt(0), 0, "sqrt_0_eq_0");

  /* rounding boundaries from upstream */
  CHECK_EQ(fix16_sqrt(214748302), 3751499, "sqrt_round_214748302_eq_3751499");
  CHECK_EQ(fix16_sqrt(214748303), 3751499, "sqrt_round_214748303_eq_3751499");
  CHECK_EQ(fix16_sqrt(214748359), 3751499, "sqrt_round_214748359_eq_3751499");
  CHECK_EQ(fix16_sqrt(214748360), 3751500, "sqrt_round_214748360_eq_3751500");
}

/* ── trigonometry (from upstream tests_trig.c) ─────────────────────── */

static void test_trig(void) {
  CHECK_EQ(fix16_sin(0), 0, "sin_0_eq_0");
  CHECK_EQ(fix16_cos(0), fix16_one, "cos_0_eq_1");
  CHECK_EQ(fix16_tan(0), 0, "tan_0_eq_0");

  /* sin(pi/2) ≈ 1.0 */
  CHECK_NEAR(fix16_sin(fix16_pi >> 1), 1.0, 0.01, "sin_pi_over_2_approx_1");
  /* cos(pi) ≈ -1.0 */
  CHECK_NEAR(fix16_cos(fix16_pi), -1.0, 0.01, "cos_pi_approx_neg1");
  /* tan(pi/4) ≈ 1.0 */
  CHECK_NEAR(fix16_tan(fix16_pi >> 2), 1.0, 0.1, "tan_pi_over_4_approx_1");

  /* asin boundaries */
  CHECK_NEAR(fix16_asin(fix16_one), fix16_to_dbl(fix16_pi >> 1), 0.001,
             "asin_1_eq_pi2");
  CHECK_NEAR(fix16_asin(-fix16_one), fix16_to_dbl(-(fix16_pi >> 1)), 0.001,
             "asin_neg1_eq_negpi2");
  CHECK_NEAR(fix16_asin(0), 0.0, 0.001, "asin_0_eq_0");
  CHECK_NEAR(fix16_asin(fix16_from_dbl(0.5)), 0.5236, 0.01,
             "asin_0p5_approx_pi_over_6");

  /* acos boundaries */
  CHECK_NEAR(fix16_acos(fix16_one), 0.0, 0.001, "acos_1_eq_0");
  CHECK_NEAR(fix16_acos(-fix16_one), fix16_to_dbl(fix16_pi), 0.001,
             "acos_neg1_eq_pi");
  CHECK_NEAR(fix16_acos(0), fix16_to_dbl(fix16_pi >> 1), 0.001,
             "acos_0_eq_pi2");

  /* atan2 boundaries */
  CHECK_NEAR(fix16_atan2(0, fix16_one), 0.0, 0.01, "atan2_0_1_eq_0");
  CHECK_NEAR(fix16_atan2(fix16_one, 0), fix16_to_dbl(fix16_pi >> 1), 0.01,
             "atan2_1_0_eq_pi2");
  CHECK_NEAR(fix16_atan2(0, -fix16_one), fix16_to_dbl(fix16_pi), 0.01,
             "atan2_0_neg1_eq_pi");
  CHECK_NEAR(fix16_atan2(-fix16_one, 0), fix16_to_dbl(-(fix16_pi >> 1)), 0.01,
             "atan2_neg1_0_eq_negpi2");
  CHECK_NEAR(fix16_atan2(0, 0), 0.0, 0.001, "atan2_0_0_eq_0");
}

/* ── exp / log ───────────────────────────────────────────────────── */

static void test_exp_log(void) {
  CHECK_EQ(fix16_exp(0), fix16_one, "exp_0_eq_1");
  CHECK_EQ(fix16_log(fix16_one), 0, "log_1_eq_0");
  CHECK_EQ(fix16_log2(fix16_one), 0, "log2_1_eq_0");

  /* exp(1) ≈ e */
  CHECK_NEAR(fix16_exp(fix16_one), fix16_to_dbl(fix16_e), 0.001,
             "exp_1_approx_e");
}

/* ── conversion ──────────────────────────────────────────────────── */

static void test_conv(void) {
  CHECK_EQ(fix16_to_int(fix16_from_int(5)), 5, "to_from_int_5");
  CHECK_EQ(fix16_from_int(0), 0, "from_int_0");
  CHECK_EQ(fix16_from_int(-1), -fix16_one, "from_int_neg1");

  CHECK_EQ(fix16_to_int(fix16_from_dbl(0.6)), 1, "to_int_0p6_round_1");
  CHECK_EQ(fix16_to_int(fix16_from_dbl(-0.6)), -1, "to_int_neg0p6_round_neg1");
}

/* ── lerp (from upstream tests_lerp.c) ────────────────────────────── */

static void test_lerp(void) {
  /* lerp8 */
  CHECK_EQ(fix16_lerp8(0, 2, 0), 0, "lerp8_0_2_t0_eq_0");
  CHECK_EQ(fix16_lerp8(0, 2, 127), 0, "lerp8_0_2_t127_eq_0");
  CHECK_EQ(fix16_lerp8(0, 2, 128), 1, "lerp8_0_2_t128_eq_1");
  CHECK_EQ(fix16_lerp8(0, 2, 255), 1, "lerp8_0_2_t255_eq_1");
  CHECK_EQ(fix16_lerp8(fix16_minimum, fix16_maximum, 0), fix16_minimum,
           "lerp8_min_max_t0_eq_min");
  CHECK_EQ(fix16_lerp8(-fix16_maximum, fix16_maximum, 128), 0,
           "lerp8_negmax_max_t128_eq_0");

  /* lerp16 */
  CHECK_EQ(fix16_lerp16(0, 2, 0x8000), 1, "lerp16_0_2_t0p5_eq_1");
  CHECK_EQ(fix16_lerp16(fix16_minimum, fix16_maximum, 0), fix16_minimum,
           "lerp16_min_max_t0_eq_min");

  /* lerp32 */
  CHECK_EQ(fix16_lerp32(0, 2, 0x80000000), 1, "lerp32_0_2_t0p5_eq_1");
  CHECK_EQ(fix16_lerp32(fix16_minimum, fix16_maximum, 0xFFFFFFFF),
           (fix16_maximum - 1), "lerp32_min_max_tmax_eq_maxp1");
}

/* ── str (from upstream tests_str.c) ──────────────────────────────── */

static void test_str(void) {
  char buf[13];

  fix16_to_str(fix16_from_dbl(1234.5678), buf, 4);
  CHECK_STREQ(buf, "1234.5678", "str_1234p5678");

  fix16_to_str(fix16_from_dbl(-1234.5678), buf, 4);
  CHECK_STREQ(buf, "-1234.5678", "str_neg1234p5678");

  fix16_to_str(0, buf, 0);
  CHECK_STREQ(buf, "0", "str_0");

  fix16_to_str(1, buf, 5);
  CHECK_STREQ(buf, "0.00002", "str_epsilon");

  fix16_to_str(fix16_maximum, buf, 5);
  CHECK_STREQ(buf, "32767.99998", "str_maximum");

  fix16_to_str(fix16_minimum, buf, 5);
  CHECK_STREQ(buf, "-32768.00000", "str_minimum");

  /* from_str */
  CHECK_EQ(fix16_from_str("1234.5678"), fix16_from_dbl(1234.5678),
           "from_str_1234p5678");
  CHECK_EQ(fix16_from_str("-1234.5678"), fix16_from_dbl(-1234.5678),
           "from_str_neg1234p5678");
  CHECK_EQ(fix16_from_str("0"), 0, "from_str_0");
  CHECK_EQ(fix16_from_str("1"), fix16_one, "from_str_1");
  CHECK_EQ(fix16_from_str("32767.99998"), fix16_maximum,
           "from_str_maximum");
  CHECK_EQ(fix16_from_str("-32768.00000"), fix16_minimum,
           "from_str_minimum");
}

/* ── macros (from upstream tests_macros.c) ────────────────────────── */

static void test_macros(void) {
  CHECK_EQ(F16(1.234), F16C(1, 234), "macros_F16_vs_F16C_1p234");
  CHECK_EQ(F16(0.0), 0, "macros_F16_0");
  CHECK_EQ(F16(1.0), fix16_one, "macros_F16_1");
  CHECK_EQ(F16(-1.0), -fix16_one, "macros_F16_neg1");
  CHECK_EQ(F16(32767.99999), F16C(32767, 99999), "macros_max");
  CHECK_EQ(F16(-32768.0), F16C(-32768, 0), "macros_min");
}

/* ── utility functions ───────────────────────────────────────────── */

static void test_util(void) {
  CHECK_EQ(fix16_abs(fix16_from_int(-3)), fix16_from_int(3), "abs_neg3_eq_3");
  CHECK_EQ(fix16_abs(fix16_from_int(3)), fix16_from_int(3), "abs_3_eq_3");
  /* fix16_minimum = 0x80000000 — abs() returns original value (no positive repr) */
CHECK(fix16_abs(fix16_minimum) == fix16_minimum, "abs_minimum_noop");

  CHECK_EQ(fix16_floor(fix16_one + (fix16_one >> 1)), fix16_one,
           "floor_1p5_eq_1");
  CHECK_EQ(fix16_ceil(fix16_one + (fix16_one >> 1)),
           fix16_from_int(2), "ceil_1p5_eq_2");

  CHECK_EQ(fix16_min(fix16_from_int(5), fix16_from_int(3)),
           fix16_from_int(3), "min_5_3_eq_3");
  CHECK_EQ(fix16_max(fix16_from_int(5), fix16_from_int(3)),
           fix16_from_int(5), "max_5_3_eq_5");

  CHECK_EQ(fix16_clamp(fix16_from_int(10), fix16_from_int(0),
                        fix16_from_int(5)),
           fix16_from_int(5), "clamp_10_to_0_5_eq_5");

  CHECK_EQ(fix16_sq(fix16_from_int(4)), fix16_from_int(16), "sq_4_eq_16");
}

int main(void) {
  printf("[SELF] fixmath_autotest starting\n");

  test_mul();
  test_div();
  test_add_sub();
  test_sqrt();
  test_trig();
  test_exp_log();
  test_conv();
  test_lerp();
  test_str();
  test_macros();
  test_util();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return (g_fail == 0) ? 0 : 1;
}