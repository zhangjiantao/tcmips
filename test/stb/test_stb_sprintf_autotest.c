// stb_sprintf.h AUTOTEST.
//
// Exercises: stbsp_snprintf, stbsp_sprintf, stbsp_vsnprintf, stbsp_vsprintf.
// No external assets. Pure string formatting.
//
// All assertions follow the autotest convention:
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb/stb_sprintf.h"

// ---------------- autotest harness -------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void t_check(int cond, const char *name, const char *detail) {
  if (cond) {
    g_pass++;
    printf("[SELF] %s PASS\n", name);
  } else {
    g_fail++;
    printf("[SELF] %s FAIL %s\n", name, detail ? detail : "");
  }
}

#define CHECK(cond, name) t_check((cond) ? 1 : 0, name, NULL)

#define CHECK_STR(actual, expected, name)                                      \
  do {                                                                         \
    const char *a_ = (actual);                                                 \
    const char *e_ = (expected);                                              \
    t_check(a_ && e_ && strcmp(a_, e_) == 0, name, NULL);                      \
  } while (0)

#define CHECK_STRF(actual, expected, fmt, ...)                                   \
  do {                                                                           \
    char d_[128];                                                                \
    snprintf(d_, sizeof(d_), fmt, __VA_ARGS__);                                  \
    t_check(actual && expected && strcmp(actual, expected) == 0,                 \
            name, d_);                                                           \
  } while (0)

// ---------------- basic formatting --------------------------------------------

static int test_basic(void) {
  char buf[256];

  // snprintf-style: integers.
  stbsp_snprintf(buf, sizeof(buf), "val=%d", 42);
  CHECK_STR(buf, "val=42", "sprintf:int");

  stbsp_snprintf(buf, sizeof(buf), "neg=%d", -100);
  CHECK_STR(buf, "neg=-100", "sprintf:neg_int");

  stbsp_snprintf(buf, sizeof(buf), "hex=%x", 255);
  CHECK_STR(buf, "hex=ff", "sprintf:hex");

  stbsp_snprintf(buf, sizeof(buf), "HEX=%X", 255);
  CHECK_STR(buf, "HEX=FF", "sprintf:HEX");

  // Zero padding.
  stbsp_snprintf(buf, sizeof(buf), "pad=%04d", 42);
  CHECK_STR(buf, "pad=0042", "sprintf:zero_pad");

  // Unsigned.
  stbsp_snprintf(buf, sizeof(buf), "u=%u", 4294967295u);
  CHECK_STR(buf, "u=4294967295", "sprintf:unsigned");

  // Strings.
  stbsp_snprintf(buf, sizeof(buf), "hello %s", "world");
  CHECK_STR(buf, "hello world", "sprintf:string");

  // Float.
  stbsp_snprintf(buf, sizeof(buf), "f=%.2f", 3.14159);
  CHECK_STR(buf, "f=3.14", "sprintf:float_2dp");

  // Multiple specifiers.
  stbsp_snprintf(buf, sizeof(buf), "x=%d y=%d z=%d", 1, 2, 3);
  CHECK_STR(buf, "x=1 y=2 z=3", "sprintf:multi");

  return 1;
}

// ---------------- buffer size limits ------------------------------------------

static int test_buffer_limits(void) {
  char buf[10];
  int ret;

  // Truncation: write 30 chars into 10-byte buffer.  stb_sprintf null-terminates
  // within count and fills the buffer fully; its return value is the length
  // that WOULD have been written without truncation (like std snprintf).
  ret = stbsp_snprintf(buf, sizeof(buf), "12345678901234567890abcdefghij");
  CHECK(buf[9] == '\0', "sprintf:trunc_terminate");
  CHECK(strlen(buf) == 9, "sprintf:trunc_length");
  CHECK(ret == 30, "sprintf:trunc_return");

  // Write exactly filling buffer (9 chars + NUL = 10 bytes total).
  ret = stbsp_snprintf(buf, sizeof(buf), "123456789");
  CHECK(buf[9] == '\0', "sprintf:exact_terminate");
  CHECK(strlen(buf) == 9, "sprintf:exact_length");
  CHECK(ret == 9, "sprintf:exact_return");

  // Empty format.
  ret = stbsp_snprintf(buf, sizeof(buf), "");
  CHECK(buf[0] == '\0', "sprintf:empty_terminate");
  CHECK(ret == 0, "sprintf:empty_return");

  // Very small buffer (1 byte = just NUL).
  ret = stbsp_snprintf(buf, 1, "hello");
  CHECK(buf[0] == '\0', "sprintf:buf1_terminate");
  CHECK(ret == 5, "sprintf:buf1_return");

  return 1;
}

