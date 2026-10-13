// BSP POSIX bridge autotest for the TCMIPS emulator.
//
// This test exercises the *standard C file API* (fopen/fread/fwrite/fseek/
// ftell/fclose, plus the low-level open/read/write/stat/opendir/readdir) on
// top of the in-memory filesystem.  Unlike test_memfs_autotest.c (which calls
// the libmemfs memfs_* API directly) and test_memfs_autotest.cpp (which drives
// libc++ <filesystem>), this one goes through the bsp glue layer
// (tcmips/bsp/posix.c) — the same path picolibc's standard I/O uses — to prove
// that fopen/fread/fwrite/stat actually reach memfs via the POSIX syscall
// bridge and honour open flags, seek semantics and directory enumeration.
//
// Assets are burned into the image by the linker (--tcm-memfs=<dir>); see
// test/memfs/CMakeLists.txt.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
    char d_[64];                                                               \
    snprintf(d_, sizeof(d_), "(actual=%lld expected=%lld)", a_, e_);           \
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

// ============================================================================
// 1. stdio file API over memfs (fopen/fwrite/fseek/ftell/fread/fclose)
// ============================================================================
static void test_stdio_file_api(void) {
  printf("\n== 1. stdio file API ==\n");

  // fopen a packed asset read-only.
  FILE *f = fopen("/hello.txt", "rb");
  CHECK(f != NULL, "fopen packed asset");
  if (f) {
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    CHECK_STREQ(buf, "Hello, memfs!", "fread packed content");
    CHECK_EQ((long long)n, (long long)strlen("Hello, memfs!"),
             "fread byte count");
    fclose(f);
  }

  // Create + write + read back through stdio.
  const char *path = "/stdio_roundtrip.txt";
  f = fopen(path, "wb");
  CHECK(f != NULL, "fopen wb creates file");
  if (f) {
    const char *data = "line1\nline2\nline3";
    size_t wn = fwrite(data, 1, strlen(data), f);
    CHECK_EQ((long long)wn, (long long)strlen(data), "fwrite byte count");
    fclose(f);
  }

  f = fopen(path, "rb");
  CHECK(f != NULL, "fopen rb reopens created file");
  if (f) {
    CHECK_EQ((long long)fseek(f, 0, SEEK_END), 0, "fseek SEEK_END");
    long sz = ftell(f);
    CHECK_EQ(sz, (long)strlen("line1\nline2\nline3"), "ftell size");
    CHECK_EQ((long long)fseek(f, 0, SEEK_SET), 0, "fseek SEEK_SET");
    char buf[64] = {0};
    fread(buf, 1, sizeof(buf) - 1, f);
    CHECK_STREQ(buf, "line1\nline2\nline3", "fread round-trip content");
    fclose(f);
  }

  // fopen with append mode: existing content preserved, appended after.
  f = fopen(path, "a");
  CHECK(f != NULL, "fopen append mode");
  if (f) {
    fwrite("+tail", 1, 5, f);
    fclose(f);
  }
  f = fopen(path, "rb");
  if (f) {
    char buf[64] = {0};
    fread(buf, 1, sizeof(buf) - 1, f);
    CHECK_STREQ(buf, "line1\nline2\nline3+tail", "append preserves + extends");
    fclose(f);
  }

  // fopen a missing name should fail (no O_CREAT implied by fopen "rb").
  f = fopen("/no_such_stdio_file.txt", "rb");
  CHECK(f == NULL, "fopen missing rb returns NULL");
}

