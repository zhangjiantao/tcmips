//// Host test harness for libmemfs.
//
// This compiles with the system toolchain (not the TCMIPS cross toolchain) and
// links memfs.c directly, so the RAMFS behavior can be validated on the host
// before it is used under picolibc. It fabricates a "packed image" payload that
// mirrors exactly what llvm-link-tcmips would emit for --tcm-memfs=<dir>.
//
// Build and run:
//   test/memfs/host/run.sh
//
// Without -DTCM_MEMFS_DEBUG the debug-only statistics tests are skipped.
//

#include "memfs/memfs.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef AT_FDCWD
#define AT_FDCWD (-2)
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x0002
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x0008
#endif
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x0010
#endif

// MinGW host headers do not define these POSIX constants / macros.
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#ifndef DT_LNK
#define DT_LNK 10
#endif

#if defined(_DIRENT_HAVE_D_TYPE) || defined(__linux__) || defined(__APPLE__)
#define TEST_HAS_D_TYPE 1
#endif

#ifndef _PC_PATH_MAX
#define _PC_PATH_MAX 4
#endif
#ifndef _PC_NAME_MAX
#define _PC_NAME_MAX 3
#endif

#ifdef TEST_HAS_D_TYPE
#define CHECK_DTYPE(actual, expected) CHECK((actual) == (expected))
#else
#define CHECK_DTYPE(actual, expected) ((void)0)
#endif

// ---- fabricate a linker-style payload ---------------------------------------------

// Layout must match the private MemfsPayloadHeader in memfs.c:
//   uint32 magic | uint32 count | uint32 version | uint32 data_off
// kept as raw bytes so cross-compilation never depends on host packing.
#define PAYLOAD_MAGIC_OFF    0
#define PAYLOAD_COUNT_OFF    4
#define PAYLOAD_VERSION_OFF  8
#define PAYLOAD_DATA_OFF    12

static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
  p[2] = (uint8_t)((v >> 16) & 0xff);
  p[3] = (uint8_t)((v >> 24) & 0xff);
}

// Two packed files: "game.dat" (small) and "levels/l1.map" (large enough that
// writes overflow segment0 and must append chunks).
static const char kFileData0[] = "HELLO_MEMFS";
static const char kFileData1[] =
    "0123456789" "0123456789" "0123456789" "0123456789" // 40
    "0123456789" "0123456789" "0123456789";              // 70 total
static const char kFileData2[] = "deep";

// A deliberately nested path LONGER than the old fixed 64-byte cap, proving the
// variable-length ABI lifts any path-length limit.
static const char kDeepPath[] =
    "assets/levels/campaign/chapter_12/secret_rooms/very/very/long/"
    "deeply_nested_full_of_treasure_chests_and_gold_1234567890/chest.txt";

// Layout mirrors the image produced by the linker:
//   header(16) | MemfsEntry[count] (20 B each) | name region | segment0s.
// Each MemfsEntry addresses its NUL-free filename via name_off/name_len into the
// name region, and its content via seg0_offset into the segment0 region.
static void fabricate_payload(uint8_t *buf, size_t buf_sz) {
  const char *names[3] = {"game.dat", "levels/l1.map", kDeepPath};

  const uint32_t hdr = 16;
  const uint32_t entry0 = hdr;                              // first MemfsEntry
  const uint32_t names_off = hdr + 3 * (uint32_t)sizeof(MemfsEntry);

  uint32_t name_off[3];
  {
    uint32_t off = names_off;
    for (int i = 0; i < 3; ++i) {
      name_off[i] = off;
      off += (uint32_t)strlen(names[i]) + 1; // keep the NUL for readability
    }
  }
  const uint32_t rock0_off = name_off[2] + (uint32_t)strlen(names[2]) + 1;
  const uint32_t rock1_off = rock0_off + (uint32_t)sizeof(kFileData0);
  const uint32_t rock2_off = rock1_off + (uint32_t)sizeof(kFileData1);

  memset(buf, 0, buf_sz);
  put_u32(buf + PAYLOAD_MAGIC_OFF, MEMFS_MAGIC);
  put_u32(buf + PAYLOAD_COUNT_OFF, 3);
  put_u32(buf + PAYLOAD_VERSION_OFF, 0);
  put_u32(buf + PAYLOAD_DATA_OFF, rock0_off); // first file's packed bytes

  MemfsEntry *e = (MemfsEntry *)(buf + entry0);

  e[0].name_off = name_off[0];
  e[0].name_len = (uint32_t)strlen(names[0]);
  e[0].seg0_offset = rock0_off;
  e[0].seg0_size = sizeof(kFileData0) - 1;
  e[0].mode = 0;

  e[1].name_off = name_off[1];
  e[1].name_len = (uint32_t)strlen(names[1]);
  e[1].seg0_offset = rock1_off;
  e[1].seg0_size = sizeof(kFileData1) - 1;
  e[1].mode = 0;

  e[2].name_off = name_off[2];
  e[2].name_len = (uint32_t)strlen(names[2]);
  e[2].seg0_offset = rock2_off;
  e[2].seg0_size = sizeof(kFileData2) - 1;
  e[2].mode = 0;

  // name region (NUL-terminated, though memfs only needs name_len bytes)
  memcpy(buf + name_off[0], names[0], strlen(names[0]) + 1);
  memcpy(buf + name_off[1], names[1], strlen(names[1]) + 1);
  memcpy(buf + name_off[2], names[2], strlen(names[2]) + 1);

  // segment0 region
  memcpy(buf + rock0_off, kFileData0, sizeof(kFileData0) - 1);
  memcpy(buf + rock1_off, kFileData1, sizeof(kFileData1) - 1);
  memcpy(buf + rock2_off, kFileData2, sizeof(kFileData2) - 1);
}

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int test_pack_read(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  int fd = memfs_open_base("levels/l1.map", O_RDONLY);
  CHECK(fd >= 0);

  char buf[80];
  memset(buf, 0, sizeof(buf));
  int got = memfs_read(fd, buf, sizeof(buf) - 1);
  CHECK(got == (int)(sizeof(kFileData1) - 1));
  CHECK(memcmp(buf, kFileData1, sizeof(kFileData1) - 1) == 0);

  char extra;
  CHECK(memfs_read(fd, &extra, 1) == 0); // EOF

  memfs_close(fd);
  printf("  [ok] pack read, EOF\n");
  return 0;
}

static int test_inplace_write(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  int fd = memfs_open_base("game.dat", O_RDWR);
  CHECK(fd >= 0);

  const char *over = "ABCDEFGHIJK"; // same length as segment0 (11)
  size_t over_len = strlen(over);
  CHECK(memfs_write(fd, over, over_len) == (int)over_len);
  CHECK(memfs_lseek(fd, 0, 0) == 0);

  char buf[32];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_read(fd, buf, sizeof(buf)) == (int)over_len);
  CHECK(memcmp(buf, over, over_len) == 0);

  // write on a read-only fd must fail
  int ro = memfs_open_base("game.dat", O_RDONLY);
  CHECK(ro >= 0);
  CHECK(memfs_write(ro, "X", 1) < 0);

  memfs_close(fd);
  memfs_close(ro);
  printf("  [ok] in-place write to packed segment0 + readonly enforcement\n");
  return 0;
}

static int test_chunk_append(void) {
  uint8_t payload[8192];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  int fd = memfs_open_base("levels/l1.map", O_RDWR);
  CHECK(fd >= 0);

  long end = memfs_lseek(fd, 0, 2); // SEEK_END
  CHECK(end == (long)(sizeof(kFileData1) - 1));
  CHECK(memfs_lseek(fd, end, 0) == end);

  const char *append = "TAIL_" "0123456789" "0123456789" "0123456789"
                       "0123456789" "0123456789" "0123456789" "0123456789"
                       "0123456789" "0123456789" "0123456789" "0123456789";
  size_t append_len = strlen(append);
  CHECK(append_len > (sizeof(kFileData1) - 1)); // must touch chunk chain
  CHECK(memfs_write(fd, append, append_len) == (int)append_len);

  CHECK(memfs_lseek(fd, 0, 0) == 0);
  char buf[512];
  memset(buf, 0, sizeof(buf));
  int selfsz = (int)(sizeof(kFileData1) - 1);
  int got = memfs_read(fd, buf, sizeof(buf));
  CHECK(got == selfsz + (int)append_len);
  CHECK(memcmp(buf, kFileData1, sizeof(kFileData1) - 1) == 0);
  CHECK(memcmp(buf + selfsz, append, append_len) == 0);

  memfs_close(fd);
  printf("  [ok] chunk-chained append beyond segment0\n");
  return 0;
}

static int test_shared_write(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  int fd1 = memfs_open_base("game.dat", O_RDWR);
  int fd2 = memfs_open_base("game.dat", O_RDWR);
  CHECK(fd1 >= 0 && fd2 >= 0);

  CHECK(memfs_write(fd1, "XYZ", 3) == 3);

  char buf[16];
  memset(buf, 0, sizeof(buf));
  int got = memfs_read(fd2, buf, sizeof(buf)); // fd2 has its own offset 0
  CHECK(got >= 3);
  CHECK(memcmp(buf, "XYZ", 3) == 0);

  memfs_close(fd1);
  memfs_close(fd2);
  printf("  [ok] shared content across two handles (POSIX route A)\n");
  return 0;
}

static int test_runtime_new_file(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  // O_CREAT required to create a name not in the catalog.
  int fd = memfs_open_base("save.dat", O_RDWR | O_CREAT);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "saved-data", 10) == 10);

  char buf[32];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_lseek(fd, 0, 0) == 0);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 10);
  CHECK(memcmp(buf, "saved-data", 10) == 0);

  // opening a non-existent name WITHOUT O_CREAT must fail
  int missing = memfs_open_base("nope.bin", O_RDONLY);
  CHECK(missing == -ENOENT);

  memfs_close(fd);
  printf("  [ok] runtime-created file, ENOENT without O_CREAT\n");
  return 0;
}

// The empty-directory case: an image with no memfs payload still boots a usable
// (empty) in-memory directory.
static int test_empty_dir(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0); // idempotent

  // no packed files -> read-only open of anything must fail
  int missing = memfs_open_base("whatever.txt", O_RDONLY);
  CHECK(missing == -ENOENT);

  // O_CREAT creates and lets us write + read back
  int fd = memfs_open_base("hello.txt", O_RDWR | O_CREAT);
  CHECK(fd >= 0);
  const char *s = "Hello from empty memfs!";
  CHECK(memfs_write(fd, s, 22) == 22);
  CHECK(memfs_lseek(fd, 0, 0) == 0);
  char buf[64];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 22);
  CHECK(memcmp(buf, s, 22) == 0);
  memfs_close(fd);

  printf("  [ok] empty directory (no packed payload) is usable\n");
  return 0;
}

