/*
 * Platform abstraction — monotonic clock, high-resolution timer, and
 * console input helpers.
 *
 * All Windows / macOS / Linux time / input code lives here, so cpu.c /
 * peripheral.c never need #ifdef __APPLE__ / _WIN32 / __MINGW64__ blocks.
 */

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#else
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#endif

/* ==================================================================== */
/*  platform_utc_time — symmetric to clock_gettime(CLOCK_REALTIME)      */
/* ==================================================================== */

bool platform_utc_time(struct timespec *ts) {
#if defined(_WIN32)
  FILETIME ft;
  ULARGE_INTEGER ul;
  GetSystemTimePreciseAsFileTime(&ft);
  ul.LowPart = ft.dwLowDateTime;
  ul.HighPart = ft.dwHighDateTime;

  const uint64_t FILETIME_EPOCH_OFFSET = 116444736000000000ULL;
  uint64_t hundred_ns = ul.QuadPart - FILETIME_EPOCH_OFFSET;

  uint64_t total_ns = hundred_ns * 100ULL;
  ts->tv_sec = (time_t)(total_ns / 1000000000ULL);
  ts->tv_nsec = (long)(total_ns % 1000000000ULL);
  return true;

#else
  /* Linux / other POSIX — glibc vDSO, ~20 cycles, integer timespec */
  return clock_gettime(CLOCK_REALTIME, ts) == 0;
#endif
}

/* ==================================================================== */
/*  platform_timer_init — cache platform params + set zero-point        */
/* ==================================================================== */

bool platform_timer_init(PlatformTimer *t) {
#if defined(__APPLE__)
  {
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    t->tb_n = tb.numer;
    t->tb_d = tb.denom;
    t->start_raw = mach_absolute_time();
    t->last_raw = t->start_raw;
  }
  return true;

#elif defined(_WIN32)
  {
    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq))
      return false;
    t->freq = (uint64_t)freq.QuadPart;
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    t->start_raw = (uint64_t)cnt.QuadPart;
    t->last_raw = t->start_raw;
  }
  return true;

#else
  {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
      return false;
    t->start_raw = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    t->last_raw = t->start_raw;
  }
  return true;
#endif
}

/* ==================================================================== */
/*  platform_timer_elapsed_ns — integer‑nanosecond delta from last call  */
/* ==================================================================== */
/** Returns nanoseconds elapsed since the previous call (or init).
 *
 *  Thread-safety: per-instance; call from a single thread (the emulator
 *  main loop).  The raw-to-ns conversion uses only integer multiply +
 *  integer divide — zero floating-point.
 *
 *  Overflow safety: on every platform the tick-delta between two
 *  successive calls is bounded by the check-interval (~4 M instructions,
 *  typically < 10 ms).  Therefore delta_ticks × numerator fits easily
 *  within uint64_t (max ≈ 10 ms × GHz × 5 ≈ 5×10¹⁰ ≪ 2⁶⁴).             */

uint64_t platform_timer_elapsed_ns(PlatformTimer *t, uint64_t *total) {
#if defined(__APPLE__)
  {
    uint64_t now_raw = mach_absolute_time();
    uint64_t delta = now_raw - t->last_raw;
    uint64_t ns = delta * (uint64_t)t->tb_n / (uint64_t)t->tb_d;
    t->last_raw = now_raw;
    if (total) {
      delta = now_raw - t->start_raw;
      *total = delta * (uint64_t)t->tb_n / (uint64_t)t->tb_d;
    }
    return ns;
  }

#elif defined(_WIN32)
  {
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    uint64_t delta = (uint64_t)cnt.QuadPart - t->last_raw;
    uint64_t ns = delta * 1000000000ULL / t->freq;
    t->last_raw = (uint64_t)cnt.QuadPart;
    if (total) {
      delta = t->last_raw - t->start_raw;
      *total = delta * 1000000000ULL / t->freq;
    }
    return ns;
  }

#else
  {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ns =
        (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    uint64_t elapsed = now_ns - t->last_raw;
    t->last_raw = now_ns;
    if (total)
      *total = now_ns - t->start_raw;
    return elapsed;
  }
#endif
}

/* ==================================================================== */
/*  platform_wait_key — wait for a single key press                     */
/* ==================================================================== */

/** Wait for and return a single key press.
 *  @param wait   true = block until a key is pressed; false = return
 * immediately. Windows: uses _getch() (already non-echo, no Enter required).
 *  POSIX:   temporarily disables canonical mode and echo, restores the
 *           previous terminal settings before returning.
 *  Returns the character, or EOF on error/end-of-input. */
int platform_wait_key(bool wait) {
  if (!wait)
    return 0;

  printf("\n\nPress any key to continue...\n");
#if defined(_WIN32) || defined(_WIN64)
  return _getch();
#else
  struct termios old_attr;

  if (tcgetattr(STDIN_FILENO, &old_attr) != 0)
    return getchar();

  struct termios new_attr = old_attr;
  new_attr.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);

  int ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
  return ch;
#endif
}