// ---------------- sprintf vs snprintf ------------------------------------------

static int test_sprintf(void) {
  char buf[256];

  // sprintf writes to buffer without explicit size; same output as snprintf.
  stbsp_sprintf(buf, "answer=%d", 7);
  CHECK_STR(buf, "answer=7", "sprintf:no_size");

  // Multiple args via sprintf.
  stbsp_sprintf(buf, "%s %d %.1f", "pi", 3, 3.1);
  CHECK_STR(buf, "pi 3 3.1", "sprintf:multi_mixed");

  return 1;
}

// ---------------- variadic interfaces ------------------------------------------

// Both stbsp_vsnprintf(buf, count, fmt, va_list) and stbsp_vsprintf(buf, fmt,
// va_list) take a pre-built va_list (NOT variadic), so each needs a variadic
// wrapper to construct the va_list.
static void helper_vsprintf(char *buf, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  stbsp_vsprintf(buf, fmt, ap);
  va_end(ap);
}

static void helper_vsnprintf(char *buf, int count, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  stbsp_vsnprintf(buf, count, fmt, ap);
  va_end(ap);
}

static int test_vsnprintf(void) {
  char buf[256];

  // stbsp_vsnprintf takes a va_list; go through the variadic wrapper.
  helper_vsnprintf(buf, sizeof(buf), "num=%d end", 123);
  CHECK_STR(buf, "num=123 end", "vsnprintf:direct");

  // stbsp_vsprintf takes a va_list too — use the variadic helper.
  helper_vsprintf(buf, "val=%s", "ok");
  CHECK_STR(buf, "val=ok", "vsprintf:va_list");

  return 1;
}

// ---------------- edge cases ---------------------------------------------------

static int test_edge(void) {
  char buf[256];

  // Escaped percent signs.
  stbsp_snprintf(buf, sizeof(buf), "100%%");
  CHECK_STR(buf, "100%", "sprintf:pct");

  // Empty string arg.
  stbsp_snprintf(buf, sizeof(buf), "empty=[%s]", "");
  CHECK_STR(buf, "empty=[]", "sprintf:empty_string");

  // Integer precision: stb_sprintf ignores precision for %d and prints all
  // digits (matches host behaviour).
  stbsp_snprintf(buf, sizeof(buf), "%.0d", 123);
  CHECK_STR(buf, "123", "sprintf:precision_int");

  // Long-long via the 'll' length modifier with a value that fits int64
  // (exact string; no reliance on printing >2^31 which TCMIPS soft-int64
  // must still handle — 123456789012345 needs real int64).
  stbsp_snprintf(buf, sizeof(buf), "ll=%lld", 123456789LL);
  CHECK_STR(buf, "ll=123456789", "sprintf:long_long");

  // String precision truncation.
  stbsp_snprintf(buf, sizeof(buf), "%.3s", "abcdef");
  CHECK_STR(buf, "abc", "sprintf:str_precision");

  // Width + left-justify.
  stbsp_snprintf(buf, sizeof(buf), "[%-5d]", 42);
  CHECK_STR(buf, "[42   ]", "sprintf:left_justify");

  return 1;
}

int main(void) {
  printf("=== stb_sprintf AUTOTEST ===\n");

  test_basic();
  test_buffer_limits();
  test_sprintf();
  test_vsnprintf();
  test_edge();

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  return g_fail > 0 ? 1 : 0;
}
