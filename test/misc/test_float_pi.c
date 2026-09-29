#include <dev/seven_segment_display.h>
#include <dev/syscall.h>
#include <stdio.h>

// Euler–Wallis iterative method
int main(int argc, char *argv[]) {
  double pi = 2.0;
  int i;
  int iterations = 1000;

  for (i = 1; i <= iterations; i++) {
    double numerator = 4.0 * i * i;
    double denominator = numerator - 1;
    pi *= numerator / denominator;

#if 1
    if ((i & 0x7) == 0)
      printf("i = %4d, pi = %u\n", i, (uint32_t)(pi * 1000000));
#else
    tcm_seven_segment_display_upper_decimal(i);
    if ((i & 0x7) == 0)
      tcm_seven_segment_display_lower_decimal(pi * 10000000);
#endif
  }
  return 0;
}
