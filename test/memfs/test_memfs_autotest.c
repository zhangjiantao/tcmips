// libmemfs cross-compiled autotest for the TCMIPS emulator.
//
// This is a full AUTOTEST (see AUTOTEST.md) for libmemfs under the real
// semihost runtime: every assertion is emitted as a single greppable line
//   [SELF] <name> PASS   /   [SELF] <name> FAIL (detail)
// and the process exits with code 0 when every check passed, non-zero
// otherwise. The emulator reports that code as "!! TCMIPS EMULATOR EXIT, code=N
// !!".
//
// Compared to the plain demo it also drives memfs' TCM_MEMFS_DEBUG accounting
// so we can prove the filesystem does not leak memory across
// create/write/unlink cycles, does not leak live Chunk nodes after
// truncate/unlink, and that a repeated init() is a full reset (heap bytes and
// chunks return to baseline).
//
// The payload is embedded by the linker (--tcm-memfs=<dir>) and mounted by
// start.c before main() runs, so the normal C/POSIX interface can be used.
//

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// TCM_MEMFS_DEBUG is project-controlled: it is defined exactly when the
// project-level TCM_MEMFS_DEBUG switch in the root CMakeLists.txt is ON,
// which propagates to <tcm_config.h> via the #cmakedefine in tcm_config.h.in.
// When OFF we deliberately do NOT force-define it here; the leak-detection
// block in test_leaks() (section 11 below) takes a SKIP branch, so the autotest
// still compiles, links and passes — it just doesn't run the leak-accounting
// checks. The project default is ON, so a normal build runs them.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memfs/memfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Portability: these POSIX constants may not be defined by the host headers.
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x0008
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x20000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x40000
#endif
#ifndef _PC_PATH_MAX
#define _PC_PATH_MAX 4
#endif
#ifndef _PC_NAME_MAX
#define _PC_NAME_MAX 3
#endif

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
#define CHECK_ERRNO(expected_errno, name)                                      \
  do {                                                                         \
    char d_[64];                                                               \
    snprintf(d_, sizeof(d_), "(errno=%d expected=%d)", errno,                  \
             (int)(expected_errno));                                           \
    t_check(errno == (expected_errno), name, d_);                              \
  } while (0)

static void write_all(int fd, const char *buf, size_t len) {
  size_t done = 0;
  while (done < len) {
    ssize_t n = write(fd, buf + done, len - done);
    if (n <= 0)
      return;
    done += (size_t)n;
  }
}

static int file_contents_eq(const char *path, const char *expected) {
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return 0;
  char buf[2048];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n < 0)
    return 0;
  buf[n] = '\0';
  size_t elen = strlen(expected);
  return (size_t)n == elen && memcmp(buf, expected, elen) == 0;
}

static void create_file(const char *path, const char *content) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd >= 0) {
    write_all(fd, content, strlen(content));
    close(fd);
  }
}

static int stat_mode(const char *path) {
  struct stat sb;
  if (stat(path, &sb) != 0)
    return -1;
  return (int)(sb.st_mode & S_IFMT);
}
static off_t stat_size(const char *path) {
  struct stat sb;
  if (stat(path, &sb) != 0)
    return -1;
  return sb.st_size;
}

// ============================================================================
// 1. Packed payload integrity (files baked in by --tcm-memfs)
// ============================================================================
static void test_packed_payload(void) {
  printf("\n== 1. packed payload ==\n");
  CHECK(file_contents_eq("/hello.txt", "Hello, memfs!"), "hello.txt readable");
  CHECK(file_contents_eq("/root.txt", "root file"), "root.txt readable");
  CHECK(file_contents_eq("/config.json",
                         "{\"name\":\"tcmips\",\"features\":[\"memfs\","
                         "\"console\"]}"),
        "config.json readable");
  CHECK(file_contents_eq("/levels/l1.conf",
                         "012345678901234567890123456789012345"
                         "6789012345678901234567890123456789"),
        "levels/l1.conf readable (nested packed file)");

  struct stat sb;
  CHECK_EQ(stat("/hello.txt", &sb), 0, "stat /hello.txt");
  CHECK(S_ISREG(sb.st_mode), "hello.txt is regular");
  CHECK_EQ(sb.st_size, 13, "hello.txt st_size is 13");
  CHECK_EQ(sb.st_nlink, 1, "hello.txt st_nlink is 1");
  CHECK_EQ(sb.st_dev, 1, "hello.txt st_dev is 1");
  CHECK(sb.st_ino != 0, "hello.txt nonzero st_ino");
  CHECK(stat_mode("/levels") == S_IFDIR, "levels is a directory");
  CHECK_EQ(stat_size("/config.json"), 48, "config.json size 48");

  // memfs_map: whenever a file's content is contiguous (a packed asset
  // is, for example) it maps directly to its single RAM region — no
  // open/read/copy required.
  size_t plen = 0;
  const void *p = memfs_map("/hello.txt", &plen);
  CHECK(p != NULL, "map hello.txt non-NULL");
  CHECK_EQ(plen, (size_t)13, "map hello.txt len 13");
  CHECK(p != NULL && memcmp(p, "Hello, memfs!", 13) == 0,
        "map hello.txt content");
  CHECK(memfs_map("/levels", NULL) == NULL, "map directory is NULL");
  CHECK(memfs_map("/no_such_file", NULL) == NULL, "map missing is NULL");
}

