///==========================================================================///
/// GPU SAXPY Example
///
/// SAXPY: Y[i] = a * X[i] + Y[i] (Single-precision A*X Plus Y)
/// Classic BLAS Level 1 operation, ideal for GPU execution.
///
/// Build: carts execute --gpu saxpy.c -o saxpy
/// Run:   ./saxpy [N]
///==========================================================================///

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "arts/Utils/Testing/CartsTest.h"

#define DEFAULT_N 1048576  // 1M elements

int main(int argc, char **argv) {
  CARTS_TIMER_START();

  const int N = (argc >= 2) ? atoi(argv[1]) : DEFAULT_N;
  const float alpha = 2.5f;

  // Allocate arrays
  float *X = (float *)malloc(N * sizeof(float));
  float *Y = (float *)malloc(N * sizeof(float));
  float *Y_expected = (float *)malloc(N * sizeof(float));

  if (!X || !Y || !Y_expected) {
    CARTS_TEST_FAIL("Memory allocation failed");
  }

  // Initialize arrays
  for (int i = 0; i < N; i++) {
    X[i] = (float)i * 0.1f;
    Y[i] = (float)i * 0.2f;
    Y_expected[i] = alpha * X[i] + Y[i];
  }

  CARTS_DEBUG_PRINT("Initialized %d elements with alpha=%f\n", N, alpha);

  // SAXPY operation - GPU eligible loop
  // This is a classic GPU kernel pattern:
  // - High arithmetic intensity (2 FLOPs per element)
  // - Coalesced memory access (stride-1)
  // - No loop-carried dependencies
  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    Y[i] = alpha * X[i] + Y[i];
  }

  // Verify results
  int success = 1;
  float max_error = 0.0f;

  for (int i = 0; i < N; i++) {
    float error = fabsf(Y[i] - Y_expected[i]);
    if (error > max_error) {
      max_error = error;
    }
    // Relative error check for floating point
    float rel_error = (Y_expected[i] != 0.0f) ?
                      fabsf(error / Y_expected[i]) : error;
    if (rel_error > 1e-5f && error > 1e-5f) {
      CARTS_DEBUG_PRINT("Mismatch at i=%d: expected %f, got %f (error=%e)\n",
                        i, Y_expected[i], Y[i], error);
      success = 0;
      break;
    }
  }

  CARTS_DEBUG_PRINT("Max error: %e\n", max_error);

  // Cleanup
  free(X);
  free(Y);
  free(Y_expected);

  if (success)
    CARTS_TEST_PASS();
  else
    CARTS_TEST_FAIL("SAXPY computation mismatch");
}
