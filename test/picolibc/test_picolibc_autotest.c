// picolibc autotest for the TCMIPS emulator.
//
// This is a full AUTOTEST (see AUTOTEST.md) for the picolibc standard C library
// as linked into the tcmips_core runtime. It exercises a broad slice of the
// libc surface — <string.h>, <strings.h>, <ctype.h>, <stdio.h>, <stdlib.h>,
// <math.h>, <setjmp.h>, <time.h>, <errno.h>, <inttypes.h>, ... — entirely
// in-guest, without relying on a backing filesystem (there is no --tcm-memfs
// payload here; stdio paths are exercised through fmemopen / stdin/stdout /
// stderr and the sprintf family).
//
// The harness mirrors test_memfs_autotest.c: every assertion is emitted as a
// single greppable line
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)
// and the process exits with code 0 when every check passed, non-zero
// otherwise. The emulator reports that code as "!! TCMIPS EMULATOR EXIT, code=N
// !!".
//
// Portability notes:
//  * picolibc is built with its default "double" printf/scanf variant, so %f,
//    %g, %e and the long-long / intmax_t / size_t specifiers are available.
//  * NaN / infinity / sign-of-zero are asserted by classification only, never
//    by comparing formatted strings, so the checks are immune to tinystdio's
//    exact float formatting.
//  * errno-dependent side effects (e.g. whether log(-1) sets EDOM) are only
//    asserted loosely, since picolibc's errno wiring is configurable.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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

#define CHECK_EQ(actual, expected, name)                                       \
  do {                                                                         \
    long long a_ = (long long)(actual);                                        \
    long long e_ = (long long)(expected);                                      \
    char d_[80];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)

#define CHECK_STREQ(actual, expected, name)                                    \
  do {                                                                         \
    const char *a_ = (actual);                                                 \
    const char *e_ = (expected);                                               \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=\"%s\" expected=\"%s\")",                \
             a_ ? a_ : "(null)", e_ ? e_ : "(null)");                          \
    t_check(a_ && e_ && strcmp(a_, e_) == 0, name, d_);                        \
  } while (0)

#define CHECK_NEAR(actual, expected, eps, name)                                \
  do {                                                                         \
    double a_ = (double)(actual);                                              \
    double e_ = (double)(expected);                                            \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%.17g expected=%.17g)", a_, e_);         \
    t_check(float_near(a_, e_, (double)(eps)), name, d_);                      \
  } while (0)

static int float_near(double a, double b, double eps) {
  double diff = fabs(a - b);
  double scale = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
  return diff <= eps * (1.0 + scale);
}

// Forward declaration so call sites can precede the definition.
static int test_vasprintf_impl(char **out, int *len, const char *fmt, ...);

// ============================================================================
// 1. <string.h> core: length / copy / compare
// ============================================================================
static void test_string_core(void) {
  const char *s = "hello, world";
  CHECK_EQ(strlen(s), 12, "strlen basic");
  CHECK_EQ(strlen(""), 0, "strlen empty");

  CHECK_EQ(strcmp("abc", "abc"), 0, "strcmp equal");
  CHECK(strcmp("abc", "abd") < 0, "strcmp less");
  CHECK(strcmp("abd", "abc") > 0, "strcmp greater");
  CHECK_EQ(strncmp("abcdef", "abcxyz", 3), 0, "strncmp prefix");

  char buf[64];
  strcpy(buf, "copy me");
  CHECK_STREQ(buf, "copy me", "strcpy");

  strncpy(buf, "abcdefghij", 5);
  buf[5] = '\0';
  CHECK_STREQ(buf, "abcde", "strncpy");

  strcat(buf, "XYZ");
  CHECK_STREQ(buf, "abcdeXYZ", "strcat");

  strncat(buf, "123456789", 3);
  CHECK_STREQ(buf, "abcdeXYZ123", "strncat");
}

// ============================================================================
// 2. <string.h> search / char funcs
// ============================================================================
static void test_string_search(void) {
  const char *hay = "the quick brown fox";

  CHECK_STREQ(strchr(hay, 'q'), "quick brown fox", "strchr");
  CHECK(strchr(hay, 'z') == NULL, "strchr miss");

  CHECK_STREQ(strrchr(hay, 'o'), "ox", "strrchr");
  CHECK(strrchr(hay, 'z') == NULL, "strrchr miss");

  CHECK_STREQ(strstr(hay, "brown"), "brown fox", "strstr");
  CHECK(strstr(hay, "missing") == NULL, "strstr miss");

  CHECK_EQ(strspn("123abc", "1234567890"), 3, "strspn");
  CHECK_EQ(strcspn("123abc", "abc"), 3, "strcspn");

  char *p = strpbrk("abcdef", "xyde");
  CHECK(p != NULL && *p == 'd', "strpbrk");

  CHECK(memchr("abcXYZ", 'X', 6) != NULL, "memchr hit");
  CHECK(memchr("abcXYZ", 'q', 6) == NULL, "memchr miss");
}

// ============================================================================
// 3. <string.h> memory block
// ============================================================================
static void test_memory(void) {
  int a[5] = {1, 2, 3, 4, 5};
  int b[5];

  memcpy(b, a, sizeof(a));
  CHECK(memcmp(a, b, sizeof(a)) == 0, "memcpy roundtrip");

  memset(b, 0xAA, sizeof(b));
  unsigned char *cb = (unsigned char *)b;
  CHECK(cb[0] == 0xAA && cb[sizeof(b) - 1] == 0xAA, "memset fill");

  char ov[16] = "abcdefgh";
  memmove(ov + 2, ov, 6);
  CHECK(memcmp(ov, "ababcdef", 8) == 0, "memmove overlap fwd");

  char ov2[16] = "abcdefgh";
  memmove(ov2, ov2 + 2, 6);
  CHECK(memcmp(ov2, "cdefgh", 6) == 0, "memmove overlap back");
}

// ============================================================================
// 4. <strings.h> (BSD): strcasecmp / strncasecmp / ffs
// ============================================================================
static void test_strings_h(void) {
  CHECK_EQ(strcasecmp("Hello", "hELLO"), 0, "strcasecmp equal");
  CHECK(strcasecmp("abc", "abd") < 0, "strcasecmp less");

  CHECK_EQ(strncasecmp("AbCdEf", "aBcXyZ", 3), 0, "strncasecmp prefix");
  CHECK(strncasecmp("AbCdEf", "aBcXyZ", 4) < 0, "strncasecmp diff");

  CHECK_EQ(ffs(0), 0, "ffs zero");
  CHECK_EQ(ffs(1), 1, "ffs lsb");
  CHECK_EQ(ffs(8), 4, "ffs bit3");
  CHECK_EQ(strcasecmp("", ""), 0, "strcasecmp empty");
}