// ============================================================================
// 2. Basic open modes & create semantics
// ============================================================================
static void test_open_modes(void) {
  printf("\n== 2. open modes & create semantics ==\n");
  create_file("/m.txt", "0123456789");

  int fd = open("/m.txt", O_RDONLY);
  CHECK(fd >= 0, "open O_RDONLY");
  if (fd >= 0) {
    ssize_t w = write(fd, "x", 1);
    t_check(w < 0 && errno == EBADF, "O_RDONLY fd rejects write",
            "(expected EBADF)");
    close(fd);
  }

  fd = open("/m.txt", O_WRONLY);
  CHECK(fd >= 0, "open O_WRONLY");
  if (fd >= 0) {
    char c;
    ssize_t r = read(fd, &c, 1);
    t_check(r < 0 && errno == EBADF, "O_WRONLY fd rejects read",
            "(expected EBADF)");
    close(fd);
  }

  errno = 0;
  fd = open("/does/not/exist", O_RDONLY);
  CHECK(fd < 0, "open missing without O_CREAT fails");
  CHECK_ERRNO(ENOENT, "missing open errno ENOENT");

  fd = open("/created.txt", O_WRONLY | O_CREAT | O_EXCL, 0644);
  CHECK(fd >= 0, "open O_CREAT|O_EXCL creates file");
  if (fd >= 0)
    close(fd);
  CHECK(access("/created.txt", F_OK) == 0, "created file exists");

  errno = 0;
  fd = open("/created.txt", O_WRONLY | O_CREAT | O_EXCL, 0644);
  CHECK(fd < 0, "open O_CREAT|O_EXCL existing fails");
  CHECK_ERRNO(EEXIST, "O_EXCL on existing errno EEXIST");
  if (fd >= 0)
    close(fd);

  fd = open("/created.txt", O_WRONLY | O_TRUNC);
  CHECK(fd >= 0, "open O_TRUNC existing");
  if (fd >= 0)
    close(fd);
  CHECK_EQ(stat_size("/created.txt"), 0, "O_TRUNC empties file");

  // Two handles share content but keep independent offsets.
  int fda = open("/m.txt", O_RDWR);
  int fdb = open("/m.txt", O_RDWR);
  CHECK(fda >= 0 && fdb >= 0, "two handles same inode open OK");
  if (fda >= 0 && fdb >= 0) {
    CHECK_EQ(write(fda, "ABC", 3), 3, "write via handle A");
    char buf[3];
    lseek(fdb, 0, SEEK_SET);
    CHECK(read(fdb, buf, 3) == 3 && memcmp(buf, "ABC", 3) == 0,
          "handle B sees handle A write");
    lseek(fda, 4, SEEK_SET);
    lseek(fdb, 0, SEEK_SET);
    CHECK_EQ(lseek(fda, 0, SEEK_CUR), 4, "independent offset A");
    CHECK_EQ(lseek(fdb, 0, SEEK_CUR), 0, "independent offset B");
    close(fda);
    close(fdb);
  }

  // O_APPEND writes always land at EOF.
  create_file("/app.txt", "aaa");
  fd = open("/app.txt", O_WRONLY | O_APPEND);
  CHECK(fd >= 0, "open O_APPEND");
  if (fd >= 0) {
    lseek(fd, 0, SEEK_SET);
    write(fd, "bbb", 3);
    close(fd);
  }
  CHECK(file_contents_eq("/app.txt", "aaabbb"), "O_APPEND appends to EOF");

  // Owner-write bit is forced on O_CREAT so later writes are allowed.
  create_file("/mode.txt", "m");
  struct stat msb;
  stat("/mode.txt", &msb);
  CHECK((msb.st_mode & 0222) != 0, "O_CREAT forces owner-write bit");

  unlink("/m.txt");
  unlink("/created.txt");
  unlink("/app.txt");
  unlink("/mode.txt");
}

// ============================================================================
// 3. lseek semantics
// ============================================================================
static void test_lseek(void) {
  printf("\n== 3. lseek ==\n");
  create_file("/seek.txt", "0123456789");
  int fd = open("/seek.txt", O_RDONLY);
  CHECK(fd >= 0, "open seek.txt");
  if (fd < 0)
    return;

  CHECK_EQ(lseek(fd, 0, SEEK_CUR), 0, "initial offset 0");
  CHECK_EQ(lseek(fd, 5, SEEK_SET), 5, "SEEK_SET to 5");
  CHECK_EQ(lseek(fd, 2, SEEK_CUR), 7, "SEEK_CUR +2");
  CHECK_EQ(lseek(fd, -3, SEEK_CUR), 4, "SEEK_CUR -3");
  CHECK_EQ(lseek(fd, -2, SEEK_END), 8, "SEEK_END -2");
  CHECK_EQ(lseek(fd, 0, SEEK_END), 10, "SEEK_END to size");

  char buf[4];
  lseek(fd, 7, SEEK_SET);
  CHECK(read(fd, buf, 3) == 3 && memcmp(buf, "789", 3) == 0,
        "read at offset 7");

  errno = 0;
  CHECK_EQ(lseek(fd, -100, SEEK_END), -1, "lseek before start fails");
  CHECK_ERRNO(EINVAL, "lseek before start errno EINVAL");

  CHECK_EQ(lseek(fd, 50, SEEK_SET), 50, "lseek past EOF allowed");
  errno = 0;
  char z;
  CHECK_EQ(read(fd, &z, 1), 0, "read past EOF returns 0");

  close(fd);
  unlink("/seek.txt");
}

// ============================================================================
// 4. truncate / ftruncate
// ============================================================================
static void test_truncate(void) {
  printf("\n== 4. truncate ==\n");
  create_file("/t.txt", "hello world");
  int fd = open("/t.txt", O_RDWR);
  CHECK(fd >= 0, "open t.txt O_RDWR");
  if (fd >= 0) {
    CHECK_EQ(ftruncate(fd, 5), 0, "ftruncate shrink to 5");
    close(fd);
  }
  CHECK_EQ(stat_size("/t.txt"), 5, "size 5 after shrink");
  CHECK(file_contents_eq("/t.txt", "hello"), "content after shrink");

  fd = open("/t.txt", O_RDWR);
  CHECK_EQ(ftruncate(fd, 12), 0, "ftruncate extend to 12");
  lseek(fd, 5, SEEK_SET);
  char tail[7] = {0};
  ssize_t r = read(fd, tail, 7);
  int zeros = 1;
  for (int i = 0; i < (int)r && i < 7; ++i)
    if (tail[i] != 0)
      zeros = 0;
  CHECK(r == 7 && zeros, "extension bytes are zero");
  close(fd);

  CHECK_EQ(truncate("/t.txt", 2), 0, "truncate(path) to 2");
  CHECK_EQ(stat_size("/t.txt"), 2, "size 2 after truncate(path)");

  CHECK_EQ(truncate("/t.txt", -1), -1, "truncate negative length fails");

  errno = 0;
  CHECK_EQ(ftruncate(55, 4), -1,
           "ftruncate bad fd fails"); // unused file-fd slot
  CHECK_ERRNO(EBADF, "ftruncate bad fd errno EBADF");

  CHECK_EQ(truncate("/levels", 0), -1, "truncate a directory fails");

  unlink("/t.txt");
}