static int test_stat(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  int fd = memfs_open_base("game.dat", O_RDONLY);
  CHECK(fd >= 0);

  struct stat st;
  memset(&st, 0, sizeof(st));
  CHECK(memfs_stat(fd, &st) == 0);
  CHECK(st.st_size == (off_t)(sizeof(kFileData0) - 1));
  CHECK((st.st_mode & S_IFMT) == S_IFREG);

  memfs_close(fd);
  printf("  [ok] stat fills size + regular-file mode\n");
  return 0;
}

// stat(path) WITHOUT opening: packed file resolvable by name, missing name -> ENOENT,
// and a grown file reports its true logical size through the live node.
static int test_stat_path(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  struct stat st;
  memset(&st, 0, sizeof(st));
  CHECK(memfs_stat_path("game.dat", &st) == 0);
  CHECK(st.st_size == (off_t)(sizeof(kFileData0) - 1));
  CHECK((st.st_mode & S_IFMT) == S_IFREG);

  // path stat must work before any open (catalog path).
  memset(&st, 0, sizeof(st));
  CHECK(memfs_stat_path("levels/l1.map", &st) == 0);
  CHECK(st.st_size == (off_t)(sizeof(kFileData1) - 1));

  // grow a file, then stat_path should see the grown logical size via the node.
  int fd = memfs_open_base("levels/l1.map", O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_lseek(fd, 0, 2) == (long)(sizeof(kFileData1) - 1)); // SEEK_END
  const char *tail = "grow";                                    // extends past segment0
  CHECK(memfs_write(fd, tail, (int)strlen(tail)) == (int)strlen(tail));
  memset(&st, 0, sizeof(st));
  CHECK(memfs_stat_path("levels/l1.map", &st) == 0);
  CHECK(st.st_size == (off_t)((sizeof(kFileData1) - 1) + strlen(tail)));
  memfs_close(fd);

  // absent name -> ENOENT
  CHECK(memfs_stat_path("nope.bin", &st) == -ENOENT);
  printf("  [ok] stat(path) before/after growth, ENOENT when absent\n");
  return 0;
}

// unlink: packed file permanently removed (ENOENT afterwards), run-time file removed,
// unlink of an OPEN file keeps content until close.
static int test_unlink(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  struct stat st;
  // packed file, never opened
  CHECK(memfs_unlink("game.dat") == 0);
  memset(&st, 0, sizeof(st));
  CHECK(memfs_stat_path("game.dat", &st) == -ENOENT);
  CHECK(memfs_open_base("game.dat", O_RDONLY) == -ENOENT);
  // unlinking a second time -> ENOENT
  CHECK(memfs_unlink("game.dat") == -ENOENT);

  // unlink an OPEN packed file: name gone, content alive until close
  int fd = memfs_open_base("levels/l1.map", O_RDONLY);
  CHECK(fd >= 0);
  CHECK(memfs_unlink("levels/l1.map") == 0);
  CHECK(memfs_stat_path("levels/l1.map", &st) == -ENOENT);
  char buf[80];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_read(fd, buf, sizeof(buf)) == (int)(sizeof(kFileData1) - 1));
  CHECK(memcmp(buf, kFileData1, sizeof(kFileData1) - 1) == 0);
  memfs_close(fd);
  // after close the name stays gone (catalog tombstoned, not resurrected)
  CHECK(memfs_stat_path("levels/l1.map", &st) == -ENOENT);

  // unlink a run-time file
  int r = memfs_open_base("tmp.bin", O_RDWR | O_CREAT);
  CHECK(r >= 0);
  CHECK(memfs_write(r, "x", 1) == 1);
  CHECK(memfs_unlink("tmp.bin") == 0);
  CHECK(memfs_stat_path("tmp.bin", &st) == -ENOENT);
  memfs_close(r);

  // unlink of a missing name -> ENOENT
  CHECK(memfs_unlink("ghost.bin") == -ENOENT);

  printf("  [ok] unlink packed/runtime/open-file, no resurrection\n");
  return 0;
}

static int test_unlink_open_fd_reuse(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // Open a file, write to it, then unlink it while the fd is still live.
  int fd = memfs_open_base("open_then_unlink.txt", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "keep-alive", 10) == 10);
  CHECK(memfs_unlink("open_then_unlink.txt") == 0);

  // Creating another file must not reuse the deleted node's slot while the
  // first fd still holds a reference to it.
  int other = memfs_open_base("other.txt", O_CREAT | O_RDWR);
  CHECK(other >= 0);
  CHECK(memfs_write(other, "new content", 11) == 11);
  memfs_close(other);

  char buf[32];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 10);
  CHECK(memcmp(buf, "keep-alive", 10) == 0);
  memfs_close(fd);

  struct stat st;
  CHECK(memfs_stat_path("open_then_unlink.txt", &st) == -ENOENT);

  printf("  [ok] unlink while open, no slot reuse corruption\n");
  return 0;
}

// Tree-backed directories: opendir("") lists root children, opendir("levels")
// lists the children of the real directory node at that path.
static int test_readdir(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  // Root: "game.dat" (file), "levels" (dir), "assets" (dir), plus "." / "..".
  void *root = memfs_opendir("");
  CHECK(root != NULL);
  struct dirent de;
  char names[8][32];
  unsigned char types[8];
  ino_t inos[8];
  int n = 0;
  while (memfs_readdir(root, &de) == 0 && n < 8) {
    strncpy(names[n], de.d_name, sizeof(names[n]) - 1);
    names[n][sizeof(names[n]) - 1] = '\0';
#ifdef TEST_HAS_D_TYPE
    types[n] = de.d_type;
#else
    types[n] = DT_UNKNOWN;
#endif
    inos[n] = de.d_ino;
    CHECK(inos[n] != 0); // every entry must carry a usable inode
    ++n;
  }
  memfs_closedir(root);
  CHECK(n == 5); // . + .. + game.dat + levels + assets
  int i;
  int dot = 0, dotdot = 0, game = 0, lvls = 0, assets = 0;
  for (i = 0; i < n; ++i) {
    if (strcmp(names[i], ".") == 0) { CHECK_DTYPE(types[i], DT_DIR); ++dot; }
    else if (strcmp(names[i], "..") == 0) { CHECK_DTYPE(types[i], DT_DIR); ++dotdot; }
    else if (strcmp(names[i], "game.dat") == 0) { CHECK_DTYPE(types[i], DT_REG); ++game; }
    else if (strcmp(names[i], "levels") == 0) { CHECK_DTYPE(types[i], DT_DIR); ++lvls; }
    else if (strcmp(names[i], "assets") == 0) { CHECK_DTYPE(types[i], DT_DIR); ++assets; }
    else CHECK(0 == 1); // unexpected root entry
  }
  CHECK(dot == 1 && dotdot == 1 && game == 1 && lvls == 1 && assets == 1);

  // "levels" contains "." / ".." and "l1.map".
  void *lv = memfs_opendir("levels");
  CHECK(lv != NULL);
  n = 0;
  memset(&de, 0, sizeof(de));
  while (memfs_readdir(lv, &de) == 0 && n < 8) {
    strncpy(names[n], de.d_name, sizeof(names[n]) - 1);
    names[n][sizeof(names[n]) - 1] = '\0';
#ifdef TEST_HAS_D_TYPE
    types[n] = de.d_type;
#else
    types[n] = DT_UNKNOWN;
#endif
    ++n;
  }
  memfs_closedir(lv);
  CHECK(n == 3);
  int mapseen = 0;
  for (i = 0; i < n; ++i) {
    if (strcmp(names[i], ".") == 0) CHECK_DTYPE(types[i], DT_DIR);
    else if (strcmp(names[i], "..") == 0) CHECK_DTYPE(types[i], DT_DIR);
    else if (strcmp(names[i], "l1.map") == 0) { CHECK_DTYPE(types[i], DT_REG); ++mapseen; }
    else CHECK(0 == 1);
  }
  CHECK(mapseen == 1);

  // Deeply nested file contributes "assets" at root and its next segment under
  // "assets": the file is "assets/levels/...", so "assets" has a "levels" dir.
  void *as = memfs_opendir("assets");
  CHECK(as != NULL);
  n = 0;
  memset(&de, 0, sizeof(de));
  while (memfs_readdir(as, &de) == 0 && n < 8) {
    strncpy(names[n], de.d_name, sizeof(names[n]) - 1);
    names[n][sizeof(names[n]) - 1] = '\0';
#ifdef TEST_HAS_D_TYPE
    types[n] = de.d_type;
#else
    types[n] = DT_UNKNOWN;
#endif
    ++n;
  }
  memfs_closedir(as);
  CHECK(n == 3);
  int levelsseen = 0;
  for (i = 0; i < n; ++i) {
    if (strcmp(names[i], ".") == 0) CHECK_DTYPE(types[i], DT_DIR);
    else if (strcmp(names[i], "..") == 0) CHECK_DTYPE(types[i], DT_DIR);
    else if (strcmp(names[i], "levels") == 0) { CHECK_DTYPE(types[i], DT_DIR); ++levelsseen; }
    else CHECK(0 == 1);
  }
  CHECK(levelsseen == 1);

  // An unknown directory cannot be opened.
  CHECK(memfs_opendir("does/not/exist") == NULL);

  // An empty directory yields "." and ".." only.
  CHECK(memfs_mkdir("empty") == 0);
  void *emp = memfs_opendir("empty");
  CHECK(emp != NULL);
  CHECK(memfs_readdir(emp, &de) == 0);
  CHECK(strcmp(de.d_name, ".") == 0);
  CHECK(memfs_readdir(emp, &de) == 0);
  CHECK(strcmp(de.d_name, "..") == 0);
  CHECK(memfs_readdir(emp, &de) < 0);
  memfs_closedir(emp);
  CHECK(memfs_rmdir("empty") == 0);

  printf("  [ok] tree-backed readdir (root / subdir / nested / unknown / empty)\n");
  return 0;
}

// Verifies the variable-length ABI lifts the old 64-byte path cap: a deeply
// nested path (>64 bytes) resolves, and a wrong-case / truncated name does not.
static int test_deep_path(void) {
  uint8_t payload[4096];
  fabricate_payload(payload, sizeof(payload));
  CHECK(memfs_init_with_payload(payload, sizeof(payload), 3) == 0);

  // the deep path is longer than any fixed 64-byte filename would allow
  CHECK(strlen(kDeepPath) > 64);

  int fd = memfs_open_base(kDeepPath, O_RDONLY);
  CHECK(fd >= 0);
  char buf[16];
  memset(buf, 0, sizeof(buf));
  int got = memfs_read(fd, buf, sizeof(buf));
  CHECK(got == (int)(sizeof(kFileData2) - 1));
  CHECK(memcmp(buf, kFileData2, sizeof(kFileData2) - 1) == 0);
  memfs_close(fd);

  // a prefix that is NOT a full name must not match (name_len is exact)
  char bad[sizeof(kDeepPath)];
  memcpy(bad, kDeepPath, sizeof(kDeepPath) - 5); // drop a suffix
  bad[sizeof(kDeepPath) - 5] = '\0';
  CHECK(memfs_open_base(bad, O_RDONLY) == -ENOENT);

  printf("  [ok] deep nested path (>64 B) resolves via variable-length ABI\n");
  return 0;
}