// ============================================================================
// 2. low-level open/read/write/lseek via the POSIX bridge
// ============================================================================
static void test_lowlevel_io(void) {
  printf("\n== 2. low-level open/read/write/lseek ==\n");

  const char *path = "/lowlevel.bin";
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  CHECK(fd >= 0, "open O_CREAT|O_TRUNC");
  if (fd >= 0) {
    const char *a = "ABCDEFGHIJ";
    ssize_t wn = write(fd, a, 10);
    CHECK_EQ((long long)wn, 10, "write 10 bytes");
    close(fd);
  }

  fd = open(path, O_RDONLY);
  CHECK(fd >= 0, "open O_RDONLY");
  if (fd >= 0) {
    char buf[16] = {0};
    ssize_t rn = read(fd, buf, 10);
    CHECK_EQ((long long)rn, 10, "read 10 bytes");
    CHECK_EQ(strncmp(buf, "ABCDEFGHIJ", 10), 0, "read content matches");
    CHECK_EQ((long long)lseek(fd, 5, SEEK_SET), 5, "lseek SEEK_SET");
    char c = 0;
    read(fd, &c, 1);
    CHECK_EQ((int)c, (int)'F', "lseek then read at offset");
    close(fd);
  }

  // Read permission is not enforced (memfs is a writable RAM volume), but a
  // read on a write-only fd should still fail per open mode (O_ACCMODE).
  fd = open(path, O_WRONLY);
  if (fd >= 0) {
    char c = 0;
    errno = 0;
    ssize_t rn = read(fd, &c, 1);
    CHECK(rn < 0, "read on write-only fd fails");
    close(fd);
  }
}

// ============================================================================
// 3. stat / fstat through the bridge
// ============================================================================
static void test_stat(void) {
  printf("\n== 3. stat / fstat ==\n");

  struct stat sb;
  int rc = stat("/hello.txt", &sb);
  CHECK_EQ((long long)rc, 0, "stat packed asset");
  CHECK_EQ((long long)(sb.st_size), (long long)strlen("Hello, memfs!"),
           "stat st_size");
  CHECK((sb.st_mode & S_IFMT) == S_IFREG, "stat S_IFREG type");

  rc = stat("/no_such_stat.txt", &sb);
  CHECK(rc != 0 && errno == ENOENT, "stat missing -> ENOENT");

  int fd = open("/hello.txt", O_RDONLY);
  if (fd >= 0) {
    rc = fstat(fd, &sb);
    CHECK_EQ((long long)rc, 0, "fstat fd");
    CHECK_EQ((long long)(sb.st_size), (long long)strlen("Hello, memfs!"),
             "fstat st_size");
    close(fd);
  }
}

// ============================================================================
// 4. directory enumeration through the bridge (opendir/readdir/closedir)
// ============================================================================
static void test_directory(void) {
  printf("\n== 4. opendir/readdir ==\n");

  DIR *d = opendir("/");
  CHECK(d != NULL, "opendir root");
  if (d) {
    int saw_hello = 0, saw_levels = 0, saw_dot = 0, saw_dotdot = 0;
    struct dirent *e;
    int count = 0;
    while ((e = readdir(d)) != NULL && count < 64) {
      count++;
      if (strcmp(e->d_name, ".") == 0)
        saw_dot = 1;
      else if (strcmp(e->d_name, "..") == 0)
        saw_dotdot = 1;
      else if (strcmp(e->d_name, "hello.txt") == 0)
        saw_hello = 1;
      else if (strcmp(e->d_name, "levels") == 0)
        saw_levels = 1;
    }
    CHECK(saw_dot, "readdir returns '.'");
    CHECK(saw_dotdot, "readdir returns '..'");
    CHECK(saw_hello, "readdir lists hello.txt");
    CHECK(saw_levels, "readdir lists levels dir");
    closedir(d);
  }

  // Create a directory via mkdir then list it.
  if (mkdir("/bridge_dir", 0755) == 0) {
    FILE *f = fopen("/bridge_dir/child.txt", "wb");
    if (f) {
      fwrite("x", 1, 1, f);
      fclose(f);
    }
    d = opendir("/bridge_dir");
    if (d) {
      int saw_child = 0;
      struct dirent *e;
      while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, "child.txt") == 0)
          saw_child = 1;
      }
      CHECK(saw_child, "readdir lists runtime-created file");
      closedir(d);
    }
  }
}

// ============================================================================
// main
// ============================================================================
int main(void) {
  printf("=== bsp fileio AUTOTEST ===\n");

  test_stdio_file_api();
  test_lowlevel_io();
  test_stat();
  test_directory();

  printf("\n[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1;
}
