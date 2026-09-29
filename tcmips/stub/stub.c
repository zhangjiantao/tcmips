//
// Created by zhangjiantao on 2026/5/7.
//

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>
#include <tcm_memfs.h>

#define _POSIX_MONOTONIC_CLOCK // NOLINT

#include <dirent.h>
#include <fcntl.h>
#include <picolibc.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_timeval.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <time.h>

extern uintptr_t __heap_start; // NOLINT
extern uintptr_t __heap_end;   // NOLINT

#define SHUTDOWN_CODE_EXIT 0x00000000
#define SHUTDOWN_CODE_DEAD 0xdeadbeef

// NOLINTNEXTLINE
__attribute__((noreturn)) void __shutdown(uint32_t code, uint32_t param) {
  tcm_seven_segment_display_upper_hexadecimal(code);
  tcm_seven_segment_display_lower_hexadecimal(param);
  while (1) {
    __asm__ __volatile__( //
        "move $a0, %0\n\t"
        "move $a1, %1\n\t"
        "break 0x3ff\n\t"
        :
        : "r"(code), "r"(param)
        : "memory");
  }
}

// Console I/O uses a host semihosting bridge when available (e.g., when the
// TCM binary is executed under the host emulator), falling back to the in-game
// text console otherwise. File descriptors >= MEMFS_BASE_FD remain under
// libmemfs control.
#ifndef POSIX_CONSOLE
static int stub_putc(char c, FILE *f) {
  init_console();
  tcm_text_console_write_char(c);
  return c;
}

static int stub_getc(FILE *f) {
  init_console();
  return tcm_text_console_read_char();
}

static FILE __stdio = // NOLINT
    FDEV_SETUP_STREAM(stub_putc, stub_getc, NULL, _FDEV_SETUP_RW);

FILE *const stdin = &__stdio;
FILE *const stdout = &__stdio;
FILE *const stderr = &__stdio;

#else

// memfs owns file descriptors starting at 3; console keeps 0/1/2
// (stdin/stdout/stderr).
#define MEMFS_BASE_FD 3

int open(const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  if (flags & O_DIRECTORY) {
    int fd = memfs_openat_dir(AT_FDCWD, pathname);
    if (fd < 0) {
      errno = -fd;
      return -1;
    }
    return fd;
  }
  int fd = memfs_open_mode(pathname, flags, MEMFS_BASE_FD, (unsigned int)mode);
  if (fd < 0) {
    errno = -fd;
    return -1;
  }
  return fd;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  if (flags & O_DIRECTORY) {
    int fd = memfs_openat_dir(dirfd, pathname);
    if (fd < 0) {
      errno = -fd;
      return -1;
    }
    return fd;
  }

  // For regular files, resolve the absolute path relative to the dir fd and
  // delegate to open().
  if (dirfd == AT_FDCWD || pathname[0] == '/')
    return open(pathname, flags, mode);

  DIR *dir = (DIR *)memfs_dir_from_fd(dirfd);
  if (!dir) {
    errno = ENOTDIR;
    return -1;
  }
  char base[4096];
  if (memfs_dirpath(dir, base, sizeof(base)) < 0) {
    errno = ENAMETOOLONG;
    return -1;
  }
  size_t blen = strlen(base);
  size_t nlen = strlen(pathname);
  if (blen + 1 + nlen + 1 > sizeof(base)) {
    errno = ENAMETOOLONG;
    return -1;
  }
  char joined[4096];
  if (blen == 1 && base[0] == '/') {
    joined[0] = '/';
    memcpy(joined + 1, pathname, nlen + 1);
  } else {
    memcpy(joined, base, blen);
    joined[blen] = '/';
    memcpy(joined + blen + 1, pathname, nlen + 1);
  }
  return open(joined, flags, mode);
}

int dirfd(DIR *dir) {
  if (!dir) {
    errno = EINVAL;
    return -1;
  }
  int rc = memfs_dirfd((void *)dir);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return rc;
}