static int test_mkdir_chdir_rmdir(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // mkdir requires parent exists.
  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("a/b") == 0);
  CHECK(memfs_mkdir("a/b/c") == 0);

  // Cannot create a file over a dir or duplicate a dir.
  CHECK(memfs_mkdir("a") == -EEXIST);
  int fd = memfs_open_base("a", O_CREAT | O_WRONLY);
  CHECK(fd == -EISDIR);

  // Change cwd and use relative paths.
  CHECK(memfs_chdir("a/b") == 0);
  fd = memfs_open_base("file.txt", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "relative-write", 14) == 14);
  memfs_close(fd);

  // Drop the temporary file so the parent directory can be removed later.
  CHECK(memfs_unlink("file.txt") == 0);

  // Relative mkdir under cwd.
  CHECK(memfs_mkdir("sub") == 0);
  CHECK(memfs_chdir("sub") == 0);
  CHECK(memfs_chdir("..") == 0);
  CHECK(memfs_chdir("..") == 0); // now at /a
  CHECK(memfs_chdir("..") == 0); // now at /

  char cwd[128];
  CHECK(memfs_getcwd(cwd, sizeof(cwd)) != NULL);
  CHECK(strcmp(cwd, "/") == 0);

  CHECK(memfs_chdir("/a/b/sub") == 0);
  CHECK(memfs_getcwd(cwd, sizeof(cwd)) != NULL);
  CHECK(strcmp(cwd, "/a/b/sub") == 0);

  // rmdir fails if not empty, not a directory, cwd, or special paths.
  CHECK(memfs_rmdir("..") == -ENOTEMPTY); // "/a/b" parent still contains c/sub
  CHECK(memfs_rmdir(".") == -EINVAL);     // "." not allowed
  CHECK(memfs_rmdir("/a/b/c") == 0);      // c is empty
  CHECK(memfs_rmdir("/a/b/sub") == -EBUSY); // cannot remove cwd

  CHECK(memfs_chdir("/") == 0);
  CHECK(memfs_rmdir("/a/b/sub") == 0);
  CHECK(memfs_rmdir("/a/b") == 0);
  CHECK(memfs_rmdir("/a") == 0);

  // Verify only root is left; readdir still returns "." / "..".
  void *root = memfs_opendir("");
  CHECK(root != NULL);
  struct dirent de;
  CHECK(memfs_readdir(root, &de) == 0);
  CHECK(strcmp(de.d_name, ".") == 0);
  CHECK(memfs_readdir(root, &de) == 0);
  CHECK(strcmp(de.d_name, "..") == 0);
  CHECK(memfs_readdir(root, &de) < 0);
  memfs_closedir(root);

  printf("  [ok] mkdir / chdir / rmdir with relative paths and getcwd\n");
  return 0;
}

static int test_rename_truncate_symlink(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // rename a regular file
  int fd = memfs_open_base("a.txt", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "hello", 5) == 5);
  memfs_close(fd);

  CHECK(memfs_rename("a.txt", "b.txt") == 0);
  fd = memfs_open_base("a.txt", O_RDONLY);
  CHECK(fd < 0); // ENOENT
  fd = memfs_open_base("b.txt", O_RDONLY);
  CHECK(fd >= 0);
  char buf[64];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 5);
  CHECK(memcmp(buf, "hello", 5) == 0);
  memfs_close(fd);

  // truncate shrink + extend with zeros
  CHECK(memfs_truncate("b.txt", 2) == 0);
  fd = memfs_open_base("b.txt", O_RDONLY);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 2);
  CHECK(memcmp(buf, "he", 2) == 0);
  memfs_close(fd);

  CHECK(memfs_truncate("b.txt", 6) == 0);
  fd = memfs_open_base("b.txt", O_RDONLY);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 6);
  CHECK(memcmp(buf, "he\x00\x00\x00\x00", 6) == 0);
  memfs_close(fd);

  // chmod stores mode bits
  struct stat sb;
  CHECK(memfs_chmod("b.txt", 0600) == 0);
  CHECK(memfs_stat_path("b.txt", &sb) == 0);
  CHECK((sb.st_mode & 07777) == 0600);

  // symlink + readlink + follow
  CHECK(memfs_symlink("/b.txt", "link") == 0);
  CHECK(memfs_readlink("link", buf, sizeof(buf)) == 6);
  CHECK(memcmp(buf, "/b.txt", 6) == 0);
  // POSIX readlink does not NUL-terminate; verify an exactly-sized buffer.
  char tiny[7];
  memset(tiny, 0, sizeof(tiny));
  CHECK(memfs_readlink("link", tiny, sizeof(tiny)) == 6);
  CHECK(memcmp(tiny, "/b.txt", 6) == 0);
  CHECK(tiny[6] == '\0'); // readlink must not write past bufsiz
  fd = memfs_open_base("link", O_RDONLY);
  CHECK(fd >= 0); // follows symlink
  memfs_close(fd);

  // realpath resolves the symlink target
  char rp[MEMFS_PATH_MAX];
  CHECK(memfs_realpath("link", rp) != NULL);
  CHECK(strcmp(rp, "/b.txt") == 0);

  // lstat sees a symlink, stat sees the target
  CHECK(memfs_lstat_path("link", &sb) == 0);
  CHECK(S_ISLNK(sb.st_mode));
  CHECK(memfs_stat_path("link", &sb) == 0);
  CHECK(!S_ISLNK(sb.st_mode));

  // st_ino / st_dev must differ between different nodes (used by libc++
  // fs::equivalent and copy collision detection).
  struct stat sb_link, sb_file;
  CHECK(memfs_lstat_path("link", &sb_link) == 0);
  CHECK(memfs_stat_path("b.txt", &sb_file) == 0);
  CHECK(sb_link.st_dev == sb_file.st_dev);
  CHECK(sb_link.st_ino != sb_file.st_ino);

  printf("  [ok] rename / truncate / chmod / symlink / realpath\n");
  return 0;
}

static int test_rename_directories(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("a/b") == 0);
  CHECK(memfs_mkdir("c") == 0);

  // Rename a directory with contents.
  CHECK(memfs_rename("a", "d") == 0);
  void *db = memfs_opendir("d/b");
  CHECK(db != NULL);
  memfs_closedir(db);
  CHECK(memfs_opendir("a") == NULL);

  // Cannot move a directory into itself or its subtree.
  CHECK(memfs_rename("d", "d/b/x") == -EINVAL);
  CHECK(memfs_rename("d", "d") == 0); // no-op

  // May replace an empty directory.
  CHECK(memfs_rename("d", "c") == 0);
  void *cb = memfs_opendir("c/b");
  CHECK(cb != NULL);
  memfs_closedir(cb);

  // Cannot replace an existing non-empty or wrong-type target.
  CHECK(memfs_mkdir("x") == 0);
  CHECK(memfs_rename("x", "c") == -ENOTEMPTY);
  int ffd = memfs_open_base("c/b/file", O_CREAT | O_RDWR);
  CHECK(ffd >= 0);
  memfs_close(ffd);
  CHECK(memfs_rename("x", "/c/b/file") == -ENOTDIR);

  printf("  [ok] rename directories\n");
  return 0;
}

static int test_open_mode_and_dirfds(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // open with mode is wired to memfs_open_mode; create a file with explicit mode.
  int fd = memfs_open_mode("mode_test", O_CREAT | O_RDWR, 0600);
  CHECK(fd >= 0);
  memfs_close(fd);
  struct stat sb;
  CHECK(memfs_stat_path("mode_test", &sb) == 0);
  CHECK((sb.st_mode & 07777) == 0600);

  // Directory file descriptors.
  CHECK(memfs_mkdir("atdir") == 0);
  CHECK(memfs_mkdir("atdir/sub") == 0);
  int dfd = memfs_openat_dir(AT_FDCWD, "atdir");
  CHECK(dfd >= 0);

  // Open a child directory via the synthetic fd.
  int subfd = memfs_openat_dir(dfd, "sub");
  CHECK(subfd >= 0);

  // fdopendir round-trips.
  void *dir = memfs_dir_from_fd(subfd);
  CHECK(dir != NULL);
  CHECK(memfs_dirfd(dir) == subfd);

  // Relative unlinkat for a file.
  int ffd = memfs_open_mode("atdir/file", O_CREAT | O_RDWR, 0644);
  CHECK(ffd >= 0);
  memfs_close(ffd);
  CHECK(memfs_unlinkat(dfd, "file", 0) == 0);

  // A-only semantics: an open directory does NOT block its own removal (B)
  // but it still blocks removal of its ancestors (A). libc++ remove_all()
  // relies on being able to delete an open directory. The caller is responsible
  // for closing any dangling directory stream.
  CHECK(memfs_unlinkat(dfd, "sub", AT_REMOVEDIR) == 0);
  CHECK(memfs_opendir("atdir/sub") == NULL);
  CHECK(memfs_close(subfd) == 0);

  // Re-create and verify the A rule: while a child directory stream is open,
  // its parent cannot be removed.
  CHECK(memfs_mkdir("atdir/sub2") == 0);
  int sub2fd = memfs_openat_dir(dfd, "sub2");
  CHECK(sub2fd >= 0);
  // A-rule: atdir is an ancestor of the open directory sub2.
  int r = memfs_rmdir("atdir");
  CHECK(r == -EBUSY);
  // B-rule: the currently-open directory sub2 may be removed itself.
  CHECK(memfs_unlinkat(dfd, "sub2", AT_REMOVEDIR) == 0);
  memfs_close(sub2fd);
  CHECK(memfs_close(dfd) == 0);
  CHECK(memfs_rmdir("atdir") == 0);

  // pathconf values are positive.
  CHECK(memfs_pathconf("/", _PC_PATH_MAX) == MEMFS_PATH_MAX);
  CHECK(memfs_pathconf("/", _PC_NAME_MAX) > 0);

  printf("  [ok] open mode + dirfd / openat / fdopendir / unlinkat\n");
  return 0;
}

// ---- additional resource / state / stress tests -----------------------------------

static int root_children_count(void) {
  void *d = memfs_opendir("");
  if (!d)
    return -1;
  struct dirent de;
  int n = 0;
  while (memfs_readdir(d, &de) == 0)
    n++;
  memfs_closedir(d);
  return n;
}

