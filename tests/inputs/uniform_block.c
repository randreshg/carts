/*
 * Uniform block-distributed write pattern.
 * Simple parallel for that writes to contiguous array elements.
 * Used as test input for partitioning and pipeline contract tests.
 */
#include <stdlib.h>

int main() {
  int N = 1024;
  double *A = (double *)malloc(N * sizeof(double));

#pragma omp parallel for
  for (int i = 0; i < N; i++) {
    A[i] = (double)i * 2.0;
  }

  double sum = 0.0;
  for (int i = 0; i < N; i++) {
    sum += A[i];
  }

  free(A);
  return (int)sum;
}