// ============================================================================
// 5. <ctype.h> character classification / mapping
// ============================================================================
static void test_ctype(void) {
  CHECK(isalpha('A') && isalpha('z'), "isalpha letters");
  CHECK(!isalpha('5'), "isalpha digit rejected");

  CHECK(isdigit('0') && isdigit('9'), "isdigit digits");
  CHECK(!isdigit('x'), "isdigit non-digit rejected");

  CHECK(isalnum('A') && isalnum('7') && !isalnum('!'), "isalnum");

  CHECK(isspace(' ') && isspace('\n') && isspace('\t'), "isspace");
  CHECK(!isspace('a'), "isspace non-space rejected");

  CHECK(islower('a') && !islower('A'), "islower");
  CHECK(isupper('A') && !isupper('a'), "isupper");

  CHECK(isxdigit('F') && isxdigit('f') && !isxdigit('G'), "isxdigit");

  CHECK(isprint('a') && !isprint('\n'), "isprint");
  CHECK(iscntrl('\n') && !iscntrl('a'), "iscntrl");

  CHECK_EQ(tolower('A'), 'a', "tolower");
  CHECK_EQ(tolower('z'), 'z', "tolower identity");
  CHECK_EQ(toupper('a'), 'A', "toupper");
  CHECK_EQ(toupper('7'), '7', "toupper digit unchanged");
}

// ============================================================================
// 6. <stdio.h> sprintf / snprintf formatting
// ============================================================================
static void test_printf_formatting(void) {
  char buf[128];

  snprintf(buf, sizeof(buf), "%d", 42);
  CHECK_STREQ(buf, "42", "printf %d");

  snprintf(buf, sizeof(buf), "%x %X %o", 255, 255, 8);
  CHECK_STREQ(buf, "ff FF 10", "printf hex/oct");

  snprintf(buf, sizeof(buf), "%d should stay %d", -7, 7);
  CHECK_STREQ(buf, "-7 should stay 7", "printf mixed signed");

  snprintf(buf, sizeof(buf), "[%5d]", 42);
  CHECK_STREQ(buf, "[   42]", "printf width right");

  snprintf(buf, sizeof(buf), "[%-5d]", 42);
  CHECK_STREQ(buf, "[42   ]", "printf width left");

  snprintf(buf, sizeof(buf), "[%05d]", 42);
  CHECK_STREQ(buf, "[00042]", "printf zero pad");

  snprintf(buf, sizeof(buf), "%lld", 12345678901234LL);
  CHECK_STREQ(buf, "12345678901234", "printf %lld");

  snprintf(buf, sizeof(buf), "%llx", 0xabcdefLL);
  CHECK_STREQ(buf, "abcdef", "printf %llx");

  snprintf(buf, sizeof(buf), "%zu", (size_t)4200);
  CHECK_STREQ(buf, "4200", "printf %zu");

  snprintf(buf, sizeof(buf), "%c %s %%", 'x', "str");
  CHECK_STREQ(buf, "x str %", "printf %c/%s/%%");
}

// ============================================================================
// 7. <stdio.h> floating-point formatting
// ============================================================================
static void test_printf_float(void) {
  char buf[128];

  snprintf(buf, sizeof(buf), "%f", 3.25);
  CHECK_STREQ(buf, "3.250000", "printf %f default precision 6");

  snprintf(buf, sizeof(buf), "%.2f", 3.14159);
  CHECK_STREQ(buf, "3.14", "printf %.2f round");

  snprintf(buf, sizeof(buf), "%.1f", 2.45);
  CHECK_STREQ(buf, "2.5", "printf %.1f rounding");

  snprintf(buf, sizeof(buf), "%g", 1000000.0);
  CHECK_STREQ(buf, "1e+06", "printf %g exponent");

  snprintf(buf, sizeof(buf), "%g", 0.5);
  CHECK_STREQ(buf, "0.5", "printf %g decimal");

  snprintf(buf, sizeof(buf), "%e", 123.0);
  CHECK_STREQ(buf, "1.230000e+02", "printf %e");

  snprintf(buf, sizeof(buf), "%+.2f", 12.5);
  CHECK_STREQ(buf, "+12.50", "printf force sign");

  snprintf(buf, sizeof(buf), "%08.3f", 12.5);
  CHECK_STREQ(buf, "0012.500", "printf float zero pad");

  snprintf(buf, sizeof(buf), "%.0f", 2.99);
  CHECK_STREQ(buf, "3", "printf %.0f round up");

  snprintf(buf, sizeof(buf), "%f", -0.5);
  CHECK_STREQ(buf, "-0.500000", "printf negative float");
}

// ============================================================================
// 8. <stdio.h> sscanf / vsscanf
// ============================================================================
static void test_scanf(void) {
  int i = 0;
  CHECK_EQ(sscanf("123", "%d", &i), 1, "sscanf %d count");
  CHECK_EQ(i, 123, "sscanf %d value");

  unsigned u = 0;
  sscanf("deadbeef", "%x", &u);
  CHECK_EQ(u, 0xdeadbeefu, "sscanf %x value");

  double d = 0.0;
  sscanf("3.5", "%lf", &d);
  CHECK_NEAR(d, 3.5, 1e-12, "sscanf %lf");

  int a = 0, b = 0;
  char s[32] = "";
  CHECK_EQ(sscanf("42 17 hello", "%d %d %31s", &a, &b, s), 3, "sscanf multi count");
  CHECK_EQ(a, 42, "sscanf multi a");
  CHECK_EQ(b, 17, "sscanf multi b");
  CHECK_STREQ(s, "hello", "sscanf multi str");

  int off = 0;
  CHECK_EQ(sscanf("key=value", "key=%31s%n", s, &off), 1, "sscanf literal count");
  CHECK_STREQ(s, "value", "sscanf literal value");
  CHECK_EQ(off, 9, "sscanf %n offset");

  long long ll = 0;
  sscanf("-12345678901234", "%lld", &ll);
  CHECK_EQ(ll, -12345678901234LL, "sscanf %lld");
}

// ============================================================================
// 9. <stdio.h> asprintf / vasprintf
// ============================================================================
static int test_vasprintf_impl(char **out, int *len, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vasprintf(out, fmt, ap);
  va_end(ap);
  if (len)
    *len = n;
  return n;
}

static void test_asprintf(void) {
  char *out = NULL;
  int n = asprintf(&out, "%d-%s-%c", 7, "mid", 'Z');
  CHECK(n > 0 && out != NULL, "asprintf success");
  if (out) {
    CHECK_STREQ(out, "7-mid-Z", "asprintf content");
    CHECK_EQ((int)strlen(out), n, "asprintf length matches");
    free(out);
  }

  out = NULL;
  n = -1;
  n = test_vasprintf_impl(&out, &n, "n=%d", 99);
  CHECK(n > 0 && out != NULL, "vasprintf success");
  if (out) {
    CHECK_STREQ(out, "n=99", "vasprintf content");
    free(out);
  }
}