// ============================================================================
// 5. rename
// ============================================================================
static void test_rename(void) {
  printf("\n== 5. rename ==\n");
  create_file("/a.txt", "renamed");
  CHECK_EQ(rename("/a.txt", "/b.txt"), 0, "rename file");
  CHECK(access("/a.txt", F_OK) != 0, "old name gone");
  CHECK(file_contents_eq("/b.txt", "renamed"), "new name has content");

  create_file("/c.txt", "old");
  create_file("/d.txt", "target");
  CHECK_EQ(rename("/c.txt", "/d.txt"), 0, "rename replaces existing file");
  CHECK(file_contents_eq("/d.txt", "old"), "replacement content");

  errno = 0;
  CHECK_EQ(rename("/nope", "/x.txt"), -1, "rename missing fails");
  CHECK_ERRNO(ENOENT, "rename missing errno ENOENT");

  errno = 0;
  CHECK_EQ(rename("/b.txt", "/no/such/dir/x.txt"), -1,
           "rename into missing dir fails");
  CHECK_ERRNO(ENOENT, "rename into missing dir errno ENOENT");

  mkdir("/rd1", 0755);
  mkdir("/rd2", 0755);
  mkdir("/rd3", 0755);
  create_file("/rd2/f.txt", "deep");
  create_file("/rd3/g.txt", "blocker"); // makes /rd3 non-empty
  CHECK_EQ(rename("/rd1", "/rd4"), 0, "rename empty dir");
  CHECK(stat_mode("/rd4") == S_IFDIR, "renamed dir exists as directory");

  // A directory can only overwrite an EMPTY target directory; a non-empty one.
  // must be refused (POSIX ENOTEMPTY).
  errno = 0;
  CHECK_EQ(rename("/rd2", "/rd3"), -1, "rename dir onto nonempty dir fails");
  CHECK_ERRNO(ENOTEMPTY, "rename onto nonempty dir errno ENOTEMPTY");
  CHECK(access("/rd2/f.txt", F_OK) == 0,
        "nonempty source intact after failed move");
  CHECK(access("/rd3/g.txt", F_OK) == 0,
        "nonempty target intact after failed move");

  CHECK_EQ(rename("/rd4", "/rd4/sub"), -1, "rename dir into itself fails");

  unlink("/b.txt");
  unlink("/d.txt");
  unlink("/rd2/f.txt");
  unlink("/rd3/g.txt");
  rmdir("/rd2");
  rmdir("/rd3");
  rmdir("/rd4");
}

// ============================================================================
// 6. Symlinks
// ============================================================================
static void test_symlink(void) {
  printf("\n== 6. symlinks ==\n");
  create_file("/tgt.txt", "target-data");
  CHECK_EQ(symlink("/tgt.txt", "/lnk.txt"), 0, "create symlink");
  struct stat sb;
  CHECK_EQ(lstat("/lnk.txt", &sb), 0, "lstat symlink");
  CHECK(S_ISLNK(sb.st_mode), "lstat sees link type");
  CHECK_EQ(stat("/lnk.txt", &sb), 0, "stat follows symlink");
  CHECK(S_ISREG(sb.st_mode), "stat sees target type");
  CHECK(file_contents_eq("/lnk.txt", "target-data"), "read through symlink");

  char buf[256];
  ssize_t r = readlink("/lnk.txt", buf, sizeof(buf) - 1);
  if (r >= 0)
    buf[r] = '\0';
  CHECK(r >= 0 && strcmp(buf, "/tgt.txt") == 0, "readlink returns target");

  // Self-loop -> ELOOP on open.
  CHECK_EQ(symlink("/loop", "/loop"), 0, "create self symlink");
  errno = 0;
  int fd = open("/loop", O_RDONLY);
  CHECK(fd < 0, "open self-loop fails");
  CHECK_ERRNO(ELOOP, "self-loop open errno ELOOP");
  if (fd >= 0)
    close(fd);

  // Dangling link: lstat works, open fails ENOENT.
  CHECK_EQ(symlink("/nope-target", "/dang.txt"), 0, "create dangling link");
  CHECK_EQ(lstat("/dang.txt", &sb), 0, "lstat dangling link");
  errno = 0;
  fd = open("/dang.txt", O_RDONLY);
  CHECK(fd < 0, "open dangling link fails");
  CHECK_ERRNO(ENOENT, "dangling link open errno ENOENT");

  errno = 0;
  fd = open("/lnk.txt", O_RDONLY | O_NOFOLLOW);
  CHECK(fd < 0, "O_NOFOLLOW open of symlink fails");
  if (fd >= 0)
    close(fd);

  unlink("/lnk.txt");
  unlink("/loop");
  unlink("/dang.txt");
  unlink("/tgt.txt");
}

