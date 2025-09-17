#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  int N = atoi(argv[1]);
  int *a = (int *)malloc(N * sizeof(int));
  int *b = (int *)malloc(N * sizeof(int));
  int *c = (int *)malloc(N * sizeof(int));

  // Initialize arrays
  for (int i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
  }

/// Parallel region with worksharing loop - vector addition
#pragma omp parallel
  {
#pragma omp for
    for (int i = 0; i < N; i++) {
      c[i] = a[i] + b[i];
    }
  }

  /// Verify results
  int sum = 0;
#pragma omp parallel
  {
#pragma omp for reduction(+ : sum)
    for (int i = 0; i < N; i++) {
      sum += c[i];
    }
  }

  printf("Sum of results: %d\n", sum);
  printf("Expected sum: %d\n", N * (N - 1) / 2 + N * (N - 1));

  free(a);
  free(b);
  free(c);

  return 0;
}
