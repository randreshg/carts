///==========================================================================///
/// GPU Vector Addition Example
///
/// Simple vector addition: C[i] = A[i] + B[i]
/// This example demonstrates GPU-eligible parallel loops in CARTS.
///
/// Build: carts execute --gpu vector_add.c -o vector_add
/// Run:   ./vector_add [N]
///==========================================================================///

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "arts/Utils/Testing/CartsTest.h"

#define DEFAULT_N 1048576  // 1M elements

int main(int argc, char **argv) {
  CARTS_TIMER_START();

  const int N = (argc >= 2) ? atoi(argv[1]) : DEFAULT_N;

  // Allocate arrays
  float *A = (float *)malloc(N * sizeof(float));
  float *B = (float *)malloc(N * sizeof(float));
  float *C = (float *)malloc(N * sizeof(float));

  if (!A || !B || !C) {
    CARTS_TEST_FAIL("Memory allocation failed");
  }

  // Initialize arrays
  for (int i = 0; i < N; i++) {
    A[i] = (float)i * 0.5f;
    B[i] = (float)i * 1.5f;
    C[i] = 0.0f;
  }

  CARTS_DEBUG_PRINT("Initialized %d elements\n", N);

  // Parallel vector addition - GPU eligible loop
  // This loop meets GPU profitability thresholds:
  // - Trip count >= gpuMinIterations (1M >> 256)
  // - Data size >= gpuMinDataBytes (12MB >> 1KB)
  // - Embarrassingly parallel (no dependencies)
  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    C[i] = A[i] + B[i];
  }

  // Verify results
  int success = 1;
  float max_error = 0.0f;

  for (int i = 0; i < N; i++) {
    float expected = A[i] + B[i];
    float error = fabsf(C[i] - expected);
    if (error > max_error) {
      max_error = error;
    }
    if (error > 1e-5f) {
      CARTS_DEBUG_PRINT("Mismatch at i=%d: expected %f, got %f\n",
                        i, expected, C[i]);
      success = 0;
      break;
    }
  }

  CARTS_DEBUG_PRINT("Max error: %e\n", max_error);

  // Cleanup
  free(A);
  free(B);
  free(C);

  if (success)
    CARTS_TEST_PASS();
  else
    CARTS_TEST_FAIL("Vector addition mismatch");
}