// ============================================================================
// 7. Directories: mkdir / rmdir / cwd / enumeration
// ============================================================================
static void test_directories(void) {
  printf("\n== 7. directories ==\n");
  struct stat sb;

  CHECK_EQ(mkdir("/d1", 0755), 0, "mkdir /d1");
  CHECK_EQ(stat("/d1", &sb), 0, "stat /d1");
  CHECK(S_ISDIR(sb.st_mode), "d1 is directory");

  errno = 0;
  CHECK_EQ(mkdir("/d1", 0755), -1, "duplicate mkdir fails");
  CHECK_ERRNO(EEXIST, "duplicate mkdir errno EEXIST");

  errno = 0;
  CHECK_EQ(mkdir("/no/such/parent/d", 0755), -1, "mkdir missing parent fails");
  CHECK_ERRNO(ENOENT, "mkdir missing parent errno ENOENT");

  create_file("/d1/f.txt", "x");
  errno = 0;
  CHECK_EQ(rmdir("/d1"), -1, "rmdir non-empty fails");
  CHECK_ERRNO(ENOTEMPTY, "rmdir non-empty errno ENOTEMPTY");

  errno = 0;
  CHECK_EQ(rmdir("/d1/f.txt"), -1, "rmdir a file fails");
  CHECK_ERRNO(ENOTDIR, "rmdir a file errno ENOTDIR");

  errno = 0;
  int fd = open("/d1/f.txt", O_RDONLY | O_DIRECTORY);
  CHECK(fd < 0, "O_DIRECTORY open of file fails");
  if (fd >= 0)
    close(fd);

  // Opening a plain directory (not O_DIRECTORY) is rejected.
  fd = open("/d1", O_RDONLY);
  CHECK(fd < 0, "open directory fails");
  if (fd >= 0)
    close(fd);

  unlink("/d1/f.txt");
  CHECK_EQ(rmdir("/d1"), 0, "rmdir empty dir");

  // cwd + relative paths.
  CHECK_EQ(chdir("/levels"), 0, "chdir /levels");
  char cwd[128];
  char *p = getcwd(cwd, sizeof(cwd));
  CHECK(p == cwd && strcmp(cwd, "/levels") == 0, "getcwd reports /levels");
  CHECK(file_contents_eq("l1.conf", "012345678901234567890123456789012345678901"
                                    "2345678901234567890123456789"),
        "relative open under cwd");
  CHECK_EQ(chdir(".."), 0, "chdir .. back to root");
  CHECK_EQ(chdir("/"), 0, "chdir /");

  // Root enumeration shows packed entries plus "." and "..".
  DIR *d = opendir("/");
  CHECK(d != NULL, "opendir /");
  if (d) {
    int found_hello = 0, found_levels = 0, found_dot = 0, found_dotdot = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
      if (strcmp(e->d_name, ".") == 0)
        found_dot = 1;
      else if (strcmp(e->d_name, "..") == 0)
        found_dotdot = 1;
      else if (strcmp(e->d_name, "hello.txt") == 0)
        found_hello = 1;
      else if (strcmp(e->d_name, "levels") == 0)
        found_levels = 1;
    }
    closedir(d);
    CHECK(found_dot, "readdir yields '.'");
    CHECK(found_dotdot, "readdir yields '..'");
    CHECK(found_hello, "readdir finds hello.txt");
    CHECK(found_levels, "readdir finds levels dir");
  }

  DIR *nested = opendir("/levels");
  CHECK(nested != NULL, "opendir /levels");
  if (nested) {
    int found_conf = 0;
    struct dirent *e;
    while ((e = readdir(nested)) != NULL)
      if (strcmp(e->d_name, "l1.conf") == 0)
        found_conf = 1;
    closedir(nested);
    CHECK(found_conf, "readdir finds levels/l1.conf");
  }
}

// ============================================================================
// 8. Directory fds: dirfd / fdopendir / openat / unlinkat
// ============================================================================
static void test_dirfd_apis(void) {
  printf("\n== 8. dirfd / openat / unlinkat ==\n");
  mkdir("/ddir", 0755);
  create_file("/ddir/f1.txt", "one");
  create_file("/ddir/f2.txt", "two");

  DIR *d = opendir("/ddir");
  CHECK(d != NULL, "opendir /ddir");
  if (!d)
    return;
  int dfd = dirfd(d);
  CHECK(dfd >= 0, "dirfd() gives a synthetic fd");

  int fd = openat(dfd, "f1.txt", O_RDONLY);
  CHECK(fd >= 0, "openat f1.txt via dirfd");
  if (fd >= 0) {
    char buf[8];
    ssize_t r = read(fd, buf, 3);
    buf[r] = '\0';
    CHECK(r == 3 && memcmp(buf, "one", 3) == 0, "openat reads file");
    close(fd);
  }

  // NOTE: unlinkat() must run while `dfd` is still valid. fdopendir(dfd)
  // takes ownership of that synthetic fd, so it is exercised last.
  errno = 0;
  CHECK_EQ(unlinkat(dfd, "f1.txt", 0), 0, "unlinkat removes file");
  CHECK(access("/ddir/f1.txt", F_OK) != 0, "f1.txt gone after unlinkat");

  mkdir("/ddir/sub", 0755);
  errno = 0;
  CHECK_EQ(unlinkat(dfd, "sub", AT_REMOVEDIR), 0,
           "unlinkat AT_REMOVEDIR rmdir");
  CHECK(access("/ddir/sub", F_OK) != 0, "sub gone after unlinkat AT_REMOVEDIR");

  errno = 0;
  CHECK_EQ(unlinkat(dfd, "f2.txt", AT_REMOVEDIR), -1,
           "unlinkat AT_REMOVEDIR on file fails");
  CHECK_ERRNO(ENOTDIR, "unlinkat AT_REMOVEDIR on file errno ENOTDIR");

  // fdopendir adopts the synthetic fd; enumerating through it must yield the
  // surviving child (f2.txt).
  DIR *d2 = fdopendir(dfd);
  CHECK(d2 != NULL, "fdopendir from dirfd");
  if (d2) {
    struct dirent *e;
    int count = 0;
    while ((e = readdir(d2)) != NULL)
      if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
        count++;
    closedir(d2); // consumes dfd
    CHECK(count == 1, "fdopendir enumerates surviving f2.txt");
  }

  CHECK(pathconf("/", _PC_PATH_MAX) == MEMFS_PATH_MAX - 1 ||
            pathconf("/", _PC_PATH_MAX) > 0,
        "pathconf _PC_PATH_MAX sane");
  CHECK(pathconf("/", _PC_NAME_MAX) > 0, "pathconf _PC_NAME_MAX sane");

  closedir(d);
  unlink("/ddir/f2.txt");
  rmdir("/ddir");
}

// ============================================================================
// 9. unlink semantics (incl. unlink-of-open-file)
// ============================================================================
static void test_unlink(void) {
  printf("\n== 9. unlink ==\n");

  create_file("/live.txt", "still-alive");
  int fd = open("/live.txt", O_RDONLY);
  CHECK(fd >= 0, "open live file");
  if (fd >= 0) {
    CHECK_EQ(unlink("/live.txt"), 0, "unlink open file succeeds");
    CHECK(access("/live.txt", F_OK) != 0, "name gone immediately");
    char buf[32];
    ssize_t r = read(fd, buf, 11);
    buf[r] = '\0';
    CHECK(r == 11 && memcmp(buf, "still-alive", 11) == 0,
          "content readable while handle open");
    close(fd); // last close releases it
  }

  errno = 0;
  CHECK_EQ(unlink("/no-such"), -1, "unlink missing fails");
  CHECK_ERRNO(ENOENT, "unlink missing errno ENOENT");

  mkdir("/ddir2", 0755);
  errno = 0;
  CHECK_EQ(unlink("/ddir2"), -1, "unlink directory fails");
  CHECK_ERRNO(EISDIR, "unlink directory errno EISDIR");
  rmdir("/ddir2");
}

