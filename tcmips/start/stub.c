//
// Created by zhangjiantao on 2026/5/7.
//

#include <dev/console.h>
#include <dev/seven_segment_display.h>
#include <dev/syscall.h>

#define _POSIX_MONOTONIC_CLOCK // NOLINT

#include <picolibc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_timeval.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <time.h>

#define SHUTDOWN_CODE_EXIT 0x00000000
#define SHUTDOWN_CODE_DEAD 0xdeadbeef

// NOLINTNEXTLINE
__attribute__((noreturn)) void __shutdown(uint32_t code, uint32_t param) {
  tcm_seven_segment_display_upper_hexadecimal(code);
  tcm_seven_segment_display_lower_hexadecimal(param);
  while (1)
    __builtin_trap();
}

// Use POSIX read/write to support stdin, stdout, stderr for better performance.
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

#define FAKE_FD_99 99
#define FAKE_FD_88 88

int close(int fd) { return 0; }

static off_t virtual_cfg_pos = 0;
off_t lseek(int fd, off_t offset, int whence) {
  if (fd == FAKE_FD_88 || fd == FAKE_FD_99) {
    if (whence == SEEK_SET) {
      virtual_cfg_pos = offset;
    } else if (whence == SEEK_CUR) {
      virtual_cfg_pos += offset;
    } else if (whence == SEEK_END) {
      virtual_cfg_pos = 0 + offset;
    }
    return virtual_cfg_pos;
  }

  __shutdown(SHUTDOWN_CODE_DEAD, (uint32_t)__builtin_return_address(0));
}

int open(const char *pathname, int flags, ...) {
  if (strstr(pathname, "default.cfg") != NULL ||
      strstr(pathname, ".cfg") != NULL)
    return FAKE_FD_99;

  if (strstr(pathname, "doom1.wad") != NULL || strstr(pathname, ".wad") != NULL)
    return FAKE_FD_88;

  errno = ENOENT;
  return -1;
}

ssize_t read(int fd, void *buf, size_t nbyte) {
  if (fd == FAKE_FD_99 || fd == FAKE_FD_88)
    return 0;

  if (fd == 0 && nbyte >= 1) {
    if (nbyte >= 2) {
      char *p = buf;
      uint32_t sz = tcm_ascii_console_read_string(buf, nbyte);
      p[sz] = '\n'; // todo: not icanon mode
      return ((ssize_t)sz + 1);
    }
    if (nbyte == 1) {
      ((char *)buf)[0] = tcm_ascii_console_read_char();
      return 1;
    }
  }
  return 0;
}

ssize_t write(int fd, const void *buf, size_t nbyte) {
  if (fd == FAKE_FD_99)
    return (ssize_t)nbyte;

  if (fd == 1 || fd == 2) {
    const char *p = buf;
    while (*p && nbyte--)
      tcm_ascii_console_write_char(*p++);
    return p - (const char *)buf;
  }
  return 0;
}

// NOLINTNEXTLINE
int unlink(const char *__path) { return 0; }

#endif

// NOLINTNEXTLINE
void _exit(int status) { __shutdown(SHUTDOWN_CODE_EXIT, status); }

// NOLINTNEXTLINE
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

// NOLINTNEXTLINE
int stat(const char *path, struct stat *statbuf);

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
  if (__useconds > 1000000000)
    __useconds = 1000000000;

  uint32_t start_sec = tcm_syscall_get_timestamp();
  uint32_t start_mic = tcm_syscall_get_timestamp_micro();
  uint32_t current_sec, current_mic;
  do {
    current_sec = tcm_syscall_get_timestamp();
    current_mic = tcm_syscall_get_timestamp_micro();
    if (current_mic < start_mic)
      current_mic += 1000000;
  } while (((current_sec - start_sec) * 1000000 + current_mic - start_mic) <
           __useconds);

  return 0;
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
