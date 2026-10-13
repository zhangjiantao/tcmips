// Semihost stdin autotest for the TCMIPS emulator.
//
// Exercises the guest-side standard-input bridge directly.  The emulator is
// launched with `-stdin=<file>`, and the guest reads that data through
// tcm_syscall_semihost_stdin — the exact syscall the BSP's read(0) wraps
// (tcmips/bsp/posix.c) — proving the semihost *input* path end to end.  Every
// other autotest only exercises the *output* half (printf → semihost stdout),
// so this is the only test that verifies data can flow from the host into the
// guest.
//
// Robustness: when no `-stdin=` is wired up the syscall returns 0xFFFFFFFF
// ("not wired", distinct from EOF), so this test detects that and SKIPs cleanly
// (exit 0, no FAIL) rather than blocking or misreporting — allowing it to be
// built alongside the other AUTOTEST targets and still survive a bare run.

#include <dev/syscall.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---------------- autotest harness -------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define SEMIHOST_NO_STDIN 0xFFFFFFFFu

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
    unsigned long long a_ = (unsigned long long)(actual);                      \
    unsigned long long e_ = (unsigned long long)(expected);                    \
    char d_[64];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%llu expected=%llu)", a_, e_);           \
    t_check(a_ == e_, name, d_);                                               \
  } while (0)
#define CHECK_STREQ(actual, expected, name)                                    \
  do {                                                                         \
    const char *a_ = (actual);                                                 \
    const char *e_ = (expected);                                               \
    char d_[96];                                                               \
    snprintf(d_, sizeof(d_), "(actual=\"%s\" expected=\"%s\")", a_, e_);       \
    t_check(strcmp(a_, e_) == 0, name, d_);                                    \
  } while (0)

/* Read exactly n bytes via the semihost stdin syscall, or fail. */
static int read_exact(uint8_t *dst, uint32_t n) {
  uint32_t off = 0;
  while (off < n) {
    uint32_t r = tcm_syscall_semihost_stdin((uint32_t)(dst + off), n - off);
    if (r == SEMIHOST_NO_STDIN || r == 0)
      return 0; /* no data / EOF before we filled the buffer */
    off += r;
  }
  return 1;
}

/* Buffer the whole stdin payload so we can probe "not wired" vs "EOF". */
static uint32_t read_all(uint8_t *buf, uint32_t cap) {
  uint32_t n = 0;
  for (;;) {
    uint32_t r = tcm_syscall_semihost_stdin((uint32_t)(buf + n), cap - n);
    if (r == SEMIHOST_NO_STDIN)
      return SEMIHOST_NO_STDIN; /* signal: not wired */
    if (r == 0)
      break; /* EOF */
    n += r;
    if (n >= cap)
      break;
  }
  return n;
}

// ============================================================================
// main — the stdin payload is: "hello stdin\n42\n"
// ============================================================================
int main(void) {
  printf("=== semihost stdin AUTOTEST ===\n");

  uint8_t buf[256];
  uint32_t n = read_all(buf, sizeof(buf));

  if (n == SEMIHOST_NO_STDIN) {
    /* No -stdin=<file> was provided: the host has nothing to feed the guest,
     * so there is nothing to test.  This is a run-configuration condition, not
     * a guest failure — report a greppable SKIP and exit 0 so the bare
     * "run every *.tcm" regression loop is not blocked and does not report a
     * false FAIL.  To actually run this suite, add the -stdin=<file> argument;
     * see test/stdin/CMakeLists.txt for the exact command + generated payload.
     */
    printf("[SKIP] stdin-wired skipped: no -stdin=<file> provided "
           "(see test/stdin/CMakeLists.txt for how to pass it)\n");
    printf("\n[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
    printf("ALL TESTS PASSED (stdin SKIPPED: needs -stdin=<file>)\n");
    return 0;
  }

  buf[n] = '\0';

  /* 1. Entire payload matches the expected stream. */
  CHECK_STREQ((const char *)buf, "hello stdin\n42\n", "stdin payload exact");

  /* 2. Stream is terminated by EOF: a further read returns 0. */
  uint32_t r = tcm_syscall_semihost_stdin((uint32_t)buf, 1);
  CHECK_EQ(r, 0u, "read past end returns EOF (0)");

  printf("\n[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1;
}