// ============================================================================
// 10. Robustness / crash-resistance (invalid inputs must error, not crash)
// ============================================================================
static void test_robustness(void) {
  printf("\n== 10. robustness (no crash) ==\n");

  errno = 0;
  CHECK_EQ(open(NULL, O_RDONLY), -1, "open(NULL) errors");
  CHECK_ERRNO(EINVAL, "open(NULL) errno EINVAL");

  errno = 0;
  CHECK_EQ(access(NULL, F_OK), -1, "access(NULL) errors");

  errno = 0;
  CHECK_EQ(stat(NULL, &(struct stat){0}), -1, "stat(NULL) errors");

  errno = 0;
  CHECK_EQ(mkdir(NULL, 0755), -1, "mkdir(NULL) errors");

  errno = 0;
  CHECK_EQ(rmdir(NULL), -1, "rmdir(NULL) errors");

  errno = 0;
  CHECK_EQ(chdir(NULL), -1, "chdir(NULL) errors");

  errno = 0;
  CHECK_EQ(unlink(NULL), -1, "unlink(NULL) errors");

  errno = 0;
  CHECK_EQ(rename(NULL, "/x"), -1, "rename(NULL, /x) errors");

  errno = 0;
  CHECK_EQ(rename("/x", NULL), -1, "rename(/x, NULL) errors");

  errno = 0;
  CHECK_EQ(symlink(NULL, "/y"), -1, "symlink(NULL, /y) errors");

  errno = 0;
  CHECK_EQ(read(9999, &(char){0}, 1), -1, "read bad fd errors");
  CHECK_ERRNO(EBADF, "read bad fd errno EBADF");

  errno = 0;
  CHECK_EQ(write(9999, "x", 1), -1, "write bad fd errors");

  errno = 0;
  CHECK_EQ(lseek(9999, 0, SEEK_SET), -1, "lseek bad fd errors");

  errno = 0;
  CHECK_EQ(close(9999), -1, "close bad fd errors");

  errno = 0;
  CHECK_EQ(fstat(9999, &(struct stat){0}), -1, "fstat bad fd errors");

  // Read with a buffer larger than the file returns only the file length.
  create_file("/small.txt", "hi");
  int fd = open("/small.txt", O_RDONLY);
  if (fd >= 0) {
    char big[4096];
    CHECK_EQ(read(fd, big, sizeof(big)), 2,
             "read length larger than file returns file size");
    close(fd);
  }
  unlink("/small.txt");

  // Pathological paths (traversal, dots, duplicate slashes) resolve safely.
  CHECK(access("/levels/../hello.txt", F_OK) == 0,
        "normalized traversal resolves");
  CHECK(access("//hello.txt", F_OK) == 0, "duplicate slash resolves");
  CHECK(access("/./hello.txt", F_OK) == 0, "dot segment resolves");
}

// ============================================================================
// 12. fd-based stat (fstat) and link-count observation
// ============================================================================
static void test_fdstat_and_nlink(void) {
  printf("\n== 12. fstat / st_nlink ==\n");
  struct stat sb;

  // fstat on a plain file fd reports size + mode + inode + dev.
  int fd = open("/config.json", O_RDONLY);
  CHECK(fd >= 0, "open config.json for fstat");
  if (fd >= 0) {
    CHECK_EQ(fstat(fd, &sb), 0, "fstat config.json");
    CHECK(S_ISREG(sb.st_mode), "fstat sees regular file");
    CHECK_EQ(sb.st_size, 48, "fstat size 48");
    CHECK_EQ(sb.st_nlink, 1, "fstat nlink 1");
    CHECK_EQ(sb.st_dev, 1, "fstat dev 1");
    close(fd);
  }

  // fstat on an O_RDWR fd opened then truncated must show the new size.
  create_file("/nlink.txt", "0123456789");
  fd = open("/nlink.txt", O_RDWR);
  CHECK(fd >= 0, "open nlink.txt O_RDWR");
  if (fd >= 0) {
    ftruncate(fd, 4);
    CHECK_EQ(fstat(fd, &sb), 0, "fstat after ftruncate");
    CHECK_EQ(sb.st_size, 4, "fstat reflects truncated size");
    close(fd);
  }

  // Directory nlink: root bucket of a fresh dir is 2, and the child count
  // grows it (memfs bumps nlink when a child is linked, root excluded).
  mkdir("/nl_dir", 0755);
  CHECK_EQ(stat("/nl_dir", &sb), 0, "stat nl_dir");
  long base_nlink = (long)sb.st_nlink;
  CHECK(base_nlink >= 2, "new dir nlink >= 2");
  create_file("/nl_dir/a.txt", "a");
  CHECK_EQ(stat("/nl_dir", &sb), 0, "stat nl_dir after child");
  CHECK(sb.st_nlink == base_nlink + 1,
        "dir nlink increases when a child is added");
  unlink("/nl_dir/a.txt");
  CHECK_EQ(stat("/nl_dir", &sb), 0, "stat nl_dir after child removed");
  CHECK(sb.st_nlink == base_nlink, "dir nlink returns after child removed");
  rmdir("/nl_dir");

  unlink("/nlink.txt");
}