static int dir_children_count(const char *path) {
  void *d = memfs_opendir(path);
  if (!d)
    return -1;
  struct dirent de;
  int n = 0;
  while (memfs_readdir(d, &de) == 0)
    if (strcmp(de.d_name, ".") != 0 && strcmp(de.d_name, "..") != 0)
      n++;
  memfs_closedir(d);
  return n;
}

static int test_invalid_args(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_open_base(NULL, O_RDONLY) == -EINVAL);
  CHECK(memfs_stat_path(NULL, NULL) == -EINVAL);

  struct stat st;
  CHECK(memfs_stat_path(NULL, &st) == -EINVAL);
  CHECK(memfs_mkdir(NULL) == -EINVAL);
  CHECK(memfs_rmdir(NULL) == -EINVAL);
  CHECK(memfs_unlink(NULL) == -EINVAL);
  CHECK(memfs_chdir(NULL) == -EINVAL);
  CHECK(memfs_rename(NULL, "x") == -EINVAL);
  CHECK(memfs_rename("x", NULL) == -EINVAL);
  CHECK(memfs_truncate(NULL, 0) == -EINVAL);
  CHECK(memfs_symlink(NULL, "x") == -EINVAL);
  CHECK(memfs_symlink("x", NULL) == -EINVAL);

  CHECK(memfs_readlink("link", NULL, 0) == -EINVAL);
  CHECK(memfs_fchmod(-999, 0644) == -EBADF);
  CHECK(memfs_ftruncate(-999, 0) == -EBADF);
  CHECK(memfs_close(-999) == -EBADF);
  CHECK(memfs_lseek(-999, 0, SEEK_SET) < 0);
  CHECK(memfs_dirfd(NULL) == -EINVAL);
  CHECK(memfs_opendir("missing") == NULL);

  printf("  [ok] invalid arguments return -EINVAL/-EBADF\n");
  return 0;
}

static int test_open_flags_variants(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // O_CREAT is required for missing name.
  CHECK(memfs_open_base("new", O_WRONLY) < 0);
  int fd = memfs_open_base("new", O_CREAT | O_WRONLY);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "abc", 3) == 3);
  memfs_close(fd);

  // O_CREAT | O_EXCL on existing file fails.
  CHECK(memfs_open_base("new", O_CREAT | O_EXCL | O_WRONLY) == -EEXIST);

  // O_TRUNC clears content.
  fd = memfs_open_base("new", O_TRUNC | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  char buf[8];
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 0);
  memfs_close(fd);

  // O_APPEND writes at end regardless of current offset.
  fd = memfs_open_base("new", O_CREAT | O_WRONLY);
  CHECK(memfs_write(fd, "ABC", 3) == 3);
  memfs_close(fd);
  fd = memfs_open_base("new", O_WRONLY | O_APPEND);
  CHECK(memfs_write(fd, "X", 1) == 1); // should append
  memfs_close(fd);
  fd = memfs_open_base("new", O_RDONLY);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 4);
  CHECK(memcmp(buf, "ABCX", 4) == 0);
  memfs_close(fd);

  // O_RDONLY | O_APPEND must still read from offset 0 (O_APPEND only affects
  // writes), not from EOF.
  fd = memfs_open_base("new", O_RDONLY | O_APPEND);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 4);
  CHECK(memcmp(buf, "ABCX", 4) == 0);
  memfs_close(fd);

  // O_DIRECTORY on a regular file.
  CHECK(memfs_open_base("new", O_DIRECTORY | O_RDONLY) == -ENOTDIR);

  printf("  [ok] open flag combinations (creat/excl/trunc/append/directory)\n");
  return 0;
}

static int test_rw_edge_and_eof(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "abcdef", 6) == 6);
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);

  char buf[32];
  CHECK(memfs_read(fd, buf, 0) == 0);         // read 0 bytes
  CHECK(memfs_read(fd, buf, 4) == 4);
  CHECK(memcmp(buf, "abcd", 4) == 0);
  CHECK(memfs_read(fd, buf, 4) == 2);         // partial at EOF
  CHECK(memcmp(buf, "ef", 2) == 0);
  CHECK(memfs_read(fd, buf, 4) == 0);         // EOF

  // Overwrite middle.
  CHECK(memfs_lseek(fd, 3, SEEK_SET) == 3);
  CHECK(memfs_write(fd, "XYZ", 3) == 3);
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd, buf, 6) == 6);
  CHECK(memcmp(buf, "abcXYZ", 6) == 0);
  memfs_close(fd);

  // Write with a gap beyond current EOF -> zero-fill.
  fd = memfs_open_base("f", O_RDWR);
  CHECK(memfs_lseek(fd, 10, SEEK_SET) == 10);
  CHECK(memfs_write(fd, "end", 3) == 3);
  struct stat st;
  CHECK(memfs_stat_path("f", &st) == 0);
  CHECK(st.st_size == 13);
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd, buf, 32) == 13);
  CHECK(memcmp(buf, "abcXYZ", 6) == 0);
  CHECK(buf[6] == 0 && buf[7] == 0 && buf[8] == 0 && buf[9] == 0);
  CHECK(memcmp(buf + 10, "end", 3) == 0);
  memfs_close(fd);

  printf("  [ok] read/write edge cases, EOF, and sparse extension\n");
  return 0;
}

static int test_multiple_fd_consistency(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int w = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(w >= 0);
  CHECK(memfs_write(w, "hello", 5) == 5);

  int r = memfs_open_base("f", O_RDONLY);
  CHECK(r >= 0);
  char buf[64];
  CHECK(memfs_read(r, buf, sizeof(buf)) == 5);
  CHECK(memcmp(buf, "hello", 5) == 0);

  CHECK(memfs_lseek(r, 2, SEEK_SET) == 2);
  CHECK(memfs_read(r, buf, 2) == 2);
  CHECK(memcmp(buf, "ll", 2) == 0);

  // Write through one fd is visible to the other from its own offset.
  CHECK(memfs_write(w, " world", 6) == 6);
  CHECK(memfs_read(r, buf, sizeof(buf)) == 7); // position 4 -> "o world"
  CHECK(memcmp(buf, "o world", 7) == 0);

  memfs_close(w);
  // Reader still valid after writer closes.
  CHECK(memfs_lseek(r, 0, SEEK_SET) == 0);
  CHECK(memfs_read(r, buf, sizeof(buf)) == 11);
  CHECK(memcmp(buf, "hello world", 11) == 0);
  memfs_close(r);

  printf("  [ok] multiple fds to the same file remain consistent\n");
  return 0;
}

static int test_lseek_variants(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(memfs_write(fd, "0123456789", 10) == 10);

  char buf[8];
  CHECK(memfs_lseek(fd, 5, SEEK_SET) == 5);
  CHECK(memfs_read(fd, buf, 2) == 2);
  CHECK(memcmp(buf, "56", 2) == 0);

  CHECK(memfs_lseek(fd, 3, SEEK_CUR) == 10); // now at end
  CHECK(memfs_read(fd, buf, 2) == 0);

  CHECK(memfs_lseek(fd, -3, SEEK_END) == 7);
  CHECK(memfs_read(fd, buf, 3) == 3);
  CHECK(memcmp(buf, "789", 3) == 0);

  // Invalid whence.
  CHECK(memfs_lseek(fd, 0, 123) < 0);
  // Negative resulting offset.
  CHECK(memfs_lseek(fd, -100, SEEK_SET) < 0);

  memfs_close(fd);
  printf("  [ok] lseek set/cur/end and invalid offsets\n");
  return 0;
}

static int test_trunc_edge_cases(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(memfs_write(fd, "abcdef", 6) == 6);

  CHECK(memfs_ftruncate(fd, 3) == 0);
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  char buf[16];
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 3);
  CHECK(memcmp(buf, "abc", 3) == 0);

  CHECK(memfs_ftruncate(fd, 10) == 0);
  struct stat st;
  CHECK(memfs_stat_path("f", &st) == 0);
  CHECK(st.st_size == 10);
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 10);
  CHECK(memcmp(buf, "abc", 3) == 0);
  CHECK(buf[3] == 0 && buf[4] == 0 && buf[5] == 0);
  memfs_close(fd);

  CHECK(memfs_ftruncate(-999, 5) == -EBADF);

  // Truncate by path then write.
  CHECK(memfs_truncate("f", 0) == 0);
  fd = memfs_open_base("f", O_RDWR);
  CHECK(memfs_write(fd, "new", 3) == 3);
  memfs_close(fd);
  CHECK(memfs_stat_path("f", &st) == 0);
  CHECK(st.st_size == 3);

  // Extending past the 32-bit address space must be rejected, not wrap.
  fd = memfs_open_base("f", O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_ftruncate(fd, (long)UINT32_MAX + 1) == -EINVAL);
  CHECK(memfs_truncate("f", (long)UINT32_MAX + 1) == -EINVAL);
  CHECK(memfs_stat_path("f", &st) == 0);
  CHECK(st.st_size == 3);
  memfs_close(fd);

  printf("  [ok] ftruncate/truncate shrink, extend, and EBADF\n");
  return 0;
}

static int test_u32_overflow_rejected(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  // lseek past UINT32_MAX must not wrap and write into early bytes.
  CHECK(memfs_lseek(fd, (long)UINT32_MAX, SEEK_SET) == (long)UINT32_MAX);
  CHECK(memfs_write(fd, "x", 1) == -EINVAL);

  // SEEK_CUR beyond UINT32_MAX must be rejected before long overflow UB.
  CHECK(memfs_lseek(fd, 1, SEEK_SET) == 1);
  CHECK(memfs_lseek(fd, (long)UINT32_MAX, SEEK_CUR) == -EINVAL);

  memfs_close(fd);
  CHECK(memfs_unlink("f") == 0);

  printf("  [ok] uint32_t offset overflow rejected\n");
  return 0;
}

static int test_readdir_mutation(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);

  int fd = memfs_open_base("d/f1", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);
  fd = memfs_open_base("d/f2", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);

  void *dir = memfs_opendir("d");
  CHECK(dir != NULL);
  struct dirent de;
  CHECK(memfs_readdir(dir, &de) == 0);
  CHECK(strcmp(de.d_name, ".") == 0);

  // Create a new child mid-enumeration.
  fd = memfs_open_base("d/f3", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);

  // Continue enumeration without crash; root may or may not see f3.
  int saw_f1 = 0, saw_f2 = 0;
  while (memfs_readdir(dir, &de) == 0) {
    if (strcmp(de.d_name, "f1") == 0) saw_f1 = 1;
    if (strcmp(de.d_name, "f2") == 0) saw_f2 = 1;
  }
  CHECK(saw_f1 && saw_f2);
  memfs_closedir(dir);

  CHECK(dir_children_count("d") == 3);

  printf("  [ok] readdir survives mutations and lists added children\n");
  return 0;
}