DIR *fdopendir(int fd) {
  void *dir = memfs_dir_from_fd(fd);
  if (!dir) {
    errno = ENOTDIR;
    return NULL;
  }
  return (DIR *)dir;
}

int unlinkat(int dirfd, const char *name, int flags) {
  int rc = memfs_unlinkat(dirfd, name, flags);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

long pathconf(const char *path, int name) {
  (void)path;
  long rc = memfs_pathconf(path, name);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return rc;
}

off_t lseek(int fd, off_t offset, int whence) {
  if (fd < MEMFS_BASE_FD) {
    errno = ESPIPE; // console streams are not seekable
    return (off_t)-1;
  }
  off_t rc = (off_t)memfs_lseek(fd, offset, whence);
  if (rc < 0) {
    errno = -rc;
    return (off_t)-1;
  }
  return rc;
}

ssize_t read(int fd, void *buf, size_t nbyte) {
  if (fd >= MEMFS_BASE_FD) {
    ssize_t rc = (ssize_t)memfs_read(fd, buf, nbyte);
    if (rc < 0) {
      errno = -rc;
      return (ssize_t)-1;
    }
    return rc;
  }

  if (fd == 0 && nbyte >= 1) {
    uint32_t sraw = tcm_syscall_semihost_stdin((uint32_t)buf, nbyte);

    if (sraw == 0xFFFFFFFFu) {
      // No semihost stdin: fall back to the in-game text console.
      if (nbyte >= 2) {
        char *p = buf;
        uint32_t sz = tcm_text_console_read_string(buf, nbyte);
        p[sz] = '\n'; // todo: not icanon mode
        return ((ssize_t)sz + 1);
      }
      if (nbyte == 1) {
        ((char *)buf)[0] = tcm_text_console_read_char();
        return 1;
      }
      return 0;
    }

    return (ssize_t)sraw;
  }
  return 0;
}

ssize_t write(int fd, const void *buf, size_t nbyte) {
  if (fd >= MEMFS_BASE_FD) {
    ssize_t rc = (ssize_t)memfs_write(fd, buf, nbyte);
    if (rc < 0) {
      errno = -rc;
      return (ssize_t)-1;
    }
    return rc;
  }

  // stdout/stderr (fd 1/2) write to the text console.
  if (fd == 1 || fd == 2) {
    if (fd == 1)
      tcm_syscall_semihost_stdout((uint32_t)buf, nbyte);
    if (fd == 2)
      tcm_syscall_semihost_stderr((uint32_t)buf, nbyte);

    const char *p = buf;
    for (size_t i = 0; i < nbyte; ++i)
      tcm_text_console_write_char(p[i]);
    return (ssize_t)nbyte;
  }
  return 0;
}

int close(int fd) {
  if (fd >= MEMFS_BASE_FD) {
    int rc = memfs_close(fd);
    if (rc < 0) {
      errno = -rc;
      return -1;
    }
    return rc;
  }
  return 0; // console fds are not individually closable
}

// NOLINTNEXTLINE
int unlink(const char *__path) {
  int rc = memfs_unlink(__path);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int stat(const char *path, struct stat *statbuf) {
  int rc = memfs_stat_path(path, statbuf);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

// libc++ <filesystem> calls lstat() for symlink_status(); it must NOT follow
// the final symbolic link.
int lstat(const char *path, struct stat *statbuf) {
  int rc = memfs_lstat_path(path, statbuf);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

// fstat() on a memfs fd (fstat-style path), used by <filesystem> status(fd).
int fstat(int fd, struct stat *statbuf) {
  if (fd < MEMFS_BASE_FD) {
    errno = EBADF;
    return -1;
  }
  int rc = memfs_stat(fd, statbuf);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

// access(): memfs is a writable RAM volume; F_OK and R/W checks reduce to
// "does the path exist". X_OK is always granted (no execute permissions model).
int access(const char *path, int amode) {
  struct stat sb;
  int rc = memfs_stat_path(path, &sb);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  (void)amode;
  return 0;
}

// Current working directory is managed by libmemfs (supports real chdir).
char *getcwd(char *buf, size_t size) {
  if (size == 0) {
    errno = EINVAL;
    return NULL;
  }
  return memfs_getcwd(buf, size);
}

int chdir(const char *path) {
  int rc = memfs_chdir(path);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int mkdir(const char *path, mode_t mode) {
  int rc = memfs_mkdir_mode(path, (unsigned int)mode);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int rmdir(const char *path) {
  int rc = memfs_rmdir(path);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

// ---- additional POSIX file operations needed by libc++ <filesystem>
// ----------

int rename(const char *oldpath, const char *newpath) {
  int rc = memfs_rename(oldpath, newpath);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int truncate(const char *path, off_t length) {
  int rc = memfs_truncate(path, (long)length);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int ftruncate(int fd, off_t length) {
  if (fd < MEMFS_BASE_FD) {
    errno = EBADF;
    return -1;
  }
  int rc = memfs_ftruncate(fd, (long)length);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int chmod(const char *path, mode_t mode) {
  int rc = memfs_chmod(path, (unsigned int)mode);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int fchmod(int fd, mode_t mode) {
  if (fd < MEMFS_BASE_FD) {
    errno = EBADF;
    return -1;
  }
  int rc = memfs_fchmod(fd, (unsigned int)mode);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
  (void)dirfd;
  int rc = memfs_fchmodat(path, (unsigned int)mode, flags);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

int symlink(const char *target, const char *linkpath) {
  int rc = memfs_symlink(target, linkpath);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
  ssize_t rc = memfs_readlink(path, buf, bufsiz);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return rc;
}

char *realpath(const char *path, char *resolved_path) {
  return memfs_realpath(path, resolved_path);
}

// Hard links are not modeled by memfs' single-parent directory tree. Report
// EPERM so libc++ copy falls back to a regular copy.
int link(const char *oldpath, const char *newpath) {
  (void)oldpath;
  (void)newpath;
  errno = EPERM;
  return -1;
}

// C/POSIX remove(): try unlink first, then rmdir for empty directories.
int remove(const char *path) {
  int rc = memfs_unlink(path);
  if (rc == -EISDIR)
    rc = memfs_rmdir(path);
  if (rc < 0) {
    errno = -rc;
    return -1;
  }
  return 0;
}

// Time stamps are not meaningful on this in-memory volume.
int utimes(const char *path, const struct timeval times[2]) {
  (void)path;
  (void)times;
  return 0;
}

int utimensat(int dirfd, const char *path, const struct timespec times[2],
              int flags) {
  (void)dirfd;
  (void)path;
  (void)times;
  (void)flags;
  return 0;
}

// Directory enumeration. memfs now maintains real directory nodes; the BSP
// adapts the opaque memfs handle to the picolibc DIR / struct dirent types.
// readdir() returns a pointer into the DIR stream's own buffer, valid until the
// next readdir/closedir, matching the libc contract (and the pattern libc++'s
// <filesystem> depends on).
DIR *opendir(const char *name) {
  DIR *d = (DIR *)memfs_opendir(name);
  if (!d)
    errno = ENOENT;
  return d;
}

struct dirent *readdir(DIR *dir) {
  if (!dir)
    return NULL;
  if (memfs_readdir(dir, &dir->dd_dirent) < 0)
    return NULL;
  return &dir->dd_dirent;
}

int closedir(DIR *dir) { return memfs_closedir(dir); }

// libc++ <filesystem> fs::space() needs statvfs(). Memfs is a single RAM
// volume; report the heap range as the volume size. Free-block accounting is
// approximate (we do not track every malloc), so claim the whole heap as
// available.
int statvfs(const char *path, struct statvfs *buf) {
  (void)path;
  if (!buf) {
    errno = EINVAL;
    return -1;
  }
  memset(buf, 0, sizeof(*buf));
  buf->f_bsize = 1;
  buf->f_frsize = 1;
  buf->f_blocks = (fsblkcnt_t)(__heap_end - __heap_start);
  buf->f_bfree = buf->f_blocks;
  buf->f_bavail = buf->f_blocks;
  buf->f_namemax = MEMFS_NAME_MAX - 1;
  return 0;
}

#endif

// NOLINTNEXTLINE
void _exit(int status) {
  /* Mask the guest exit code to [0, 127] so it can never collide with the
   * emulator's own reserved exit codes ([128, 255]; see emulator/emulator_exit.h
   * and AUTOTEST.md).  A guest calling exit(130) still yields 2, never 130, so
   * "window closed by user" (130) remains unambiguous. */
  __shutdown(SHUTDOWN_CODE_EXIT, (uint32_t)status & 0x7Fu);
}

// NOLINTNEXTLINE
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

int getentropy(void *buffer, size_t length) {
  uint8_t *buf = (uint8_t *)buffer;
  for (size_t i = 0; i < length; ++i)
    buf[i] = tcm_syscall_get_timestamp_micro() & 0xff;
  return 0;
}

clock_t times(struct tms *buf) {
  extern struct timeval __tcm_startup_time; // NOLINT

  struct timeval now;
  gettimeofday(&now, NULL);

#if CLOCKS_PER_SEC != 1000000
#error ???
#endif

  clock_t tk = (now.tv_sec - __tcm_startup_time.tv_sec) * CLOCKS_PER_SEC;

  tk = tk + (now.tv_usec > __tcm_startup_time.tv_usec
                 ? now.tv_usec - __tcm_startup_time.tv_usec
                 : -(__tcm_startup_time.tv_usec - now.tv_usec));

  if (buf) {
    buf->tms_utime = tk;
    buf->tms_stime = 0;
    buf->tms_cutime = 0;
    buf->tms_cstime = 0;
  }
  return tk;
}

char *_user_strerror(int errnum, int internal, int *errptr) {
  return "_user_strerror";
}

int usleep(useconds_t __useconds) { // NOLINT
  if (__useconds == 0)
    return 0;
  if (__useconds > 1000000000UL)
    __useconds = 1000000000UL;

  uint32_t start_sec = tcm_syscall_get_timestamp();
  uint32_t start_mic = tcm_syscall_get_timestamp_micro();
  uint32_t current_sec, current_mic;
  do {
    current_sec = tcm_syscall_get_timestamp();
    current_mic = tcm_syscall_get_timestamp_micro();
    if (current_mic < start_mic)
      current_mic += 1000000UL;
  } while (((current_sec - start_sec) * 1000000UL + current_mic - start_mic) <
           __useconds);

  return 0;
}

unsigned sleep(unsigned int __seconds) { // NOLINT
  return usleep(__seconds * 1000000UL);
}

int gettimeofday(struct timeval *__tv, void *__tz) { // NOLINT
  if (__tv == NULL)
    return -1;

  uint32_t sec = tcm_syscall_get_timestamp();
  uint32_t micro = tcm_syscall_get_timestamp_micro();

  // Retry if a second rollover occurred during read
  if (__builtin_expect(micro < 1000, 0)) {
    sec = tcm_syscall_get_timestamp();
    micro = tcm_syscall_get_timestamp_micro();
  }

  __tv->tv_sec = sec;
  __tv->tv_usec = (suseconds_t)micro;
  return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp) {
  if (tp == NULL)
    return -1;

  uint32_t sec = tcm_syscall_get_timestamp();
  uint32_t nano = tcm_syscall_get_timestamp_nano();

  // Retry if a second rollover occurred during read
  if (__builtin_expect(nano < 1000000, 0)) {
    sec = tcm_syscall_get_timestamp();
    nano = tcm_syscall_get_timestamp_micro();
  }

  if (clock_id == CLOCK_MONOTONIC) {
    extern struct timeval __tcm_startup_time; // NOLINT
    sec -= __tcm_startup_time.tv_sec;
  }

  tp->tv_sec = sec;
  tp->tv_nsec = (long)nano;
  return 0;
}