// ============================================================================
// 10. <stdio.h> fmemopen / open_memstream
// ============================================================================
static void test_memory_streams(void) {
  char buf[64];

  FILE *f = fmemopen(buf, sizeof(buf), "w");
  CHECK(f != NULL, "fmemopen w open");
  if (f) {
    int w = fprintf(f, "hello %d", 5);
    fflush(f);
    CHECK_STREQ(buf, "hello 5", "fmemopen w content");
    CHECK_EQ(w, 7, "fprintf return count");
    fclose(f);
  }

  const char *src = "read this";
  f = fmemopen((void *)src, strlen(src), "r");
  CHECK(f != NULL, "fmemopen r open");
  if (f) {
    char tmp[64] = {0};
    size_t got = fread(tmp, 1, sizeof(tmp) - 1, f);
    CHECK_STREQ(tmp, "read this", "fmemopen r content");
    CHECK_EQ(got, strlen(src), "fread return count");
    fclose(f);
  }

  // fmemopen read/write ("w+") supports read-back after writing.
  char rw[32] = {0};
  f = fmemopen(rw, sizeof(rw), "w+");
  CHECK(f != NULL, "fmemopen w+ open");
  if (f) {
    int w = fputs("roundtrip", f);
    CHECK(w >= 0, "fputs into w+ stream");
    fflush(f);
    rewind(f);
    CHECK_STREQ(rw, "roundtrip", "fmemopen w+ read-back");
    fclose(f);
  }
}

// ============================================================================
// 11. <stdio.h> stdio stream primitives
// ============================================================================
static void test_stdio_primitives(void) {
  const char *src = "line one\nline two\n";
  FILE *f = fmemopen((void *)src, strlen(src), "r");
  CHECK(f != NULL, "fmemopen r for primitives");
  if (f) {
    CHECK_EQ(fgetc(f), 'l', "fgetc first");
    char line[64];
    CHECK(fgets(line, sizeof(line), f) != NULL, "fgets line1");
    CHECK_STREQ(line, "ine one\n", "fgets line1 content");
    CHECK(fgets(line, sizeof(line), f) != NULL, "fgets line2");
    CHECK_STREQ(line, "line two\n", "fgets line2 content");
    CHECK(fgets(line, sizeof(line), f) == NULL, "fgets EOF");
    CHECK(feof(f) != 0, "feof set at end");
    fclose(f);
  }

  CHECK_EQ(fflush(stdout), 0, "fflush stdout");

  CHECK(stdout != NULL && stderr != NULL, "stdout/stderr exist");
  CHECK(stdout != stderr, "stdout != stderr");

  f = fmemopen((void *)src, strlen(src), "r");
  if (f) {
    int c = fgetc(f);
    CHECK_EQ(ungetc(c, f), c, "ungetc return");
    CHECK_EQ(fgetc(f), c, "ungetc re-read");
    fclose(f);
  }

  f = fmemopen((void *)src, strlen(src), "r");
  if (f) {
    char big[256];
    CHECK(setvbuf(f, big, _IONBF, 0) == 0, "setvbuf unbuffered");
    fclose(f);
  }
}

// ============================================================================
// 12. <stdio.h> getline / getdelim
// ============================================================================
static void test_getline(void) {
  const char *src = "alpha\nbeta\n";
  FILE *f = fmemopen((void *)src, strlen(src), "r");
  if (!f) {
    CHECK(0, "getline open");
    return;
  }
  char *line = NULL;
  size_t cap = 0;
  ssize_t n1 = getline(&line, &cap, f);
  CHECK_EQ(n1, 6, "getline line1 len");
  if (n1 >= 0)
    CHECK_STREQ(line, "alpha\n", "getline line1 content");
  ssize_t n2 = getline(&line, &cap, f);
  CHECK_EQ(n2, 5, "getline line2 len");
  if (n2 >= 0)
    CHECK_STREQ(line, "beta\n", "getline line2 content");
  ssize_t n3 = getline(&line, &cap, f);
  // picolibc's getdelim/getline return the accumulated count (0 here) rather
  // than -1 when EOF is reached with no further characters read.
  CHECK_EQ(n3, 0, "getline EOF returns 0");
  free(line);
  fclose(f);

  const char *src2 = "a:b:c";
  f = fmemopen((void *)src2, strlen(src2), "r");
  if (f) {
    char *l = NULL;
    size_t c = 0;
    ssize_t n = getdelim(&l, &c, ':', f);
    CHECK_EQ(n, 2, "getdelim len");
    if (n >= 0)
      CHECK_STREQ(l, "a:", "getdelim content");
    free(l);
    fclose(f);
  }
}

// ============================================================================
// 13. <stdlib.h> string -> number conversions
// ============================================================================
static void test_strto(void) {
  CHECK_EQ(atoi("  42"), 42, "atoi basic");
  CHECK_EQ(atoi("-17"), -17, "atoi negative");
  CHECK_EQ(atoi("0"), 0, "atoi zero");
  CHECK_EQ(atoi("12x9"), 12, "atoi stops");

  CHECK_EQ(atol("123456789"), 123456789L, "atol");
  CHECK_NEAR(atof("3.25"), 3.25, 1e-15, "atof");
  CHECK_NEAR(atof("-0.5"), -0.5, 1e-15, "atof negative");

  char *end = NULL;
  long v = strtol("   -42xyz", &end, 10);
  CHECK_EQ(v, -42, "strtol value");
  CHECK(end != NULL && *end == 'x', "strtol endptr");

  v = strtol("0x1F", &end, 0);
  CHECK_EQ(v, 31, "strtol base0 hex");
  v = strtol("ff", &end, 16);
  CHECK_EQ(v, 255, "strtol base16");

  unsigned long long uv = strtoull("18446744073709551615", &end, 10);
  CHECK_EQ(uv, ULLONG_MAX, "strtoull max");

  long long lv = strtoll("-9223372036854775807", &end, 10);
  CHECK_EQ(lv, -9223372036854775807LL, "strtoll negative");
  CHECK_EQ(*end, 0, "strtoll full consume");

  CHECK_NEAR(strtod("3.5e2", &end), 350.0, 1e-12, "strtod exponent");
}