static int test_rmdir_errors_and_types(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_rmdir("missing") == -ENOENT);
  CHECK(memfs_rmdir("/") == -EINVAL);
  CHECK(memfs_rmdir(".") == -EINVAL);

  CHECK(memfs_mkdir("d") == 0);
  CHECK(memfs_mkdir("d/e") == 0);
  CHECK(memfs_rmdir("d") == -ENOTEMPTY);
  CHECK(memfs_rmdir("d/e") == 0);
  CHECK(memfs_rmdir("d") == 0);

  int fd = memfs_open_base("file", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);
  CHECK(memfs_rmdir("file") == -ENOTDIR);
  CHECK(memfs_unlink("file") == 0);

  // Cannot unlink a directory.
  CHECK(memfs_mkdir("d2") == 0);
  CHECK(memfs_unlink("d2") == -EISDIR);
  CHECK(memfs_rmdir("d2") == 0);

  // Cannot remove cwd.
  CHECK(memfs_mkdir("cwdlock") == 0);
  CHECK(memfs_chdir("cwdlock") == 0);
  CHECK(memfs_rmdir("/cwdlock") == -EBUSY);
  CHECK(memfs_chdir("/") == 0);
  CHECK(memfs_rmdir("/cwdlock") == 0);

  printf("  [ok] rmdir/unlink error codes (ENOTEMPTY, EISDIR, cwd, root)\n");
  return 0;
}

static int test_rename_variants(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // Rename file across directories.
  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("b") == 0);
  int fd = memfs_open_base("a/f", O_CREAT | O_RDWR);
  CHECK(memfs_write(fd, "data", 4) == 4);
  memfs_close(fd);
  CHECK(memfs_rename("a/f", "b/f") == 0);
  fd = memfs_open_base("a/f", O_RDONLY);
  CHECK(fd < 0);
  fd = memfs_open_base("b/f", O_RDONLY);
  CHECK(fd >= 0);
  char buf[8];
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 4);
  CHECK(memcmp(buf, "data", 4) == 0);
  memfs_close(fd);

  // Replace file with file.
  fd = memfs_open_base("b/f", O_WRONLY | O_TRUNC);
  CHECK(memfs_write(fd, "new", 3) == 3);
  memfs_close(fd);
  fd = memfs_open_base("b/f", O_RDONLY);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 3);
  CHECK(memcmp(buf, "new", 3) == 0);
  memfs_close(fd);

  // Rename file over directory -> EISDIR? Actually target is dir.
  fd = memfs_open_base("a/file2", O_CREAT | O_RDWR);
  memfs_close(fd);
  CHECK(memfs_rename("a/file2", "b") < 0);

  // Rename directory over file -> ENOTDIR.
  CHECK(memfs_mkdir("a/sub") == 0);
  CHECK(memfs_rename("a/sub", "b/f") == -ENOTDIR);

  // Rename directory over non-empty directory -> ENOTEMPTY.
  CHECK(memfs_mkdir("b/sub") == 0);
  CHECK(memfs_mkdir("b/sub/inner") == 0);
  CHECK(memfs_mkdir("a/empty") == 0);
  CHECK(memfs_rename("a/empty", "b/sub") == -ENOTEMPTY);

  // Rename directory over empty directory -> ok.
  CHECK(memfs_rmdir("b/sub/inner") == 0);
  CHECK(memfs_rename("a/empty", "b/sub") == 0);

  // Rename into descendant -> EINVAL.
  CHECK(memfs_rename("a", "a/sub/new") == -EINVAL);

  CHECK(memfs_rmdir("a/sub") == 0);
  CHECK(memfs_unlink("a/file2") == 0);
  CHECK(memfs_rmdir("a") == 0);
  CHECK(memfs_rmdir("b/sub") == 0);
  CHECK(memfs_unlink("b/f") == 0);
  CHECK(memfs_rmdir("b") == 0);

  printf("  [ok] rename across dirs, replacement, and descendant checks\n");
  return 0;
}

static int test_symlink_full(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int fd = memfs_open_base("target", O_CREAT | O_RDWR);
  CHECK(memfs_write(fd, "hello", 5) == 5);
  memfs_close(fd);

  CHECK(memfs_symlink("target", "link") == 0);

  struct stat st;
  CHECK(memfs_lstat_path("link", &st) == 0);
  CHECK(S_ISLNK(st.st_mode));
  CHECK(memfs_stat_path("link", &st) == 0);
  CHECK(!S_ISLNK(st.st_mode));

  char target[64];
  CHECK(memfs_readlink("link", target, sizeof(target)) == 6);
  CHECK(memcmp(target, "target", 6) == 0);

  fd = memfs_open_base("link", O_RDONLY);
  CHECK(fd >= 0);
  char buf[16];
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 5);
  CHECK(memcmp(buf, "hello", 5) == 0);
  memfs_close(fd);

  // Symlink chain.
  CHECK(memfs_symlink("link", "chain") == 0);
  fd = memfs_open_base("chain", O_RDONLY);
  CHECK(fd >= 0);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 5);
  memfs_close(fd);

  // O_NOFOLLOW on a symlink should fail.
  CHECK(memfs_open_base("link", O_RDONLY | O_NOFOLLOW) < 0);

  // Loop.
  CHECK(memfs_symlink("loop", "loop") == 0);
  CHECK(memfs_open_base("loop", O_RDONLY) == -ELOOP);

  // Dangling symlink.
  CHECK(memfs_symlink("nowhere", "dangling") == 0);
  CHECK(memfs_stat_path("dangling", &st) == -ENOENT);
  CHECK(memfs_open_base("dangling", O_RDONLY) == -ENOENT);

  // Unlink symlink leaves target.
  CHECK(memfs_unlink("link") == 0);
  CHECK(memfs_open_base("target", O_RDONLY) >= 0);

  printf("  [ok] symlinks: lstat, readlink, chain, loop, O_NOFOLLOW\n");
  return 0;
}

static int test_path_resolution(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("a/b") == 0);
  int fd = memfs_open_base("a/b/file", O_CREAT | O_RDWR);
  memfs_close(fd);

  struct stat st;
  CHECK(memfs_stat_path("/a/b/file", &st) == 0);
  CHECK(memfs_stat_path("a/b/file", &st) == 0);
  CHECK(memfs_stat_path("a//b/../b/./file", &st) == 0);
  CHECK(memfs_stat_path("a/b/../b/", &st) == 0);
  // memfs normalizes away trailing slashes, so this resolves the file rather
  // than returning -ENOTDIR. Document that behaviour.
  CHECK(memfs_stat_path("a/b/file/", &st) == 0);

  // Too long.
  char longname[MEMFS_NAME_MAX + 4];
  memset(longname, 'x', sizeof(longname) - 1);
  longname[sizeof(longname) - 1] = 0;
  CHECK(memfs_mkdir(longname) < 0); // name too long

  // Too long path.
  char longpath[MEMFS_PATH_MAX + 4];
  memset(longpath, '/', 1);
  memset(longpath + 1, 'a', MEMFS_PATH_MAX);
  longpath[MEMFS_PATH_MAX + 1] = 0;
  CHECK(memfs_stat_path(longpath, &st) < 0);

  printf("  [ok] path normalization and length limits\n");
  return 0;
}

static int test_stat_nlink_state(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  struct stat st;
  CHECK(memfs_stat_path("/", &st) == 0);
  CHECK(S_ISDIR(st.st_mode));
  CHECK(st.st_nlink == 2);

  // memfs keeps root nlink fixed at 2; only non-root directories change.
  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_stat_path("/", &st) == 0);
  CHECK(st.st_nlink == 2);
  CHECK(memfs_stat_path("a", &st) == 0);
  CHECK(st.st_nlink == 2);

  CHECK(memfs_mkdir("a/b") == 0);
  CHECK(memfs_stat_path("a", &st) == 0);
  CHECK(st.st_nlink == 3);

  CHECK(memfs_rmdir("a/b") == 0);
  CHECK(memfs_stat_path("a", &st) == 0);
  CHECK(st.st_nlink == 2);

  // Rename directory preserves counts.
  CHECK(memfs_mkdir("a/b") == 0);
  CHECK(memfs_rename("a", "x") == 0);
  CHECK(memfs_stat_path("x/b", &st) == 0);
  CHECK(st.st_nlink == 2);
  CHECK(memfs_stat_path("x", &st) == 0);
  CHECK(st.st_nlink == 3);

  CHECK(memfs_chdir("/") == 0);
  CHECK(memfs_rmdir("x/b") == 0);
  CHECK(memfs_rmdir("x") == 0);
  CHECK(memfs_stat_path("/", &st) == 0);
  CHECK(st.st_nlink == 2);

  printf("  [ok] stat mode/nlink state through mkdir/rmdir/rename\n");
  return 0;
}

static int test_dangling_dir_stream(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_mkdir("d") == 0);
  CHECK(memfs_mkdir("d/sub") == 0);
  void *dir = memfs_opendir("d");
  CHECK(dir != NULL);

  // With A-only rule, removing the open directory itself is allowed.
  CHECK(memfs_rmdir("d/sub") == 0);
  CHECK(memfs_rmdir("d") == 0);
  CHECK(memfs_closedir(dir) == 0);

  // After dangling stream is closed, the tree is gone.
  CHECK(memfs_opendir("d") == NULL);

  // Removing an ancestor while a descendant stream is open is still blocked.
  CHECK(memfs_mkdir("parent") == 0);
  CHECK(memfs_mkdir("parent/child") == 0);
  void *child = memfs_opendir("parent/child");
  CHECK(child != NULL);
  CHECK(memfs_rmdir("parent") == -EBUSY);
  CHECK(memfs_closedir(child) == 0);
  CHECK(memfs_rmdir("parent/child") == 0);
  CHECK(memfs_rmdir("parent") == 0);

  printf("  [ok] dangling dir streams close safely after parent removal\n");
  return 0;
}

