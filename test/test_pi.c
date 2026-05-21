#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  long *pi, *t, m, n, r, s;
  int t0[][3] = {48, 32, 20, 24, 8, 4}, k0[][3] = {1, 1, 0, 1, 1, 1};
  int n0[][3] = {18, 57, 239, 8, 57, 239}, d, i, j, k, p, q;

  d = (argc > 1) ? (((i = atoi(argv[1])) < 0) ? 0 : i) : 100;
  q = (argc > 2) ? 1 : 0;
  printf("%s\n\n", "Nature (R) Pi value compute Program  (C) Tue 1999.11.30");
  printf(
      "pi= %s%d * arctg(1/%d) %s %d * arctg(1/%d) %s %d * arctg(1/%d) [%s]\n",
      k0[q][0] ? "" : "-", t0[q][0], n0[q][0], k0[q][1] ? "+" : "-", t0[q][1],
      n0[q][1], k0[q][2] ? "+" : "-", t0[q][2], n0[q][2],
      q ? "Stomer" : "Gauss");
  if ((t = (long *)calloc((d += 5) + 1, sizeof(long))) == NULL)
    return 1;
  if ((pi = (long *)calloc(d + 1, sizeof(long))) == NULL)
    return 2;
  for (i = d; i >= 0; i--)
    pi[i] = 0;
  for (p = 0; p < 3; p++) {
    for (k = k0[q][p], n = n0[q][p], t[i = j = d] = t0[q][p], i--; i >= 0; i--)
      t[i] = 0;
    for (r = 0, i = j; i >= 0; i--) {
      r = (m = 10 * r + t[i]) % n;
      t[i] = m / n;
      k ? (pi[i] += t[i]) : (pi[i] -= t[i]);
    }
    while (j > 0 && t[j] == 0)
      j--;
    for (k = !k, s = 3, n *= n; j > 0; k = !k, s += 2) {
      for (r = 0, i = j; i >= 0; i--) {
        r = (m = 10 * r + t[i]) % n;
        t[i] = m / n;
      }
      while (j > 0 && t[j] == 0)
        j--;
      for (r = 0, i = j; i >= 0; i--) {
        r = (m = 10 * r + t[i]) % s;
        m /= s;
        k ? (pi[i] += m) : (pi[i] -= m);
      }
    }
  }
  for (n = i = 0; i <= d; pi[i++] = r) {
    n = (m = pi[i] + n) / 10;
    if ((r = m % 10) < 0)
      r += 10, n--;
  }
  printf("pi= %ld.", pi[d]);
  for (i = d - 1; i >= 5; i--)
    printf("%ld%s", pi[i],
           ((m = d - i + 5) % 65) ? ((m % 5) ? "" : " ") : "\n");
  printf("%sDIGITS: %d\n", (m % 65) ? "\n" : "", d - 5);
  return 0;
}