// ============================================================================
// 14. <stdlib.h> heap allocation
// ============================================================================
static void test_malloc(void) {
  int *p = calloc(16, sizeof(int));
  CHECK(p != NULL, "calloc non-null");
  if (p) {
    int all_zero = 1;
    for (int k = 0; k < 16; k++)
      if (p[k] != 0)
        all_zero = 0;
    CHECK(all_zero, "calloc zeroed");
    free(p);
  }

  char *q = malloc(8);
  CHECK(q != NULL, "malloc non-null");
  if (q) {
    strcpy(q, "abcd");
    q = realloc(q, 32);
    CHECK(q != NULL, "realloc non-null");
    CHECK_STREQ(q, "abcd", "realloc preserves");
    free(q);
  }

  char *z = malloc(0);
  free(z);
  CHECK(1, "malloc(0) freeable");

  q = malloc(16);
  if (q) {
    strcpy(q, "0123456789");
    q = realloc(q, 4);
    CHECK(q != NULL, "realloc shrink non-null");
    CHECK(q[0] == '0' && q[1] == '1', "realloc shrink prefix");
    free(q);
  }
}

// ============================================================================
// 15. aligned allocation
// ============================================================================
static void test_aligned_alloc(void) {
  void *p = memalign(64, 128);
  CHECK(p != NULL, "memalign non-null");
  if (p) {
    CHECK(((uintptr_t)p % 64) == 0, "memalign 64-byte aligned");
    memset(p, 0xEE, 128);
    free(p);
  }
}

// ============================================================================
// 16. <stdlib.h> qsort / bsearch
// ============================================================================
static int cmp_int(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (x > y) - (x < y);
}

static void test_qsort_bsearch(void) {
  int arr[] = {5, 2, 9, 1, 7, 3};
  qsort(arr, 6, sizeof(int), cmp_int);
  int sorted = 1;
  for (int k = 1; k < 6; k++)
    if (arr[k - 1] > arr[k])
      sorted = 0;
  CHECK(sorted, "qsort ascending");
  CHECK_EQ(arr[0], 1, "qsort min");
  CHECK_EQ(arr[5], 9, "qsort max");

  int key = 7;
  int *found = bsearch(&key, arr, 6, sizeof(int), cmp_int);
  CHECK(found != NULL && *found == 7, "bsearch hit");

  key = 100;
  found = bsearch(&key, arr, 6, sizeof(int), cmp_int);
  CHECK(found == NULL, "bsearch miss");
}

// ============================================================================
// 17. <stdlib.h> abs/div family and rand
// ============================================================================
static void test_misc_stdlib(void) {
  CHECK_EQ(abs(-12), 12, "abs");
  CHECK_EQ(labs(-123456L), 123456L, "labs");
  CHECK_EQ(llabs(-123456789123LL), 123456789123LL, "llabs");

  div_t d = div(17, 5);
  CHECK_EQ(d.quot, 3, "div quot");
  CHECK_EQ(d.rem, 2, "div rem");

  ldiv_t ld = ldiv(-17, 5);
  CHECK_EQ(ld.quot, -3, "ldiv quot");
  CHECK_EQ(ld.rem, -2, "ldiv rem");

  lldiv_t lld = lldiv(100, 7);
  CHECK_EQ(lld.quot, 14, "lldiv quot");
  CHECK_EQ(lld.rem, 2, "lldiv rem");

  srand(12345);
  int expect = rand();
  srand(12345);
  CHECK_EQ(rand(), expect, "srand reproducible");

  srand(1);
  int in_range = 1;
  for (int k = 0; k < 1000; k++) {
    if (rand() < 0 || rand() > RAND_MAX) {
      in_range = 0;
      break;
    }
  }
  CHECK(in_range, "rand within range");
}

// ============================================================================
// 18. <math.h> basics
// ============================================================================
static void test_math_basic(void) {
  CHECK_NEAR(sqrt(2.0), 1.4142135623730951, 1e-15, "sqrt");
  CHECK_NEAR(sqrt(0.0), 0.0, 1e-15, "sqrt zero");

  CHECK_NEAR(fabs(-3.5), 3.5, 1e-15, "fabs");

  CHECK_NEAR(floor(2.7), 2.0, 1e-15, "floor");
  CHECK_NEAR(ceil(2.1), 3.0, 1e-15, "ceil");
  CHECK_NEAR(round(2.5), 3.0, 1e-15, "round half away");
  CHECK_NEAR(round(-2.5), -3.0, 1e-15, "round negative half");
  CHECK_NEAR(trunc(-2.9), -2.0, 1e-15, "trunc");

  CHECK_NEAR(pow(2.0, 10.0), 1024.0, 1e-12, "pow");
  CHECK_NEAR(pow(9.0, 0.5), 3.0, 1e-12, "pow sqrt");

  CHECK_NEAR(exp(1.0), 2.718281828459045, 1e-14, "exp");
  CHECK_NEAR(exp(0.0), 1.0, 1e-15, "exp zero");

  CHECK_NEAR(log(exp(1.0)), 1.0, 1e-14, "log");
  CHECK_NEAR(log(1.0), 0.0, 1e-15, "log one");

  CHECK_NEAR(log10(1000.0), 3.0, 1e-12, "log10");
  CHECK_NEAR(log2(1024.0), 10.0, 1e-12, "log2");
  CHECK_NEAR(exp2(10.0), 1024.0, 1e-9, "exp2");

  CHECK_NEAR(fmod(10.5, 3.0), 1.5, 1e-15, "fmod");
  CHECK(copysign(5.0, -1.0) < 0.0, "copysign negative");
  CHECK(copysign(5.0, 3.0) > 0.0, "copysign positive");
}

// ============================================================================
// 19. <math.h> trigonometry & hyperbolic
// ============================================================================
static void test_math_trig(void) {
  const double pi = 3.14159265358979323846;

  CHECK_NEAR(sin(0.0), 0.0, 1e-15, "sin 0");
  CHECK_NEAR(sin(pi / 2), 1.0, 1e-15, "sin pi/2");
  CHECK_NEAR(cos(0.0), 1.0, 1e-15, "cos 0");
  CHECK_NEAR(cos(pi), -1.0, 1e-15, "cos pi");

  CHECK_NEAR(tan(0.0), 0.0, 1e-15, "tan 0");
  CHECK_NEAR(tan(pi / 4), 1.0, 1e-15, "tan pi/4");

  CHECK_NEAR(asin(1.0), pi / 2, 1e-15, "asin 1");
  CHECK_NEAR(acos(1.0), 0.0, 1e-15, "acos 1");
  CHECK_NEAR(atan(1.0), pi / 4, 1e-15, "atan 1");
  CHECK_NEAR(atan2(1.0, 1.0), pi / 4, 1e-15, "atan2");

  CHECK_NEAR(sinh(0.0), 0.0, 1e-15, "sinh 0");
  CHECK_NEAR(cosh(0.0), 1.0, 1e-15, "cosh 0");
  CHECK_NEAR(tanh(0.0), 0.0, 1e-15, "tanh 0");

  int ok = 1;
  for (int k = -100; k <= 100; k++) {
    double x = k * 0.05;
    if (!float_near(sin(x) * sin(x) + cos(x) * cos(x), 1.0, 1e-12)) {
      ok = 0;
      break;
    }
  }
  CHECK(ok, "sin^2 + cos^2 == 1");
}