// ============================================================================
// 13. Content boundaries: segment0 / chunk edge, zero-fill, integrity
// ============================================================================
static void test_content_boundaries(void) {
  printf("\n== 13. content boundaries ==\n");

  // Writing beyond the packed segment0 appends Chunk nodes; reads outside the
  // logical size return 0, and bytes we never wrote in a chunk are zero.
  const int big = 20000; // well past seg0 (which is 0 for a new file)
  int fd = open("/edge.bin", O_WRONLY | O_CREAT, 0644);
  CHECK(fd >= 0, "open edge.bin");
  if (fd >= 0) {
    // Write only a few bytes far into the file via a single large write of a
    // mostly-zero buffer to test the chunk chain and zero-fill.
    char *buf = (char *)malloc(big);
    if (buf) {
      memset(buf, 0, big);
      memset(buf, 0xAA, 8);         // 8 marker bytes at offset 0
      memset(buf + 4096, 0xBB, 8);  // 8 marker bytes spanning chunk boundary
      memset(buf + 8191, 0xCC, 16); // straddles the boundary between two chunks
      ssize_t w = write(fd, buf, big);
      CHECK_EQ(w, big, "write 20000 bytes");
      free(buf);
    }
    close(fd);
  }
  struct stat sb;
  CHECK_EQ(stat("/edge.bin", &sb), 0, "stat edge.bin");
  CHECK_EQ(sb.st_size, big, "edge.bin size 20000");

  fd = open("/edge.bin", O_RDONLY);
  CHECK(fd >= 0, "reopen edge.bin read");
  if (fd >= 0) {
    char r[24];
    // marker at 0
    lseek(fd, 0, SEEK_SET);
    CHECK_EQ(read(fd, r, 8), 8, "read header marker len");
    CHECK(r[0] == (char)0xAA && r[1] == (char)0xAA, "marker at 0 intact");
    // zero-fill around offset 8
    lseek(fd, 8, SEEK_SET);
    CHECK_EQ(read(fd, r, 8), 8, "read zero-fill region len");
    int allzero = 1;
    for (int i = 0; i < 8; ++i)
      if (r[i] != 0) {
        allzero = 0;
        break;
      }
    CHECK(allzero, "untouched chunk bytes are zero");
    // marker at 4096 (first chunk boundary)
    lseek(fd, 4096, SEEK_SET);
    CHECK_EQ(read(fd, r, 8), 8, "read chunk-1 marker len");
    CHECK(r[0] == (char)0xBB && r[1] == (char)0xBB, "marker at 4096 intact");
    // marker straddling chunk boundary 8191..8206
    lseek(fd, 8191, SEEK_SET);
    CHECK_EQ(read(fd, r, 16), 16, "read straddle marker len");
    CHECK(r[0] == (char)0xCC, "straddle marker start intact");
    CHECK(r[15] == (char)0xCC, "straddle marker end intact");
    // read at logical EOF yields 0 and does not advance into garbage
    lseek(fd, big, SEEK_SET);
    char one;
    CHECK_EQ(read(fd, &one, 1), 0, "read at EOF returns 0");
    close(fd);
  }

  // A single pass of random reads after writes must return exact bytes.
  create_file("/ow.txt", "0123456789");
  fd = open("/ow.txt", O_RDWR);
  CHECK(fd >= 0, "open ow.txt O_RDWR");
  if (fd >= 0) {
    lseek(fd, 2, SEEK_SET);
    CHECK_EQ(write(fd, "XYZ", 3), 3, "overwrite middle");
    close(fd);
  }
  CHECK(file_contents_eq("/ow.txt", "01XYZ56789"), "overwrite exact bytes");

  // Packed-file truncate-shrink edge: node_read must not read past logical_size
  // even though the packed seg0 is larger (logical_size < seg0_size after
  // shrink). /hello.txt is a packed seg0 asset of 13 bytes.
  char hbuf[24];
  memset(hbuf, 0x55, sizeof(hbuf));
  CHECK_EQ(memfs_truncate("/hello.txt", 5), 0, "truncate packed file to 5");
  fd = open("/hello.txt", O_RDONLY);
  CHECK(fd >= 0, "open shrunk packed file");
  if (fd >= 0) {
    ssize_t n = read(fd, hbuf, 20);
    CHECK_EQ(n, 5, "read of shrunk packed file returns 5");
    CHECK(hbuf[0] == 'H' && hbuf[4] == 'o', "shrunk content intact");
    CHECK(hbuf[5] == (char)0x55, "does not read past logical_size");
    close(fd);
  }
  // seek past the shrunk logical_size (but still inside the packed seg0) must
  // read nothing: node_read's seg0 branch must not underflow
  // `logical_size - pos` and spill bytes past the logical EOF.
  fd = open("/hello.txt", O_RDONLY);
  CHECK(fd >= 0, "reopen shrunk packed file");
  if (fd >= 0) {
    memset(hbuf, 0x55, sizeof(hbuf));
    CHECK_EQ(lseek(fd, 10, SEEK_SET), 10, "seek to 10 (beyond logical EOF=5)");
    ssize_t n = read(fd, hbuf, 4);
    CHECK_EQ(n, 0, "read past shrunk EOF returns 0");
    CHECK(hbuf[0] == (char)0x55, "no bytes read past logical_size");
    close(fd);
  }
  CHECK_EQ(memfs_truncate("/hello.txt", 13), 0,
           "restore packed file size to 13");
  CHECK_EQ(stat_size("/hello.txt"), 13, "packed file size back to 13");

  unlink("/edge.bin");
  unlink("/ow.txt");
}

// ============================================================================
// 14. Path resolution corners: mixed separators, chains, relative symlinks
// ============================================================================
static void test_path_corners(void) {
  printf("\n== 14. path corners ==\n");

  // Mixed separators and dot segments all resolve to the same node.
  CHECK(access("/levels/./l1.conf", F_OK) == 0, "a/./b resolves");
  CHECK(access("/levels//l1.conf", F_OK) == 0, "a//b resolves");
  CHECK(access("/root.txt", F_OK) == 0, "root.txt exists");

  // A path that over-normalizes to root is valid and points at the directory.
  struct stat sb;
  CHECK_EQ(stat("/../../..", &sb), 0, "excess .. normalizes to root");
  CHECK(S_ISDIR(sb.st_mode), "root is a directory");

  // Chained symlinks: hello -> target1 -> target2 (deep, under depth cap).
  create_file("/deep_target", "deep-data");
  CHECK_EQ(symlink("/deep_target", "/m1"), 0, "link m1");
  CHECK_EQ(symlink("/m1", "/m2"), 0, "link m2 (chained)");
  CHECK_EQ(symlink("/m2", "/m3"), 0, "link m3 (chained)");
  CHECK(file_contents_eq("/m3", "deep-data"),
        "3-level chained symlink resolves");

  // Symlink with a RELATIVE target is resolved against the cwd-at-lookup, not
  // the directory of the link. Because our cwd is "/" here, a relative target
  // "deep_target" resolves relative to root.
  CHECK_EQ(symlink("deep_target", "/rel_link"), 0, "create relative symlink");
  CHECK(file_contents_eq("/rel_link", "deep-data"),
        "relative-target symlink resolves against cwd");

  // Dangling chain step fails with ENOENT.
  CHECK_EQ(symlink("/no_such_leaf", "/dangle2"), 0, "dangling chain link");
  errno = 0;
  int fd = open("/dangle2", O_RDONLY);
  CHECK(fd < 0, "open dangling chain fails");
  CHECK_ERRNO(ENOENT, "dangling chain open errno ENOENT");
  if (fd >= 0)
    close(fd);

  // opendir("") aliases the root.
  DIR *d = opendir("");
  CHECK(d != NULL, "opendir() with empty path");
  if (d) {
    int found = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
      if (strcmp(e->d_name, "hello.txt") == 0)
        found = 1;
    closedir(d);
    CHECK(found, "empty-path opendir lists root children");
  }

  // chdir down then normalize ".." twice back to root.
  CHECK_EQ(chdir("/levels"), 0, "chdir /levels");
  CHECK(access("../hello.txt", F_OK) == 0, ".. resolves to parent of cwd");
  CHECK_EQ(chdir("../.."), 0, "chdir ../.. normalizes to root");
  CHECK(access("/hello.txt", F_OK) == 0, "back at root");

  unlink("/deep_target");
  unlink("/m1");
  unlink("/m2");
  unlink("/m3");
  unlink("/rel_link");
  unlink("/dangle2");
}