/* ==================================================================== */
/*  Threads, mutexes, and sleep                                          */
/* ==================================================================== */

typedef struct {
  platform_thread_fn_t fn;
  void *arg;
} platform_thread_wrapper_t;

#if defined(_WIN32) || defined(_WIN64)

#include <process.h>

static DWORD WINAPI platform_thread_trampoline(LPVOID arg) {
  platform_thread_wrapper_t *w = (platform_thread_wrapper_t *)arg;
  w->fn(w->arg);
  free(w);
  return 0;
}

bool platform_thread_create(platform_thread_t *t, platform_thread_fn_t fn,
                            void *arg) {
  platform_thread_wrapper_t *w =
      (platform_thread_wrapper_t *)malloc(sizeof(*w));
  if (!w)
    return false;
  w->fn = fn;
  w->arg = arg;
  DWORD tid;
  t->handle = CreateThread(NULL, 0, platform_thread_trampoline, w, 0, &tid);
  if (!t->handle) {
    free(w);
    return false;
  }
  return true;
}

void platform_thread_join(platform_thread_t *t) {
  if (t->handle) {
    WaitForSingleObject(t->handle, INFINITE);
    CloseHandle(t->handle);
    t->handle = NULL;
  }
}

bool platform_mutex_init(platform_mutex_t *m) {
  m->cs =
      HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CRITICAL_SECTION));
  if (!m->cs)
    return false;
  InitializeCriticalSection((CRITICAL_SECTION *)m->cs);
  return true;
}

void platform_mutex_lock(platform_mutex_t *m) {
  EnterCriticalSection((CRITICAL_SECTION *)m->cs);
}

void platform_mutex_unlock(platform_mutex_t *m) {
  LeaveCriticalSection((CRITICAL_SECTION *)m->cs);
}

void platform_mutex_destroy(platform_mutex_t *m) {
  if (m->cs) {
    DeleteCriticalSection((CRITICAL_SECTION *)m->cs);
    HeapFree(GetProcessHeap(), 0, m->cs);
    m->cs = NULL;
  }
}

void platform_sleep_ms(uint32_t ms) { Sleep((DWORD)ms); }

#else /* POSIX */

static void *platform_thread_trampoline(void *arg) {
  platform_thread_wrapper_t *w = (platform_thread_wrapper_t *)arg;
  w->fn(w->arg);
  free(w);
  return NULL;
}

bool platform_thread_create(platform_thread_t *t, platform_thread_fn_t fn,
                            void *arg) {
  platform_thread_wrapper_t *w =
      (platform_thread_wrapper_t *)malloc(sizeof(*w));
  if (!w)
    return false;
  w->fn = fn;
  w->arg = arg;
  if (pthread_create(&t->tid, NULL, platform_thread_trampoline, w) != 0) {
    free(w);
    return false;
  }
  return true;
}

void platform_thread_join(platform_thread_t *t) { pthread_join(t->tid, NULL); }

bool platform_mutex_init(platform_mutex_t *m) {
  m->mtx = malloc(sizeof(pthread_mutex_t));
  if (!m->mtx)
    return false;
  if (pthread_mutex_init((pthread_mutex_t *)m->mtx, NULL) != 0) {
    free(m->mtx);
    m->mtx = NULL;
    return false;
  }
  return true;
}

void platform_mutex_lock(platform_mutex_t *m) {
  pthread_mutex_lock((pthread_mutex_t *)m->mtx);
}

void platform_mutex_unlock(platform_mutex_t *m) {
  pthread_mutex_unlock((pthread_mutex_t *)m->mtx);
}

void platform_mutex_destroy(platform_mutex_t *m) {
  if (m->mtx) {
    pthread_mutex_destroy((pthread_mutex_t *)m->mtx);
    free(m->mtx);
    m->mtx = NULL;
  }
}

void platform_sleep_ms(uint32_t ms) {
  if (ms == 0) {
    sched_yield();
    return;
  }
  usleep((useconds_t)ms * 1000U);
}

#endif