// ============================================================================
// 20. <math.h> classification / special values
// ============================================================================
static void test_math_classify(void) {
  double inf = INFINITY;
  double ninf = -INFINITY;
  double nan = NAN;
  double nan2 = NAN;

  CHECK(isinf(inf) && isinf(ninf), "isinf");
  CHECK(!isinf(1.0), "isinf false on finite");

  CHECK(isnan(nan), "isnan");
  CHECK(!isnan(1.0), "isnan false");

  CHECK(isfinite(1.0) && !isfinite(inf) && !isfinite(nan), "isfinite");

  CHECK(signbit(-1.0), "signbit negative");
  CHECK(!signbit(1.0), "signbit positive");
  CHECK(signbit(-0.0), "signbit negative zero");

  CHECK(fpclassify(inf) == FP_INFINITE, "fpclassify inf");
  CHECK(fpclassify(nan) == FP_NAN, "fpclassify nan");
  CHECK(fpclassify(0.0) == FP_ZERO, "fpclassify zero");
  CHECK(fpclassify(1.0) == FP_NORMAL, "fpclassify normal");

  CHECK(nan != nan2, "nan != nan");
  CHECK(inf > 1e308, "inf > huge");

  CHECK_NEAR(hypot(3.0, 4.0), 5.0, 1e-15, "hypot 3-4-5");

  CHECK_NEAR(fma(2.0, 3.0, 4.0), 10.0, 1e-15, "fma");

  CHECK_NEAR(ldexp(1.5, 3), 12.0, 1e-15, "ldexp");
  int ex = 0;
  double fr = frexp(8.0, &ex);
  CHECK_NEAR(fr, 0.5, 1e-15, "frexp frac");
  CHECK_EQ(ex, 4, "frexp exp");
  double ip = 0;
  double fp = modf(3.75, &ip);
  CHECK_NEAR(ip, 3.0, 1e-15, "modf int");
  CHECK_NEAR(fp, 0.75, 1e-15, "modf frac");
}

// ============================================================================
// 21. <setjmp.h> longjmp / setjmp
// ============================================================================
static jmp_buf g_jmp;
static int g_jmp_stage = 0;

__attribute__((noreturn)) static void longjmp_target(void) {
  longjmp(g_jmp, 1);
}

static void test_setjmp_longjmp(void) {
  volatile int depth = 0;
  g_jmp_stage = 0;

  if (setjmp(g_jmp) == 0) {
    g_jmp_stage = 1;
    depth = 1;
    longjmp_target();         // never returns
    g_jmp_stage = -1;         // unreachable if longjmp works
  } else {
    CHECK(g_jmp_stage == 1, "setjmp: resumed with stage preserved");
    CHECK(depth == 1, "setjmp: volatile stack value preserved");
    g_jmp_stage = 2;
  }
  CHECK(g_jmp_stage == 2, "setjmp: normal completion");
}

// ============================================================================
// 22. <time.h> clock / time / difftime / mktime / strftime
// ============================================================================
static void test_time(void) {
  time_t t0 = time(NULL);
  CHECK(t0 != (time_t)-1, "time() available");

  time_t a = 100;
  time_t b = 250;
  CHECK_NEAR(difftime(b, a), 150.0, 1e-15, "difftime");

  clock_t c1 = clock();
  clock_t c2 = clock();
  CHECK(c2 >= c1, "clock monotonic");

  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  tm.tm_year = 120;
  tm.tm_mon = 0;
  tm.tm_mday = 1;
  tm.tm_hour = 12;
  tm.tm_min = 30;
  tm.tm_sec = 15;
  time_t mt = mktime(&tm);
  CHECK(mt != (time_t)-1, "mktime success");
  CHECK_EQ(tm.tm_year, 120, "mktime normalized year");
  CHECK_EQ(tm.tm_mday, 1, "mktime normalized day");

  char out[64];
  size_t n = strftime(out, sizeof(out), "%Y-%m-%d", &tm);
  CHECK(n > 0, "strftime non-empty");
  if (n > 0)
    CHECK_STREQ(out, "2020-01-01", "strftime format");

  CHECK(asctime(&tm) != NULL, "asctime non-null");
  CHECK(ctime(&mt) != NULL, "ctime non-null");

  struct tm *lt = localtime(&mt);
  struct tm *gt = gmtime(&mt);
  CHECK(lt != NULL && gt != NULL, "localtime/gmtime non-null");
}

// ============================================================================
// 23. <errno.h> errno get/set semantics
// ============================================================================
static void test_errno(void) {
  errno = 0;
  CHECK_EQ(errno, 0, "errno zeroed");

  errno = EINVAL;
  CHECK_EQ(errno, EINVAL, "errno set/get");

  errno = 0;
  double r = log(-1.0);
  CHECK(isnan(r), "log(-1) is nan");
  CHECK(errno == 0 || errno == EDOM, "errno after log(-1)");

  CHECK(EINVAL > 0, "EINVAL defined positive");
  CHECK(ENOMEM > 0, "ENOMEM defined");
  CHECK(EDOM > 0, "EDOM defined");
  CHECK(ERANGE > 0, "ERANGE defined");

  errno = 0;
}

// ============================================================================
// 24. <inttypes.h> and <stdint.h> integer types
// ============================================================================
static void test_inttypes(void) {
  CHECK_EQ(sizeof(int8_t), 1, "int8_t size");
  CHECK_EQ(sizeof(int16_t), 2, "int16_t size");
  CHECK_EQ(sizeof(int32_t), 4, "int32_t size");
  CHECK_EQ(sizeof(int64_t), 8, "int64_t size");
  CHECK_EQ(sizeof(uintptr_t), sizeof(void *), "uintptr_t size");
  CHECK_EQ(sizeof(size_t), sizeof(void *), "size_t size");
  CHECK_EQ(sizeof(intmax_t), 8, "intmax_t size");

  CHECK_EQ(INT16_MAX, 32767, "INT16_MAX");
  CHECK_EQ(UINT32_MAX, 4294967295u, "UINT32_MAX");

  char *end = NULL;
  intmax_t v = strtoimax("123456789012345", &end, 10);
  CHECK_EQ(v, 123456789012345LL, "strtoimax value");
  uintmax_t uv = strtoumax("0xFFFFFFFFFFFFFFFF", &end, 16);
  CHECK_EQ(uv, UINTMAX_MAX, "strtoumax max");

  char buf[64];
  snprintf(buf, sizeof(buf), "%" PRId64, (int64_t)-42);
  CHECK_STREQ(buf, "-42", "PRId64 format");
  snprintf(buf, sizeof(buf), "%" PRIu32, (uint32_t)42);
  CHECK_STREQ(buf, "42", "PRIu32 format");
  snprintf(buf, sizeof(buf), "%" PRIxPTR, (uintptr_t)0xabc);
  CHECK_STREQ(buf, "abc", "PRIxPTR format");
}