// ============================================================================
// 15. Name / path length limits
// ============================================================================
static void test_length_limits(void) {
  printf("\n== 15. name/path length limits ==\n");

  // NAME_MAX = 256 (incl NUL) => a 255-byte name is ok, 256 is not.
  char name255[MEMFS_NAME_MAX + 2];
  memset(name255, 'n', MEMFS_NAME_MAX - 1); // 255 usable bytes
  name255[MEMFS_NAME_MAX - 1] = '\0';
  char pathbuf[MEMFS_PATH_MAX * 2];
  snprintf(pathbuf, sizeof(pathbuf), "/%s", name255);

  int fd = open(pathbuf, O_WRONLY | O_CREAT | O_EXCL, 0644);
  CHECK(fd >= 0, "open 255-char name succeeds");
  if (fd >= 0)
    close(fd);
  CHECK(access(pathbuf, F_OK) == 0, "255-char name is addressable");
  CHECK_EQ(unlink(pathbuf), 0, "unlink 255-char name");

  // A 256-byte name (>= MEMFS_NAME_MAX, incl NUL) must be rejected.
  char name256[MEMFS_NAME_MAX + 2];
  memset(name256, 'x', MEMFS_NAME_MAX); // 256 bytes + NUL
  name256[MEMFS_NAME_MAX] = '\0';
  snprintf(pathbuf, sizeof(pathbuf), "/%s", name256);
  errno = 0;
  fd = open(pathbuf, O_WRONLY | O_CREAT | O_EXCL, 0644);
  CHECK(fd < 0, "open 256-char name fails");
  if (fd >= 0)
    close(fd);

  // Deep nesting: several levels under root.
  CHECK_EQ(mkdir("/d1", 0755), 0, "mkdir /d1");
  CHECK_EQ(mkdir("/d1/d2", 0755), 0, "mkdir /d1/d2");
  CHECK_EQ(mkdir("/d1/d2/d3", 0755), 0, "mkdir /d1/d2/d3");
  create_file("/d1/d2/d3/x.txt", "leaf");
  CHECK(file_contents_eq("/d1/d2/d3/x.txt", "leaf"), "deep leaf readable");
  unlink("/d1/d2/d3/x.txt");
  rmdir("/d1/d2/d3");
  rmdir("/d1/d2");
  rmdir("/d1");

  // mkdir with a too-long intermediate name fails cleanly.
  errno = 0;
  CHECK_EQ(mkdir("/too/long/path/that/does/not/exist", 0755), -1,
           "mkdir into missing deep path fails");
}

// ============================================================================
// 16. Multi-handle interactions on the same inode
// ============================================================================
static void test_handle_interactions(void) {
  printf("\n== 16. multi-handle interactions ==\n");

  // One handle truncates while a second observes the change (shared inode).
  create_file("/shared.txt", "0123456789");
  int a = open("/shared.txt", O_RDWR);
  int b = open("/shared.txt", O_RDONLY);
  CHECK(a >= 0 && b >= 0, "two handles opened");
  if (a >= 0 && b >= 0) {
    CHECK_EQ(ftruncate(a, 4), 0, "handle A truncates to 4");
    struct stat sb;
    CHECK_EQ(fstat(b, &sb), 0, "handle B fstat");
    CHECK_EQ(sb.st_size, 4, "handle B sees truncated size");
    char r[8];
    CHECK_EQ(read(b, r, 8), 4, "handle B reads only 4 bytes");
    close(a);
    close(b);
  }

  // O_APPEND writes by two handles both land at EOF.
  create_file("/ap2.txt", "aa");
  a = open("/ap2.txt", O_WRONLY | O_APPEND);
  b = open("/ap2.txt", O_WRONLY | O_APPEND);
  CHECK(a >= 0 && b >= 0, "two append handles");
  if (a >= 0 && b >= 0) {
    CHECK_EQ(write(a, "b", 1), 1, "append via A");
    CHECK_EQ(write(b, "c", 1), 1, "append via B");
    close(a);
    close(b);
  }
  CHECK(file_contents_eq("/ap2.txt", "aabc"), "append handles concatenate");

  unlink("/shared.txt");
  unlink("/ap2.txt");
}