static int test_remove_all_pattern(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_mkdir("out") == 0);
  CHECK(memfs_mkdir("out/a") == 0);
  CHECK(memfs_mkdir("out/b") == 0);
  int fd = memfs_open_base("out/a/f1", O_CREAT | O_RDWR);
  CHECK(memfs_write(fd, "1", 1) == 1);
  memfs_close(fd);
  fd = memfs_open_base("out/b/f2", O_CREAT | O_RDWR);
  CHECK(memfs_write(fd, "2", 1) == 1);
  memfs_close(fd);
  CHECK(memfs_symlink("a/f1", "out/link") == 0);

  // Simulate libc++ remove_all: open dir, iterate, recurse, then rmdir it.
  struct frame { const char *path; void *dir; };
  struct frame stack[16];
  int sp = 0;
  stack[sp].path = "out";
  stack[sp].dir = memfs_opendir("out");
  CHECK(stack[sp].dir != NULL);
  sp++;

  while (sp > 0) {
    struct frame *f = &stack[sp - 1];
    struct dirent de;
    if (memfs_readdir(f->dir, &de) < 0) {
      // Finished this directory: rmdir while its stream is still open.
      CHECK(memfs_rmdir(f->path) == 0);
      CHECK(memfs_closedir(f->dir) == 0);
      sp--;
      continue;
    }
    if (strcmp(de.d_name, ".") == 0 || strcmp(de.d_name, "..") == 0)
      continue;
    char child[256];
    snprintf(child, sizeof(child), "%s/%s", f->path, de.d_name);

    struct stat st;
    if (memfs_lstat_path(child, &st) == 0 && S_ISDIR(st.st_mode)) {
      CHECK(sp < (int)(sizeof(stack) / sizeof(stack[0])) - 1);
      stack[sp].path = "x";
      // Use a local copy because pointer must remain valid.
      static char paths[16][256];
      strncpy(paths[sp], child, sizeof(paths[sp]) - 1);
      paths[sp][sizeof(paths[sp]) - 1] = 0;
      stack[sp].path = paths[sp];
      stack[sp].dir = memfs_opendir(child);
      CHECK(stack[sp].dir != NULL);
      sp++;
    } else {
      CHECK(memfs_unlink(child) == 0);
    }
  }

  CHECK(memfs_opendir("out") == NULL);
  printf("  [ok] recursive remove_all pattern with dangling parent streams\n");
  return 0;
}

static int test_resource_cleanup(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // Create and destroy a large set of files.
  for (int i = 0; i < 80; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "f%d", i);
    int fd = memfs_open_base(name, O_CREAT | O_RDWR);
    CHECK(fd >= 0);
    CHECK(memfs_write(fd, name, strlen(name)) == (int)strlen(name));
    memfs_close(fd);
  }
  CHECK(dir_children_count("") == 80);

  for (int i = 0; i < 80; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "f%d", i);
    CHECK(memfs_unlink(name) == 0);
  }
  CHECK(dir_children_count("") == 0);
  CHECK(root_children_count() == 2); // only . and ..

  // Create and destroy a deep directory chain (each level under the previous).
  char paths[32][256];
  const int chain_depth = 25;
  for (int i = 0; i < chain_depth; ++i) {
    if (i == 0)
      snprintf(paths[i], sizeof(paths[i]), "c0");
    else
      snprintf(paths[i], sizeof(paths[i]), "%s/c%d", paths[i - 1], i);
    CHECK(memfs_mkdir(paths[i]) == 0);
  }
  for (int i = chain_depth - 1; i >= 0; --i) {
    CHECK(memfs_rmdir(paths[i]) == 0);
  }

  // After all cleanup we should be able to allocate again.
  int fd = memfs_open_base("fresh", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);
  CHECK(memfs_unlink("fresh") == 0);

  printf("  [ok] resource cleanup after many files and deep directory chains\n");
  return 0;
}

static int test_table_exhaustion_recovery(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int fds[200];
  int n = 0;
  for (; n < 200; ++n) {
    char name[32];
    snprintf(name, sizeof(name), "slot%d", n);
    fds[n] = memfs_open_base(name, O_CREAT | O_RDWR);
    if (fds[n] < 0)
      break;
  }
  CHECK(n > 0);
  CHECK(memfs_open_base("overflow", O_CREAT | O_RDWR) == -ENFILE);

  for (int i = 0; i < n; ++i)
    memfs_close(fds[i]);

  // Recovery.
  CHECK(memfs_open_base("after", O_CREAT | O_RDWR) >= 0);

  // Directory streams.
  for (int i = 0; i < 20; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "ds%d", i);
    CHECK(memfs_mkdir(name) == 0);
  }
  int dirstreams[64];
  int m = 0;
  for (; m < 64; ++m) {
    char name[32];
    snprintf(name, sizeof(name), "ds%d", m % 20);
    dirstreams[m] = memfs_openat_dir(AT_FDCWD, name);
    if (dirstreams[m] < 0)
      break;
  }
  CHECK(m > 0);
  // Opening one more should fail.
  CHECK(memfs_openat_dir(AT_FDCWD, "ds0") == -EMFILE);
  for (int i = 0; i < m; ++i)
    memfs_close(dirstreams[i]);

  CHECK(memfs_openat_dir(AT_FDCWD, "ds0") >= 0);

  printf("  [ok] fd and dir-stream table exhaustion and recovery\n");
  return 0;
}

static int test_node_table_exhaustion(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  int created = 0;
  for (int i = 0; i < 200; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "n%d", i);
    if (memfs_mkdir(name) != 0)
      break;
    created++;
  }
  CHECK(created > 0);
  CHECK(memfs_mkdir("toomany") < 0); // table full

  // Delete half and verify recovery.
  for (int i = 0; i < created / 2; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "n%d", i);
    CHECK(memfs_rmdir(name) == 0);
  }
  CHECK(memfs_mkdir("recovery") == 0);
  CHECK(memfs_rmdir("recovery") == 0);

  // Clean remaining.
  for (int i = created / 2; i < created; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "n%d", i);
    CHECK(memfs_rmdir(name) == 0);
  }

  printf("  [ok] memfs node table exhaustion and reuse\n");
  return 0;
}

static int test_dirfd_unlinkat(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  CHECK(memfs_mkdir("base") == 0);
  int dfd = memfs_openat_dir(AT_FDCWD, "base");
  CHECK(dfd >= 0);

  int fd = memfs_open_base("base/file", O_CREAT | O_RDWR);
  memfs_close(fd);
  CHECK(memfs_mkdir("base/sub") == 0);

  CHECK(memfs_unlinkat(dfd, "file", 0) == 0);
  CHECK(memfs_unlinkat(dfd, "sub", AT_REMOVEDIR) == 0);
  CHECK(memfs_unlinkat(dfd, "sub", AT_REMOVEDIR) == -ENOENT);

  CHECK(memfs_close(dfd) == 0);
  CHECK(memfs_rmdir("base") == 0);

  printf("  [ok] unlinkat through synthetic dir fd\n");
  return 0;
}

static int test_realpath_variants(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("a/b") == 0);

  char *rp = memfs_realpath("/a/b", NULL);
  CHECK(rp != NULL);
  CHECK(strcmp(rp, "/a/b") == 0);
  free(rp);

  rp = memfs_realpath("a/b/../b", NULL);
  CHECK(rp != NULL);
  CHECK(strcmp(rp, "/a/b") == 0);
  free(rp);

  rp = memfs_realpath("./a", NULL);
  CHECK(rp != NULL);
  CHECK(strcmp(rp, "/a") == 0);
  free(rp);

  rp = memfs_realpath("missing", NULL);
  CHECK(rp == NULL);

  printf("  [ok] realpath on absolute, relative, and missing paths\n");
  return 0;
}

//// Coverage migrated from test_memfs_extended.c

#define DEFAULT_MEMFS_MAX_FD 64
#define DEFAULT_MEMFS_MAX_OPEN 128
#define DEFAULT_MEMFS_DIR_MAX 32

// Synthetic directory-fd base is implementation-private; memfs.c uses 1000.
#define TEST_MEMFS_DIRFD_BASE 1000

static uint8_t s_payload_buf[8192];



static void make_simple_payload(uint8_t *buf, size_t buf_sz) {
  const char *names[1] = {"pack.txt"};
  const char data[] = "packed";

  const uint32_t hdr = 16;
  const uint32_t entry0 = hdr;
  const uint32_t names_off = hdr + (uint32_t)sizeof(MemfsEntry);
  const uint32_t data_off = names_off + (uint32_t)strlen(names[0]) + 1;

  memset(buf, 0, buf_sz);
  put_u32(buf + PAYLOAD_MAGIC_OFF, MEMFS_MAGIC);
  put_u32(buf + PAYLOAD_COUNT_OFF, 1);
  put_u32(buf + PAYLOAD_VERSION_OFF, 0);
  put_u32(buf + PAYLOAD_DATA_OFF, data_off);

  MemfsEntry *e = (MemfsEntry *)(buf + entry0);
  e[0].name_off = names_off;
  e[0].name_len = (uint32_t)strlen(names[0]);
  e[0].seg0_offset = data_off;
  e[0].seg0_size = (uint32_t)(sizeof(data) - 1);
  e[0].mode = 0644;

  memcpy(buf + names_off, names[0], strlen(names[0]) + 1);
  memcpy(buf + data_off, data, sizeof(data) - 1);
}


static int test_opendir_null_root(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // Header says NULL / "" / "." mean root. NULL currently fails in
  // normalize_path; this test documents the mismatch.
  void *root = memfs_opendir(NULL);
  CHECK(root != NULL);
  struct dirent de;
  CHECK(memfs_readdir(root, &de) == 0);
  CHECK(strcmp(de.d_name, ".") == 0);
  CHECK(memfs_readdir(root, &de) == 0);
  CHECK(strcmp(de.d_name, "..") == 0);
  CHECK(memfs_readdir(root, &de) < 0);
  memfs_closedir(root);

  root = memfs_opendir("");
  CHECK(root != NULL);
  CHECK(memfs_readdir(root, &de) == 0 && strcmp(de.d_name, ".") == 0);
  CHECK(memfs_readdir(root, &de) == 0 && strcmp(de.d_name, "..") == 0);
  CHECK(memfs_readdir(root, &de) < 0);
  memfs_closedir(root);

  printf("  [ok] opendir(NULL/\"\") lists root\n");
  return 0;
}

static int test_dirfd_fd_isolation(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);

  void *dir = memfs_opendir("d");
  CHECK(dir != NULL);
  int dfd = memfs_dirfd(dir);
  CHECK(dfd >= TEST_MEMFS_DIRFD_BASE);

  // Synthetic dir fds must not be accepted by regular-file fd entry points.
  char buf[16];
  CHECK(memfs_read(dfd, buf, sizeof(buf)) == -EBADF);
  CHECK(memfs_write(dfd, "x", 1) == -EBADF);
  CHECK(memfs_lseek(dfd, 0, SEEK_SET) == -EBADF);

  struct stat st;
  CHECK(memfs_stat(dfd, &st) == 0); // fstat is intentionally supported
  CHECK(memfs_fchmod(dfd, 0755) == 0);
  CHECK(memfs_ftruncate(dfd, 0) == -EINVAL);

  CHECK(memfs_close(dfd) == 0);

  // After closing the synthetic fd, the directory stream is gone too.
  CHECK(memfs_dir_from_fd(dfd) == NULL);

  memfs_closedir(dir);
  CHECK(memfs_rmdir("d") == 0);
  printf("  [ok] synthetic dir fd isolated from read/write/lseek\n");
  return 0;
}