// ============================================================================
// 25. <stdlib.h> environment variables
// ============================================================================
static void test_env(void) {
  CHECK(setenv("TCM_TEST_VAR", "12345", 1) == 0, "setenv success");
  CHECK_STREQ(getenv("TCM_TEST_VAR"), "12345", "getenv roundtrip");
  CHECK(setenv("TCM_TEST_VAR", "overwritten", 1) == 0, "setenv overwrite");
  CHECK_STREQ(getenv("TCM_TEST_VAR"), "overwritten", "getenv overwritten");
  CHECK(setenv("TCM_TEST_VAR", "keep", 0) == 0, "setenv no-overwrite");
  CHECK_STREQ(getenv("TCM_TEST_VAR"), "overwritten", "getenv no-overwrite kept");
  CHECK(unsetenv("TCM_TEST_VAR") == 0, "unsetenv");
  CHECK(getenv("TCM_TEST_VAR") == NULL, "getenv after unsetenv NULL");
}

// ============================================================================
// 26. <stdlib.h> additional conversions
// ============================================================================
static void test_additional_conversions(void) {
  char *end = NULL;

  CHECK_EQ(atoll("9223372036854775807"), 9223372036854775807LL, "atoll max");

  unsigned long ul = strtoul("4000000000", &end, 10);
  CHECK_EQ(ul, 4000000000UL, "strtoul large");
  CHECK(end != NULL && *end == 0, "strtoul full consume");

  double d = strtod("  -1.25e-1 ", &end);
  CHECK_NEAR(d, -0.125, 1e-15, "strtod negative exponent");

  float f = strtof("2.5", &end);
  CHECK_NEAR((double)f, 2.5, 1e-6, "strtof");

  long double ld = strtold("3.5", &end);
  CHECK_NEAR((double)ld, 3.5, 1e-15, "strtold");
}

// ============================================================================
// 27. <stdio.h> vprintf family
// ============================================================================
static int fmt_capture(char *buf, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, n, fmt, ap);
  va_end(ap);
  return r;
}

static void test_vprintf_family(void) {
  char buf[128];
  CHECK_EQ(fmt_capture(buf, sizeof(buf), "%d %s", 5, "five"), 6,
           "vsnprintf return");
  CHECK_STREQ(buf, "5 five", "vsnprintf content");

  int n = fmt_capture(buf, sizeof(buf), "%08x", 0x1a);
  CHECK_STREQ(buf, "0000001a", "vsnprintf zero pad");
  CHECK_EQ(n, 8, "vsnprintf zero pad len");
}

// ============================================================================
// 28. <stdio.h> fseek / ftell / rewind on memory stream
// ============================================================================
static void test_fseek(void) {
  const char *src = "0123456789";
  FILE *f = fmemopen((void *)src, 10, "r");
  if (!f) {
    CHECK(0, "fseek open");
    return;
  }

  CHECK_EQ(fseek(f, 4, SEEK_SET), 0, "fseek set");
  CHECK_EQ(ftell(f), 4, "ftell after set");

  CHECK_EQ(fseek(f, 2, SEEK_CUR), 0, "fseek cur");
  CHECK_EQ(ftell(f), 6, "ftell after cur");

  CHECK_EQ(fseek(f, -2, SEEK_END), 0, "fseek end");
  CHECK_EQ(ftell(f), 8, "ftell after end");

  CHECK_EQ(fgetc(f), '8', "fgetc after seek");

  rewind(f);
  CHECK_EQ(ftell(f), 0, "ftell after rewind");
  CHECK_EQ(fgetc(f), '0', "fgetc after rewind");

  fclose(f);

  f = fmemopen((void *)src, 10, "r");
  if (f) {
    CHECK_EQ(fseeko(f, 5, SEEK_SET), 0, "fseeko set");
    CHECK_EQ(ftello(f), 5, "ftello");
    fclose(f);
  }
}

// ============================================================================
// 29. <stdio.h> fread/fwrite binary round-trip
// ============================================================================
static void test_fread_fwrite(void) {
  unsigned char in[16];
  for (int k = 0; k < 16; k++)
    in[k] = (unsigned char)(k * 17);

  FILE *f = fmemopen(NULL, 16, "w+");
  CHECK(f != NULL, "fmemopen w+");
  if (f) {
    CHECK_EQ(fwrite(in, 1, 16, f), 16, "fwrite count");
    rewind(f);
    unsigned char out[16] = {0};
    CHECK_EQ(fread(out, 1, 16, f), 16, "fread count");
    CHECK(memcmp(in, out, 16) == 0, "fread/fwrite binary roundtrip");
    fclose(f);
  }

  f = fmemopen((void *)in, 16, "r");
  if (f) {
    unsigned char out[8];
    CHECK_EQ(fread(out, 2, 8, f), 8, "fread nmemb");
    CHECK(out[0] == in[0] && out[1] == in[1], "fread nmemb content");
    fclose(f);
  }
}

// ============================================================================
// 30. <math.h> rounding modes and remaining funcs
// ============================================================================
static void test_math_rounding(void) {
  CHECK_EQ((int)rint(2.5), 2, "rint half-even");
  CHECK_EQ((int)rint(3.5), 4, "rint half-even 3.5");
  CHECK_EQ((int)nearbyint(2.5), 2, "nearbyint");

  CHECK_NEAR(remainder(10.0, 3.0), 1.0, 1e-15, "remainder value");

  CHECK_NEAR(fmin(3.0, 5.0), 3.0, 1e-15, "fmin");
  CHECK_NEAR(fmax(3.0, 5.0), 5.0, 1e-15, "fmax");

  CHECK_NEAR(fdim(5.0, 3.0), 2.0, 1e-15, "fdim");
  CHECK_NEAR(fdim(3.0, 5.0), 0.0, 1e-15, "fdim clamp");

  CHECK(copysign(2.0, -0.0) < 0.0, "copysign neg zero");

  CHECK_NEAR(nextafter(1.0, 2.0), 1.0000000000000002, 1e-16, "nextafter up");
  CHECK(nextafter(1.0, 2.0) > 1.0, "nextafter greater");

  CHECK_NEAR(cbrt(27.0), 3.0, 1e-15, "cbrt");
  CHECK_NEAR(erf(0.0), 0.0, 1e-15, "erf zero");
  CHECK_NEAR(erfc(0.0), 1.0, 1e-15, "erfc zero");
  CHECK_NEAR(tgamma(3.0), 2.0, 1e-13, "tgamma 3");
  CHECK_NEAR(lgamma(4.0), log(6.0), 1e-12, "lgamma 4");

  CHECK_NEAR(expm1(0.0), 0.0, 1e-15, "expm1 zero");
  CHECK_NEAR(log1p(0.0), 0.0, 1e-15, "log1p zero");
}

