/* Ping-pong stencil regression with a full-array sequential oracle. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "carts/utils/testing/CartsTest.h"

#define N 128
#define TSTEPS 8

static float **alloc2d(void) {
  float **a = (float **)malloc(N * sizeof(float *));
  for (int i = 0; i < N; i++)
    a[i] = (float *)malloc(N * sizeof(float));
  return a;
}

static void init(float **A, float **B) {
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
      A[i][j] = (float)((i + j) % 256) * 0.001f;
      B[i][j] = 0.0f;
    }
}

int main(void) {
  CARTS_TIMER_START();
  float **A = alloc2d(), **B = alloc2d();
  float **As = alloc2d(), **Bs = alloc2d();

  /* Reference first so the kernel buffers cannot affect the oracle. */
  init(As, Bs);
  for (int t = 0; t < TSTEPS; t += 2) {
    for (int i = 1; i < N - 1; i++)
      for (int j = 1; j < N - 1; j++)
        Bs[i][j] = 0.2f * (As[i][j] + As[i - 1][j] + As[i + 1][j] +
                           As[i][j - 1] + As[i][j + 1]);
    if (t + 1 < TSTEPS)
      for (int i = 1; i < N - 1; i++)
        for (int j = 1; j < N - 1; j++)
          As[i][j] = 0.2f * (Bs[i][j] + Bs[i - 1][j] + Bs[i + 1][j] +
                             Bs[i][j - 1] + Bs[i][j + 1]);
  }
  double ref = 0.0;
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      ref += As[i][j];

  /* Distributed kernel. */
  init(A, B);
  for (int t = 0; t < TSTEPS; t += 2) {
#pragma omp parallel for schedule(static)
    for (int i = 1; i < N - 1; i++)
      for (int j = 1; j < N - 1; j++)
        B[i][j] = 0.2f * (A[i][j] + A[i - 1][j] + A[i + 1][j] + A[i][j - 1] +
                          A[i][j + 1]);
    if (t + 1 < TSTEPS)
#pragma omp parallel for schedule(static)
      for (int i = 1; i < N - 1; i++)
        for (int j = 1; j < N - 1; j++)
          A[i][j] = 0.2f * (B[i][j] + B[i - 1][j] + B[i + 1][j] + B[i][j - 1] +
                            B[i][j + 1]);
  }
  double got = 0.0;
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      got += A[i][j];

  printf("jacobi2d two-sweep: got=%.6f ref=%.6f\n", got, ref);
  if (fabs(got - ref) <= 1e-3 * (fabs(ref) + 1.0))
    CARTS_TEST_PASS();
  else
    CARTS_TEST_FAIL("distributed two-sweep stencil diverged from reference");
}