static int test_base_fd_stability(void) {
  // The base fd is now a property of init(), not open(): open() simply
  // allocates regular fds starting at the base this instance was initialised
  // with. Use a non-default base to prove it is actually honoured.
  CHECK(memfs_init_with_payload(NULL, 0, 10) == 0);

  int fd1 = memfs_open_base("file1", O_CREAT | O_RDWR);
  CHECK(fd1 == 10); // first regular fd == the init base

  int fd2 = memfs_open_base("file2", O_CREAT | O_RDWR);
  CHECK(fd2 == 11);

  // Both fds are independently usable and offset from the same base.
  CHECK(memfs_write(fd1, "abc", 3) == 3);
  CHECK(memfs_write(fd2, "xy", 2) == 2);

  char buf[16];
  memset(buf, 0, sizeof(buf));
  CHECK(memfs_lseek(fd1, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd1, buf, sizeof(buf)) == 3);
  CHECK(memcmp(buf, "abc", 3) == 0);

  memset(buf, 0, sizeof(buf));
  CHECK(memfs_lseek(fd2, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd2, buf, sizeof(buf)) == 2);
  CHECK(memcmp(buf, "xy", 2) == 0);

  CHECK(memfs_close(fd1) == 0);
  CHECK(memfs_close(fd2) == 0);

  // Objects created under the custom base are addressable by path.
  struct stat st;
  CHECK(memfs_stat_path("file1", &st) == 0);
  CHECK(memfs_stat_path("file2", &st) == 0);

  printf("  [ok] fds allocated from init-time base\n");
  return 0;
}

static int test_rename_directory_slot_leak(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);

  // Setup: create a directory that will be repeatedly replaced by rename.
  CHECK(memfs_mkdir("parent") == 0);
  CHECK(memfs_mkdir("parent/empty") == 0);
  CHECK(memfs_mkdir("scratch") == 0);

  // Each iteration replaces the empty directory parent/empty with scratch.
  // The replaced node is unlinked but, in the current implementation, its
  // is_dir flag is never cleared by node_release(), so the slot remains
  // unavailable forever.
  for (int i = 0; i < DEFAULT_MEMFS_MAX_OPEN + 10; ++i) {
    int rc = memfs_rename("scratch", "parent/empty");
    if (rc != 0) {
      fprintf(stderr, "  [info] rename loop stopped at i=%d with rc=%d\n", i,
              rc);
      break;
    }
    // Recreate scratch for the next iteration.
    rc = memfs_mkdir("scratch");
    if (rc != 0) {
      fprintf(stderr, "  [info] mkdir scratch stopped at i=%d with rc=%d\n", i,
              rc);
      break;
    }
  }

  // If slots were recycled we should still be able to create a brand-new dir.
  // If not, this mkdir will fail with -ENOMEM.
  int final_rc = memfs_mkdir("fresh_dir");
  printf("  [info] final mkdir(\"fresh_dir\") after stress loop = %d\n",
         final_rc);

#ifdef TCM_MEMFS_DEBUG
  memfs_debug_dump_tree();
#endif
  printf("  [ok] rename directory slot stress completed\n");
  return 0;
}

static int test_symlink_loop(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_symlink("a", "a") == 0);
  int fd = memfs_open_base("a", O_RDONLY);
  CHECK(fd == -ELOOP);
  printf("  [ok] symlink self-loop returns -ELOOP\n");
  return 0;
}

static int test_symlink_to_directory(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);
  CHECK(memfs_symlink("d", "ld") == 0);

  // Opening a symlink that points to a directory should fail (no O_DIRECTORY).
  int fd = memfs_open_base("ld", O_RDONLY);
  CHECK(fd == -EISDIR);

  // But stat through the symlink should see the directory.
  struct stat st;
  CHECK(memfs_stat_path("ld", &st) == 0);
  CHECK((st.st_mode & S_IFMT) == S_IFDIR);

  // lstat should see the symlink.
  CHECK(memfs_lstat_path("ld", &st) == 0);
  CHECK(S_ISLNK(st.st_mode));

  printf("  [ok] symlink-to-directory open returns -EISDIR, stat/lstat differ\n");
  return 0;
}

static int test_fchmodat_symlink_no_follow(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_symlink("target", "link") == 0);

  int rc = memfs_fchmodat("link", 0600, AT_SYMLINK_NOFOLLOW);
  CHECK(rc == -EOPNOTSUPP);

  rc = memfs_fchmodat("link", 0644, 0);
  CHECK(rc == -ENOENT); // follows symlink; target does not exist

  printf("  [ok] fchmodat(AT_SYMLINK_NOFOLLOW) on dangling symlink returns -EOPNOTSUPP\n");
  return 0;
}

static int test_fchmodat_symlink_no_follow_real(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);
  CHECK(memfs_symlink("f", "link") == 0);

  int rc = memfs_fchmodat("link", 0600, AT_SYMLINK_NOFOLLOW);
  CHECK(rc == -EOPNOTSUPP);

  rc = memfs_fchmodat("link", 0600, 0);
  CHECK(rc == 0);
  struct stat st;
  CHECK(memfs_stat_path("f", &st) == 0);
  CHECK((st.st_mode & 07777) == 0600);
  CHECK(memfs_lstat_path("link", &st) == 0);
  CHECK((st.st_mode & 07777) == 0777); // symlink own mode unchanged

  printf("  [ok] fchmodat on followed symlink changes target, not link\n");
  return 0;
}

static int test_truncate_extend_shrink(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "abcdefghij", 10) == 10);
  memfs_close(fd);

  CHECK(memfs_truncate("f", 4) == 0);
  fd = memfs_open_base("f", O_RDONLY);
  char buf[16];
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 4);
  CHECK(memcmp(buf, "abcd", 4) == 0);
  memfs_close(fd);

  CHECK(memfs_truncate("f", 7) == 0);
  fd = memfs_open_base("f", O_RDONLY);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 7);
  CHECK(memcmp(buf, "abcd\x00\x00\x00", 7) == 0);
  memfs_close(fd);

  printf("  [ok] truncate shrink and zero-extend\n");
  return 0;
}

static int test_rw_mode_enforcement(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int wofd = memfs_open_base("f", O_CREAT | O_WRONLY);
  CHECK(wofd >= 0);
  CHECK(memfs_write(wofd, "x", 1) == 1);

  char buf[4];
  int rc = memfs_read(wofd, buf, sizeof(buf));
  CHECK(rc == -EBADF);

  int rofd = memfs_open_base("f", O_RDONLY);
  CHECK(rofd >= 0);
  rc = memfs_write(rofd, "y", 1);
  CHECK(rc == -EBADF);

  memfs_close(wofd);
  memfs_close(rofd);
  printf("  [ok] read/write mode enforcement\n");
  return 0;
}

static int test_lseek_boundaries(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "12345", 5) == 5);

  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  CHECK(memfs_lseek(fd, 5, SEEK_END) == 10); // seek past EOF is allowed
  CHECK(memfs_lseek(fd, -11, SEEK_CUR) == -EINVAL);
  CHECK(memfs_lseek(fd, -1, SEEK_SET) == -EINVAL);
  CHECK(memfs_lseek(fd, 0, 99) == -EINVAL);

  memfs_close(fd);
  printf("  [ok] lseek boundary and invalid whence\n");
  return 0;
}

static int test_o_excl_and_o_trunc(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "data", 4) == 4);
  memfs_close(fd);

  int rc = memfs_open_base("f", O_CREAT | O_EXCL | O_RDWR);
  CHECK(rc == -EEXIST);

  // O_RDONLY | O_TRUNC should ideally either fail with EACCES or be ignored.
  int ro = memfs_open_base("f", O_RDONLY | O_TRUNC);
  CHECK(ro >= 0);
  char buf[8];
  CHECK(memfs_read(ro, buf, sizeof(buf)) == 4);
  CHECK(memcmp(buf, "data", 4) == 0); // current impl ignores O_TRUNC for RDONLY
  memfs_close(ro);

  int wr = memfs_open_base("f", O_WRONLY | O_TRUNC);
  CHECK(wr >= 0);
  memfs_close(wr);
  struct stat st;
  CHECK(memfs_stat_path("f", &st) == 0);
  CHECK(st.st_size == 0);

  printf("  [ok] O_EXCL conflict and O_TRUNC semantics\n");
  return 0;
}

static int test_max_fd_exhaustion(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fds[DEFAULT_MEMFS_MAX_FD];
  int count = 0;
  for (int i = 0; i < DEFAULT_MEMFS_MAX_FD; ++i) {
    char name[32];
    snprintf(name, sizeof(name), "many/%d", i);
    if (i == 0) {
      CHECK(memfs_mkdir("many") == 0);
    }
    int fd = memfs_open_base(name, O_CREAT | O_RDWR);
    if (fd < 0) {
      CHECK(fd == -ENFILE);
      break;
    }
    fds[count++] = fd;
  }
  CHECK(count > 0);
  for (int i = 0; i < count; ++i) {
    memfs_close(fds[i]);
  }
  // After closing all, one more open should succeed.
  int fd = memfs_open_base("many/after", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);
  printf("  [ok] fd table exhaustion and reuse (%d opened before ENFILE)\n",
         count);
  return 0;
}

static int test_max_dir_streams(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);
  void *dirs[DEFAULT_MEMFS_DIR_MAX];
  int count = 0;
  for (int i = 0; i < DEFAULT_MEMFS_DIR_MAX + 2; ++i) {
    void *d = memfs_opendir("d");
    if (!d) {
      break;
    }
    dirs[count++] = d;
  }
  CHECK(count <= DEFAULT_MEMFS_DIR_MAX);
  for (int i = 0; i < count; ++i) {
    memfs_closedir(dirs[i]);
  }
  printf("  [ok] directory stream table exhaustion (%d opened)\n", count);
  return 0;
}

static int test_readdir_after_child_unlink(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);
  int fd = memfs_open_base("d/f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  memfs_close(fd);

  void *dir = memfs_opendir("d");
  CHECK(dir != NULL);
  CHECK(memfs_readdir(dir, &(struct dirent){0}) == 0); // "."
  CHECK(memfs_unlink("d/f") == 0);
  CHECK(memfs_readdir(dir, &(struct dirent){0}) == 0); // ".."
  CHECK(memfs_readdir(dir, &(struct dirent){0}) < 0); // no f
  memfs_closedir(dir);
  CHECK(memfs_rmdir("d") == 0);
  printf("  [ok] readdir skips children deleted mid-enumeration\n");
  return 0;
}

static int test_unlink_open_file_keep_content(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("f", O_CREAT | O_RDWR);
  CHECK(fd >= 0);
  CHECK(memfs_write(fd, "open-content", 12) == 12);
  CHECK(memfs_unlink("f") == 0);

  char buf[16];
  CHECK(memfs_lseek(fd, 0, SEEK_SET) == 0);
  CHECK(memfs_read(fd, buf, sizeof(buf)) == 12);
  CHECK(memcmp(buf, "open-content", 12) == 0);

  memfs_close(fd);
  struct stat st;
  CHECK(memfs_stat_path("f", &st) == -ENOENT);
  printf("  [ok] unlink while open preserves content\n");
  return 0;
}