// ============================================================================
// 31. <string.h> strtok / strerror
// ============================================================================
static void test_strtok_strerror(void) {
  char buf[64] = "one,two,three";
  char *tok = strtok(buf, ",");
  CHECK(tok != NULL && strcmp(tok, "one") == 0, "strtok first");
  tok = strtok(NULL, ",");
  CHECK(tok != NULL && strcmp(tok, "two") == 0, "strtok second");
  tok = strtok(NULL, ",");
  CHECK(tok != NULL && strcmp(tok, "three") == 0, "strtok third");
  tok = strtok(NULL, ",");
  CHECK(tok == NULL, "strtok exhausted");

  const char *e = strerror(EINVAL);
  CHECK(e != NULL && *e != '\0', "strerror non-empty");
  CHECK_STREQ(strerror(0), "Success", "strerror(0) == Success");
}

// ============================================================================
// 32. memory layout helpers
// ============================================================================
static void test_more_memory(void) {
  char big[512];
  memset(big, 0x5A, sizeof(big));
  int ok = 1;
  for (size_t i = 0; i < sizeof(big); i++)
    if ((unsigned char)big[i] != 0x5A)
      ok = 0;
  CHECK(ok, "memset large fill");

  struct pair {
    int a;
    double b;
  } s1 = {7, 3.5}, s2;
  memcpy(&s2, &s1, sizeof(s2));
  CHECK(s2.a == 7 && s2.b == 3.5, "memcpy struct");

  CHECK_EQ(memcmp("a", "b", 0), 0, "memcmp zero length");
}

// ============================================================================
// 33. snprintf truncation & return value semantics
// ============================================================================
static void test_snprintf_truncation(void) {
  char small[8];
  int n = snprintf(small, sizeof(small), "%s", "this is a long string");
  CHECK_EQ(n, (int)strlen("this is a long string"), "snprintf would-be length");
  CHECK_STREQ(small, "this is", "snprintf truncated content");
  CHECK(small[sizeof(small) - 1] == '\0', "snprintf NUL-terminated");

  n = snprintf(NULL, 0, "%d", 1234);
  CHECK_EQ(n, 4, "snprintf size-0 return");

  n = snprintf(small, sizeof(small), "%010d", 5);
  CHECK_EQ(n, 10, "snprintf width would-be length");
}

// ============================================================================
// 34. integer formatting edges
// ============================================================================
static void test_format_edges(void) {
  char buf[64];

  snprintf(buf, sizeof(buf), "%d", INT_MAX);
  CHECK_STREQ(buf, "2147483647", "printf INT_MAX");

  snprintf(buf, sizeof(buf), "%d", INT_MIN);
  CHECK_STREQ(buf, "-2147483648", "printf INT_MIN");

  snprintf(buf, sizeof(buf), "%u", UINT_MAX);
  CHECK_STREQ(buf, "4294967295", "printf UINT_MAX");

  snprintf(buf, sizeof(buf), "%08x", 0);
  CHECK_STREQ(buf, "00000000", "printf hex zero pad");

  snprintf(buf, sizeof(buf), "%#x", 0x1a);
  CHECK_STREQ(buf, "0x1a", "printf alt hex");

  snprintf(buf, sizeof(buf), "%#o", 8);
  CHECK_STREQ(buf, "010", "printf alt oct");
}

// ============================================================================
// 35. character classification ASCII edges
// ============================================================================
static void test_ctype_edge(void) {
  CHECK(isascii('a') && isascii(127), "isascii");
  CHECK(!isascii(128), "isascii high bit");
  CHECK(isblank(' ') && isblank('\t') && !isblank('a'), "isblank");
  CHECK(!ispunct('a') && ispunct('!') && ispunct('.'), "ispunct");
  CHECK(isgraph('a') && isgraph('!') && !isgraph(' '), "isgraph");
}

// ============================================================================
// 36. <stdio.h> fgetpos / fsetpos (opaque position round-trip on fmemopen)
// ============================================================================
static void test_fgetpos_fsetpos(void) {
  const char *src = "abcdefghij";
  FILE *f = fmemopen((void *)src, 10, "r");
  if (!f) {
    CHECK(0, "fgetpos open");
    return;
  }

  // Read two chars, grab the position, advance, then restore the saved point.
  CHECK_EQ(fgetc(f), 'a', "fgetpos fgetc1");
  CHECK_EQ(fgetc(f), 'b', "fgetpos fgetc2");
  fpos_t pos;
  CHECK_EQ(fgetpos(f, &pos), 0, "fgetpos call");
  CHECK_EQ(ftell(f), 2, "fgetpos matches ftell");

  CHECK_EQ(fgetc(f), 'c', "fgetpos fgetc3");
  CHECK_EQ(ftell(f), 3, "fgetpos progressed");

  CHECK_EQ(fsetpos(f, &pos), 0, "fsetpos call");
  CHECK_EQ(ftell(f), 2, "fsetpos restored offset");
  CHECK_EQ(fgetc(f), 'c', "fsetpos re-read at saved pos");
  fclose(f);

  // Write path: save position after writing, then verify.
  char out[16] = {0};
  f = fmemopen(out, 16, "w+");
  if (f) {
    int w = fprintf(f, "12345");
    CHECK(w == 5, "fgetpos write first");
    fpos_t p2;
    CHECK_EQ(fgetpos(f, &p2), 0, "fgetpos after write");
    CHECK_EQ(ftell(f), 5, "fgetpos write offset 5");
    CHECK_EQ(fsetpos(f, &p2), 0, "fsetpos to saved");
    CHECK_EQ(ftell(f), 5, "fsetpos offset restored");
    CHECK_EQ(fseeko(f, 0, SEEK_SET), 0, "fgetpos seek to 0");
    CHECK_EQ(ftell(f), 0, "fgetpos at 0");
    CHECK_EQ(fgetc(f), '1', "fgetpos verify byte 0");
    fclose(f);
  }
}

// ============================================================================
// 37. <string.h> strdup / strndup (GNU; allocated copy)
// ============================================================================
static void test_string_dup(void) {
  char *p = strdup("dup me");
  CHECK(p != NULL, "strdup non-null");
  if (p) {
    CHECK_STREQ(p, "dup me", "strdup content");
    free(p);
  }

  char *q = strndup("abcdef", 3);
  CHECK(q != NULL, "strndup non-null");
  if (q) {
    CHECK_STREQ(q, "abc", "strndup limited");
    free(q);
  }

  char *r = strndup("short", 100);
  CHECK(r != NULL, "strndup over-len non-null");
  if (r) {
    CHECK_STREQ(r, "short", "strndup over-len content");
    free(r);
  }
}

