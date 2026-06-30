//
// Created by zhangjiantao on 26-5-8.
//


#define _POSIX_MONOTONIC_CLOCK

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* Embedded Test Configuration */
#define TEST_DURATION_SEC 10
#define SAMPLE_INTERVAL_MS 1000
#define MAX_ALLOWED_DRIFT_MS 50

static long long timespec_to_ms(const struct timespec *ts) {
  return (long long)ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
}

int
#ifdef TESTALL
test_stdlib_time(int argc, char**argv)
#else
main(void)
#endif
{
  time_t wall_start, wall_now, prev_wall;
  struct tm start_tm;
#ifdef CLOCK_MONOTONIC
  struct timespec mono_start, mono_now;
#endif

  time(&wall_start);
  prev_wall = wall_start;
  localtime_r(&wall_start, &start_tm);

#ifdef CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &mono_start);
#endif

  bool time_backward = false;
  bool time_stuck = false;
  bool drift_over = false;

  printf("=============================================\n");
  printf("   Embedded Libc Time Accuracy Test\n");
  printf("=============================================\n");
  printf("Start Time: %04d-%02d-%02d %02d:%02d:%02d\n", start_tm.tm_year + 1900,
         start_tm.tm_mon + 1, start_tm.tm_mday, start_tm.tm_hour,
         start_tm.tm_min, start_tm.tm_sec);
#ifdef CLOCK_MONOTONIC
  printf("Monotonic clock: SUPPORTED\n");
#else
  printf("Monotonic clock: NOT SUPPORTED\n");
#endif
  printf("=============================================\n\n");

  int total_rounds = (TEST_DURATION_SEC * 1000) / SAMPLE_INTERVAL_MS;

  for (int i = 0; i < total_rounds; i++) {
    usleep(SAMPLE_INTERVAL_MS * 1000);
    time(&wall_now);

    if (wall_now < prev_wall) {
      printf("[ERROR] Time jumped backward!\n");
      time_backward = true;
    }
    if (wall_now == prev_wall && i > 0) {
      time_stuck = true;
    }

#ifdef CLOCK_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, &mono_now);
    long long wall_elapsed = (long long)(wall_now - wall_start) * 1000;
    long long mono_elapsed =
        timespec_to_ms(&mono_now) - timespec_to_ms(&mono_start);
    long long drift = llabs(wall_elapsed - mono_elapsed);

    if (drift > MAX_ALLOWED_DRIFT_MS)
      drift_over = true;

    printf("[Sample] Drift: %3lld ms | Time: %s", drift, ctime(&wall_now));
#else
    printf("[Sample] System time: %s", ctime(&wall_now));
#endif

    prev_wall = wall_now;
  }

  printf("\n=============================================\n");
  printf("Test Result:\n");
  if (time_backward)
    printf("FAIL: Time jumped backward\n");
  else if (time_stuck)
    printf("FAIL: Time is stuck\n");
  else
    printf("PASS: System time is stable\n");
  printf("=============================================\n");

  return 0;
}
