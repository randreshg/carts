#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <size>\n", argv[0]);
    return 1;
  }

  int N = atoi(argv[1]);
  int *a = (int *)malloc(N * sizeof(int));
  int *b = (int *)malloc(N * sizeof(int));
  int *c = (int *)malloc(N * sizeof(int));
  int *d = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
    c[i] = 0;
    d[i] = 0;
  }
  /// Single parallel region with back-to-back for loops
#pragma omp parallel
  {
#pragma omp for schedule(static, 4)
    for (int i = 0; i < N; i++)
      c[i] = a[i] + b[i];

#pragma omp for schedule(static, 4)
    for (int i = 0; i < N; i++)
      d[i] = c[i] * 2;
  }

  int success = 1;
  for (int i = 0; i < N; i++) {
    if (c[i] != a[i] + b[i] || d[i] != 2 * (a[i] + b[i])) {
      success = 0;
      break;
    }
  }

  printf("Success: %s\n", success ? "true" : "false");

  free(a);
  free(b);
  free(c);
  free(d);
  return 0;
}