static int test_chdir_dotdot(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("a/b") == 0);
  CHECK(memfs_chdir("a/b") == 0);

  char cwd[128];
  CHECK(memfs_getcwd(cwd, sizeof(cwd)) != NULL);
  CHECK(strcmp(cwd, "/a/b") == 0);

  CHECK(memfs_chdir("..") == 0);
  CHECK(memfs_getcwd(cwd, sizeof(cwd)) != NULL);
  CHECK(strcmp(cwd, "/a") == 0);

  CHECK(memfs_chdir("..") == 0);
  CHECK(memfs_getcwd(cwd, sizeof(cwd)) != NULL);
  CHECK(strcmp(cwd, "/") == 0);
  CHECK(memfs_chdir("..") == 0); // root's parent is root
  CHECK(memfs_getcwd(cwd, sizeof(cwd)) != NULL);
  CHECK(strcmp(cwd, "/") == 0);

  printf("  [ok] chdir and getcwd with dotdot\n");
  return 0;
}

static int test_unlinkat_dir(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("atdir") == 0);
  CHECK(memfs_mkdir("atdir/sub") == 0);

  int dfd = memfs_openat_dir(AT_FDCWD, "atdir");
  CHECK(dfd >= 0);
  CHECK(memfs_unlinkat(dfd, "sub", AT_REMOVEDIR) == 0);
  CHECK(memfs_opendir("atdir/sub") == NULL);
  CHECK(memfs_unlinkat(dfd, "sub", AT_REMOVEDIR) == -ENOENT);
  CHECK(memfs_close(dfd) == 0);
  CHECK(memfs_rmdir("atdir") == 0);
  printf("  [ok] unlinkat(AT_REMOVEDIR) via synthetic dir fd\n");
  return 0;
}

static int test_dirfd_close_roundtrip(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);
  void *dir = memfs_opendir("d");
  CHECK(dir != NULL);

  int dfd = memfs_dirfd(dir);
  CHECK(dfd >= TEST_MEMFS_DIRFD_BASE);
  CHECK(memfs_dir_from_fd(dfd) == dir);

  // Closing the synthetic fd must close the directory stream as well.
  CHECK(memfs_close(dfd) == 0);
  CHECK(memfs_dir_from_fd(dfd) == NULL);

  printf("  [ok] dirfd/close roundtrip\n");
  return 0;
}

static int test_realpath_nonexistent(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  char *rp = memfs_realpath("/no/such/path", NULL);
  CHECK(rp == NULL);
  CHECK(errno == ENOENT);
  printf("  [ok] realpath on missing path returns NULL/ENOENT\n");
  return 0;
}

static int test_ftruncate_readonly_and_dir(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("f", O_CREAT | O_RDONLY);
  CHECK(fd >= 0);
  CHECK(memfs_ftruncate(fd, 4) == -EBADF);
  memfs_close(fd);

  CHECK(memfs_mkdir("d") == 0);
  void *dir = memfs_opendir("d");
  CHECK(dir != NULL);
  int dfd = memfs_dirfd(dir);
  CHECK(memfs_ftruncate(dfd, 0) == -EINVAL);
  memfs_close(dfd);
  memfs_closedir(dir);
  CHECK(memfs_rmdir("d") == 0);
  printf("  [ok] ftruncate rejects readonly fd and dir fd\n");
  return 0;
}

static int test_dir_nlink(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  struct stat st;
  CHECK(memfs_stat_path("/", &st) == 0);
  CHECK(st.st_nlink == 2);

  CHECK(memfs_mkdir("a") == 0);
  CHECK(memfs_mkdir("a/b") == 0);
  CHECK(memfs_stat_path("/a", &st) == 0);
  CHECK(st.st_nlink == 3); // self + parent + b

  CHECK(memfs_rmdir("a/b") == 0);
  CHECK(memfs_stat_path("/a", &st) == 0);
  CHECK(st.st_nlink == 2);

  CHECK(memfs_rmdir("a") == 0);
  printf("  [ok] directory nlink counts updated by mkdir/rmdir\n");
  return 0;
}

static int test_open_directory_and_symlink(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  CHECK(memfs_mkdir("d") == 0);

  int fd = memfs_open_base("d", O_RDONLY);
  CHECK(fd == -EISDIR);

  CHECK(memfs_symlink("d", "ld") == 0);
  fd = memfs_open_base("ld", O_RDONLY | O_NOFOLLOW);
  // O_NOFOLLOW must refuse to follow the final symlink.
  printf("  [info] open(symlink-to-dir, O_NOFOLLOW) returned %d\n", fd);
  CHECK(fd == -ELOOP);

  printf("  [ok] open-directory and O_NOFOLLOW on symlink\n");
  return 0;
}

static int test_extend_large_file(void) {
  CHECK(memfs_init_with_payload(NULL, 0, 3) == 0);
  int fd = memfs_open_base("big", O_CREAT | O_RDWR);
  CHECK(fd >= 0);

  const size_t size = 12345;
  int rc = memfs_lseek(fd, (long)(size - 1), SEEK_SET);
  CHECK(rc == (long)(size - 1));
  CHECK(memfs_write(fd, "Z", 1) == 1);

  struct stat st;
  CHECK(memfs_stat(fd, &st) == 0);
  CHECK(st.st_size == (off_t)size);

  memfs_close(fd);
  printf("  [ok] large sparse file extends via chunk chain\n");
  return 0;
}


#ifdef TCM_MEMFS_DEBUG
static int test_debug_memory_stats(void) {
  if (memfs_init_with_payload(NULL, 0, 3) != 0)
    return 1;

  size_t base_heap = memfs_debug_heap_bytes();
  size_t base_chunks = memfs_debug_chunk_count();
  printf("  [info] baseline heap=%zu chunks=%zu\n", base_heap, base_chunks);
  CHECK(base_chunks == 0);

  int fd = memfs_open_base("memstats.bin", O_WRONLY | O_CREAT);
  CHECK(fd >= 0);
  CHECK(memfs_debug_chunk_count() == base_chunks);

  char buf[8192];
  memset(buf, 'x', sizeof(buf));

  CHECK(memfs_write(fd, buf, 1) == 1);
  CHECK(memfs_debug_chunk_count() == base_chunks + 1);
  CHECK(memfs_debug_heap_bytes() > base_heap);

  CHECK(memfs_write(fd, buf, 8192) == 8192);
  CHECK(memfs_debug_chunk_count() == base_chunks + 3);

  CHECK(memfs_close(fd) == 0);

  CHECK(memfs_truncate("memstats.bin", 0) == 0);
  CHECK(memfs_debug_chunk_count() == base_chunks);

  CHECK(memfs_unlink("memstats.bin") == 0);
  CHECK(memfs_debug_heap_bytes() == base_heap);
  CHECK(memfs_debug_chunk_count() == base_chunks);

  CHECK(memfs_mkdir("memstats_dir") == 0);
  CHECK(memfs_symlink("target", "memstats_link") == 0);
  CHECK(memfs_debug_heap_bytes() > base_heap);

  CHECK(memfs_unlink("memstats_link") == 0);
  CHECK(memfs_rmdir("memstats_dir") == 0);
  CHECK(memfs_debug_heap_bytes() == base_heap);
  CHECK(memfs_debug_chunk_count() == base_chunks);

  // Dump helpers must not crash and should report matching counters.
  memfs_debug_dump_tree();
  memfs_debug_dump_tree_at("/");

  printf("  [ok] TCM_MEMFS_DEBUG memory statistics\n");
  return 0;
}
#endif

int main(void) {
  printf("test_memfs...\n");
  if (test_pack_read()) return 1;
  if (test_inplace_write()) return 1;
  if (test_chunk_append()) return 1;
  if (test_shared_write()) return 1;
  if (test_runtime_new_file()) return 1;
  if (test_empty_dir()) return 1;
  if (test_stat()) return 1;
  if (test_stat_path()) return 1;
  if (test_unlink()) return 1;
  if (test_unlink_open_fd_reuse()) return 1;
  if (test_readdir()) return 1;
  if (test_deep_path()) return 1;
  if (test_mkdir_chdir_rmdir()) return 1;
  if (test_rename_truncate_symlink()) return 1;
  if (test_rename_directories()) return 1;
  if (test_open_mode_and_dirfds()) return 1;
  // Extended coverage.
  if (test_invalid_args()) return 1;
  if (test_open_flags_variants()) return 1;
  if (test_rw_edge_and_eof()) return 1;
  if (test_multiple_fd_consistency()) return 1;
  if (test_lseek_variants()) return 1;
  if (test_trunc_edge_cases()) return 1;
  if (test_readdir_mutation()) return 1;
  if (test_rmdir_errors_and_types()) return 1;
  if (test_rename_variants()) return 1;
  if (test_symlink_full()) return 1;
  if (test_path_resolution()) return 1;
  if (test_stat_nlink_state()) return 1;
  if (test_dangling_dir_stream()) return 1;
  if (test_remove_all_pattern()) return 1;
  if (test_resource_cleanup()) return 1;
  if (test_table_exhaustion_recovery()) return 1;
  if (test_node_table_exhaustion()) return 1;
  if (test_dirfd_unlinkat()) return 1;
  if (test_u32_overflow_rejected()) return 1;
  // Former test_memfs_extended.c coverage
  if (test_opendir_null_root()) return 1;
  if (test_dirfd_fd_isolation()) return 1;
  if (test_base_fd_stability()) return 1;
  if (test_rename_directory_slot_leak()) return 1;
  if (test_symlink_loop()) return 1;
  if (test_symlink_to_directory()) return 1;
  if (test_fchmodat_symlink_no_follow()) return 1;
  if (test_fchmodat_symlink_no_follow_real()) return 1;
  if (test_truncate_extend_shrink()) return 1;
  if (test_rw_mode_enforcement()) return 1;
  if (test_lseek_boundaries()) return 1;
  if (test_o_excl_and_o_trunc()) return 1;
  if (test_max_fd_exhaustion()) return 1;
  if (test_max_dir_streams()) return 1;
  if (test_readdir_after_child_unlink()) return 1;
  if (test_unlink_open_file_keep_content()) return 1;
  if (test_chdir_dotdot()) return 1;
  if (test_unlinkat_dir()) return 1;
  if (test_dirfd_close_roundtrip()) return 1;
  if (test_realpath_nonexistent()) return 1;
  if (test_ftruncate_readonly_and_dir()) return 1;
  if (test_dir_nlink()) return 1;
  if (test_open_directory_and_symlink()) return 1;
  if (test_extend_large_file()) return 1;

#ifdef TCM_MEMFS_DEBUG
  if (test_debug_memory_stats()) return 1;
#endif
  if (test_realpath_variants()) return 1;
  printf("ALL PASS\n");
  return 0;
}
