/*
 * Platform abstraction — monotonic clock, high-resolution timer, thread,
 * mutex, sleep and console input helpers.
 *
 * Centralises all Windows / macOS / Linux platform-specific code, so
 * cpu.c / peripheral.c never need #ifdef __APPLE__ / _WIN32 / __MINGW64__
 * in their hot loops.
 *
 * APIs provided:
 *   1. platform_mono_now()           — returns struct timespec (integer,
 *                                      symmetric to clock_gettime).  Used by
 *                                      the syscall subsystem.
 *   2. PlatformTimer / init / elapsed — integer-nanosecond high-resolution
 *                                      timer for frame pacing.  Zero
 *                                      floating-point arithmetic.
 *   3. platform_wait_key()           — wait for a single key press without
 *                                      requiring Enter (POSIX).
 *   4. platform_thread_xxx / platform_mutex_xxx / platform_sleep_ms() —
 *                                      cross-platform threading primitives
 *                                      used by the async render thread.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

bool platform_utc_time(struct timespec *ts);

/* ── High-resolution monotonic timer (nanoseconds, integer only) ────── */

/**
 * Platform-specific timer for frame pacing — zero floating-point.
 *
 *  - macOS:  stores mach_absolute_time() tick counts; elapsed ns =
 *              delta_ticks * timebase_numer / timebase_denom
 *  - Windows: stores QPC tick counts;       elapsed ns =
 *              delta_ticks * 1 000 000 000 / frequency
 *  - Linux:  stores vDSO clock_gettime(CLOCK_MONOTONIC) nanoseconds;
 *              elapsed ns = delta_ns (single subtraction, vDSO is fast)
 *
 *  Because delta is always a small interval between two successive calls,
 *  delta × numerator never overflows uint64_t (max delta ≈ seconds ×
 *  GHz ≤ 10¹⁰, × numerator ≤ 10¹² — well within 2⁶⁴).
 */
typedef struct {
  uint64_t start_raw;
  uint64_t last_raw; /* Raw counter / ns value captured at last call    */
#if defined(__APPLE__)
  uint32_t tb_n; /* mach_timebase numer (typically 1-3)            */
  uint32_t tb_d; /* mach_timebase denom (typically 1-3)            */
#elif defined(_WIN32)
  uint64_t freq; /* QueryPerformanceFrequency (typically 1-4 MHz)  */
#endif
} PlatformTimer;

/** Initialise a PlatformTimer.
 *  Caches the platform-specific parameters into t, and records the current
 *  counter as the "zero" point so the first elapsed_ns() call returns ~0.
 *  Returns false on failure. */
bool platform_timer_init(PlatformTimer *t);

/** Return the number of nanoseconds elapsed since the previous call to this
 *  function (or since platform_timer_init) for @p t.
 *
 *  This function updates t->last_raw internally so the next call measures
 *  the delta from now — it is the caller's responsibility to accumulate
 *  deltas if they need total elapsed time.
 *
 *  Returns 0 on failure (never, for vDSO Linux; unlikely for macOS/Win). */
uint64_t platform_timer_elapsed_ns(PlatformTimer *t, uint64_t *total);

/* ── Console input helper ───────────────────────────────────────────── */

/** Wait for and return a single key press.
 *  @param wait   true = block until a key is pressed; false = return immediately.
 *  On Windows this calls _getch(); on POSIX it temporarily disables
 *  canonical mode and echo so the user does not need to press Enter.
 *  Returns the character (as unsigned char), or EOF on error/end-of-input. */
int platform_wait_key(bool wait);

/* ── Threads, mutexes, and sleep ────────────────────────────────────── */

typedef void (*platform_thread_fn_t)(void *arg);

#if !defined(_WIN32) && !defined(_WIN64)
#include <pthread.h>
#endif

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
  void *handle; /* HANDLE */
#else
  pthread_t tid;
#endif
} platform_thread_t;

/** Create a new thread.  Returns true on success. */
bool platform_thread_create(platform_thread_t *t, platform_thread_fn_t fn,
                            void *arg);

/** Block until the thread exits. */
void platform_thread_join(platform_thread_t *t);

typedef struct {
#if defined(_WIN32) || defined(_WIN64)
  void *cs; /* CRITICAL_SECTION * */
#else
  void *mtx; /* pthread_mutex_t * */
#endif
} platform_mutex_t;

/** Initialise a mutex.  Returns true on success. */
bool platform_mutex_init(platform_mutex_t *m);

void platform_mutex_lock(platform_mutex_t *m);
void platform_mutex_unlock(platform_mutex_t *m);
void platform_mutex_destroy(platform_mutex_t *m);

/** Sleep for at least @p ms milliseconds. */
void platform_sleep_ms(uint32_t ms);

#ifdef __APPLE__
#define BSS_SECTION
#else
#define BSS_SECTION __attribute__((section(".bss")))
#endif

#endif /* PLATFORM_H */