// ============================================================================
// 17. Directory mutation during iteration + dir-not-dir errors
// ============================================================================
static void test_dir_mutation(void) {
  printf("\n== 17. dir mutation & type errors ==\n");

  // opendir on a regular file fails; POSIX allows either ENOENT or ENOTDIR here
  // (the BSP stub reports a single ENOENT, see stub.c opendir).
  errno = 0;
  DIR *d = opendir("/config.json");
  CHECK(d == NULL, "opendir on a file returns NULL");
  t_check(errno == ENOENT || errno == ENOTDIR,
          "opendir-file errno ENOENT/ENOTDIR", NULL);

  // opendir on a non-existent path fails (ENOENT).
  errno = 0;
  d = opendir("/no/such/dir");
  CHECK(d == NULL, "opendir on missing path returns NULL");
  CHECK_ERRNO(ENOENT, "opendir-missing errno ENOENT");

  // mkdir where a parent component is a FILE -> ENOTDIR.
  errno = 0;
  CHECK_EQ(mkdir("/config.json/sub", 0755), -1, "mkdir under a file fails");
  CHECK_ERRNO(ENOTDIR, "mkdir-under-file errno ENOTDIR");

  // Enumerate a directory and remove a child mid-iteration: it must be
  // skipped consistently and subsequent entries still appear.
  mkdir("/mut", 0755);
  create_file("/mut/keep.txt", "k");
  create_file("/mut/doomed.txt", "d");
  create_file("/mut/also.txt", "a");
  DIR *it = opendir("/mut");
  CHECK(it != NULL, "opendir /mut");
  if (it) {
    struct dirent *e;
    while ((e = readdir(it)) != NULL) {
      if (strcmp(e->d_name, "doomed.txt") == 0) {
        unlink("/mut/doomed.txt"); // delete a child while we are iterating
      }
      if (strcmp(e->d_name, "also.txt") == 0) {
        create_file("/mut/#new#.txt", "x"); // create while iterating
      }
    }
    closedir(it);
  }
  // Regardless of iteration order, deterministic final state:
  CHECK(access("/mut/keep.txt", F_OK) == 0, "keep.txt survives");
  CHECK(access("/mut/doomed.txt", F_OK) != 0,
        "doomed.txt was deleted mid-iteration");
  CHECK(access("/mut/also.txt", F_OK) == 0, "also.txt survives");
  CHECK(access("/mut/#new#.txt", F_OK) == 0, "created child exists");
  unlink("/mut/keep.txt");
  unlink("/mut/doomed.txt");
  unlink("/mut/also.txt");
  unlink("/mut/#new#.txt");
  rmdir("/mut");
}

// ============================================================================
// 11. Memory-leak detection via TCM_MEMFS_DEBUG accounting
// ============================================================================
#ifdef TCM_MEMFS_DEBUG
// helper: open(path, O_CREAT|O_TRUNC), write nbytes of 0x77, close; return
// written.
static long fill_path_with_bytes(const char *path, size_t nbytes) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    return -1;
  char blob[512];
  memset(blob, 0x77, sizeof(blob));
  size_t done = 0;
  while (done < nbytes) {
    size_t chunk = nbytes - done;
    if (chunk > sizeof(blob))
      chunk = sizeof(blob);
    ssize_t w = write(fd, blob, chunk);
    if (w <= 0)
      break;
    done += (size_t)w;
  }
  close(fd);
  return (long)done;
}

static void test_leaks(void) {
  printf("\n== 11. leaks (TCM_MEMFS_DEBUG) ==\n");

  size_t base_heap = memfs_debug_heap_bytes();
  size_t base_chunks = memfs_debug_chunk_count();

  // Create/write/unlink many files whose content allocates Chunk chains; every
  // chunk must be freed once the inode is gone (unlink with no open handles).
  for (int i = 0; i < 32; ++i) {
    char path[64];
    snprintf(path, sizeof(path), "/leak_%d.bin", i);
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
      continue;
    char blob[300]; // > the fixed Chunk size triggers chunk allocation
    memset(blob, 0x41 + (i & 15), sizeof(blob));
    write_all(fd, blob, sizeof(blob));
    close(fd);
    CHECK_EQ(unlink(path), 0, "unlink leak file");
  }

  CHECK(memfs_debug_heap_bytes() <= base_heap,
        "heap does not grow after create+unlink cycles");
  CHECK(memfs_debug_chunk_count() <= base_chunks,
        "chunks all freed after unlink cycles");

  // Truncate shrinks a big file: trailing Chunk nodes must be released.
  CHECK_EQ(fill_path_with_bytes("/bigfile.bin", 20000), 20000,
           "grow big file to many chunks");
  size_t grown_chunks = memfs_debug_chunk_count();
  CHECK(grown_chunks > 0, "large file holds chunk nodes");
  CHECK_EQ(truncate("/bigfile.bin", 4), 0, "truncate big file small");
  size_t after_trunc_chunks = memfs_debug_chunk_count();
  CHECK(after_trunc_chunks < grown_chunks, "truncate releases trailing chunks");
  CHECK_EQ(unlink("/bigfile.bin"), 0, "unlink big file");

  // A repeated init() is a full reset: heap and chunks return to baseline.
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0,
        "memfs_init_with_payload reset");
  CHECK(memfs_debug_heap_bytes() <= base_heap, "reset does not leak heap");
  CHECK(memfs_debug_chunk_count() <= base_chunks, "reset does not leak chunks");

  // After reset the tree is empty again but still usable for new files.
  errno = 0;
  CHECK(open("/hello.txt", O_RDONLY) < 0, "packed file gone after re-init");
  CHECK_ERRNO(ENOENT, "post-reset open errno ENOENT");
}
#else
// When the project-level TCM_MEMFS_DEBUG switch is OFF, libtcmips_memfs is
// built without the debug counters and <memfs/memfs.h> does not declare
// memfs_debug_heap_bytes / memfs_debug_chunk_count / memfs_debug_dump_tree, so
// we can't invoke the leak-accounting checks here. The SKIP line below tells
// the operator exactly why leak detection did not run; the rest of the suite
// still compiles, links and passes.
static void test_leaks(void) {
  printf("\n== 11. leaks (TCM_MEMFS_DEBUG) == SKIPPED ==\n");
  printf("[SELF] %s SKIP (%s)\n", "memfs leak accounting",
         "project built without TCM_MEMFS_DEBUG");
}
#endif

// ============================================================================
int main(void) {
  printf("=== memfs AUTOTEST ===\n");

  test_packed_payload();
  test_open_modes();
  test_lseek();
  test_truncate();
  test_rename();
  test_symlink();
  test_directories();
  test_dirfd_apis();
  test_unlink();
  test_robustness();
  test_fdstat_and_nlink();
  test_content_boundaries();
  test_path_corners();
  test_length_limits();
  test_handle_interactions();
  test_dir_mutation();
  test_leaks(); // must run last: it resets the filesystem

#ifdef TCM_MEMFS_DEBUG
  printf("[memfs-debug] heap=%zu bytes, chunks=%zu\n", memfs_debug_heap_bytes(),
         memfs_debug_chunk_count());
#else
  printf("[memfs-debug] leak accounting skipped "
         "(project built without TCM_MEMFS_DEBUG)\n");
#endif

  printf("[RESULT] pass=%d fail=%d\n", g_pass, g_fail);
  printf("%s\n", g_fail == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
  return g_fail == 0 ? 0 : 1; // emulator reports this as "!! TCMIPS EMULATOR
                              // EXIT, code=N !!"
}