// ============================================================================
// 38. <string.h> GNU/BSD extras: strnlen / stpcpy / stpncpy / strsep
// ============================================================================
static void test_string_gnu(void) {
  CHECK_EQ(strnlen("hello", 10), 5, "strnlen within");
  CHECK_EQ(strnlen("hello world", 5), 5, "strnlen capped");
  CHECK_EQ(strnlen("", 3), 0, "strnlen empty");

  char buf[32];
  char *end = stpcpy(buf, "tail");
  CHECK_STREQ(buf, "tail", "stpcpy content");
  CHECK_EQ((int)(end - buf), 4, "stpcpy returns end ptr");

  char b2[32];
  char *end2 = stpncpy(b2, "abcdef", 3);
  CHECK_EQ((int)(end2 - b2), 3, "stpncpy position");
  b2[3] = '\0';
  CHECK_STREQ(b2, "abc", "stpncpy content");

  char sep[32] = "a,b,c";
  char *save = sep;
  char *tok = strsep(&save, ",");
  CHECK(tok != NULL && strcmp(tok, "a") == 0, "strsep first");
  tok = strsep(&save, ",");
  CHECK(tok != NULL && strcmp(tok, "b") == 0, "strsep second");
  tok = strsep(&save, ",");
  CHECK(tok != NULL && strcmp(tok, "c") == 0, "strsep third");
  tok = strsep(&save, ",");
  CHECK(tok == NULL, "strsep exhausted");
}

// ============================================================================
// 39. <string.h> memccpy / mempcpy (POSIX/GNU copy with return)
// ============================================================================
static void test_posix_mem(void) {
  const char *hay = "abcXYZ";
  char buf[16];
  char *r = memccpy(buf, hay, 'X', 6);
  CHECK(r != NULL, "memccpy found");
  if (r) {
    CHECK(r > buf && *(r - 1) == 'X', "memccpy stops after X");
    CHECK(buf[0] == 'a' && buf[1] == 'b' && buf[2] == 'c', "memccpy content");
  }

  char buf2[16];
  char *m = mempcpy(buf2, "xyzab", 3);
  CHECK_EQ((int)(m - buf2), 3, "mempcpy return");
  buf2[3] = 0;
  CHECK_STREQ(buf2, "xyz", "mempcpy content");
}

// ============================================================================
// 40. <strings.h> ffsl / ffsll (GNU long variants)
// ============================================================================
static void test_ffsl_ffsll(void) {
  CHECK_EQ(ffsl(0L), 0, "ffsl zero");
  CHECK_EQ(ffsl(1L), 1, "ffsl lsb");
  CHECK_EQ(ffsl(1L << 20), 21, "ffsl high bit");
  CHECK_EQ(ffsll(0LL), 0, "ffsll zero");
  CHECK_EQ(ffsll(4LL), 3, "ffsll bit2");
}

// ============================================================================
// 41. <ctype.h> toascii (7-bit mask)
// ============================================================================
static void test_ctype_extras(void) {
  CHECK_EQ(toascii(0x80 | 'a'), 'a', "toascii strips high bit");
  CHECK_EQ(toascii(0xF5), 0x75, "toascii mask");
  CHECK(isascii(toascii(0x90)), "toascii yields ascii");
}

// ============================================================================
// 42. <stdio.h> more scanf format: %i / %* suppress / %[ scanset / width
// ============================================================================
static void test_scanf_more(void) {
  int i = 0;
  CHECK_EQ(sscanf("42", "%i", &i), 1, "sscanf %i dec count");
  CHECK_EQ(i, 42, "sscanf %i dec value");
  CHECK_EQ(sscanf("0x1A", "%i", &i), 1, "sscanf %i hex count");
  CHECK_EQ(i, 26, "sscanf %i hex value");

  int a = 0, b = 0;
  CHECK_EQ(sscanf("10 20 30", "%d %*d %d", &a, &b), 2, "sscanf %*d count");
  CHECK_EQ(a, 10, "sscanf suppressed a");
  CHECK_EQ(b, 30, "sscanf suppressed b");

  char letters[16] = "";
  CHECK_EQ(sscanf("abc123", "%[a-z]", letters), 1, "sscanf scanset count");
  CHECK_STREQ(letters, "abc", "sscanf scanset content");

  char digits[16] = "";
  CHECK_EQ(sscanf("123abc", "%[0-9]", digits), 1, "sscanf scanset digits");
  CHECK_STREQ(digits, "123", "sscanf scanset digits content");

  char c = 0;
  CHECK_EQ(sscanf("xy", "%c", &c), 1, "sscanf %c count");
  CHECK_EQ(c, 'x', "sscanf %c value");

  char w[4] = "";
  CHECK_EQ(sscanf("hello", "%3s", w), 1, "sscanf width count");
  CHECK_STREQ(w, "hel", "sscanf width content");
}

// ============================================================================
// 43. <stdio.h> more printf format: %i / %.precision string / %p non-empty
// ============================================================================
static void test_printf_more(void) {
  char buf[64];

  snprintf(buf, sizeof(buf), "%i", -5);
  CHECK_STREQ(buf, "-5", "printf %i");

  snprintf(buf, sizeof(buf), "%.3s", "abcdef");
  CHECK_STREQ(buf, "abc", "printf .precision string");

  snprintf(buf, sizeof(buf), "[%6.2f]", 3.14159);
  CHECK_STREQ(buf, "[  3.14]", "printf width+precision float");

  // %p formatting varies across tinystdio versions; assert a non-empty result only.
  int n = snprintf(buf, sizeof(buf), "%p", (void *)0x1234);
  CHECK(n > 0 && buf[0] != '\0', "printf %p non-empty");
}

// ============================================================================
// main
// ============================================================================
int main(void) {
  printf("=== picolibc AUTOTEST ===\n");

  test_string_core();
  test_string_search();
  test_memory();
  test_strings_h();
  test_ctype();
  test_printf_formatting();
  test_printf_float();
  test_scanf();
  test_asprintf();
  test_memory_streams();
  test_stdio_primitives();
  test_getline();
  test_strto();
  test_malloc();
  test_aligned_alloc();
  test_qsort_bsearch();
  test_misc_stdlib();
  test_math_basic();
  test_math_trig();
  test_math_classify();
  test_setjmp_longjmp();
  test_time();
  test_errno();
  test_inttypes();
  test_env();
  test_additional_conversions();
  test_vprintf_family();
  test_fseek();
  test_fread_fwrite();
  test_math_rounding();
  test_strtok_strerror();
  test_more_memory();
  test_snprintf_truncation();
  test_format_edges();
  test_ctype_edge();
  test_fgetpos_fsetpos();
  test_string_dup();
  test_string_gnu();
  test_posix_mem();
  test_ffsl_ffsll();
  test_ctype_extras();
  test_scanf_more();
  test_printf_more();

  printf("\n[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1;
}
