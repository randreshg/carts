/*
 * CARTS N-D Block Partitioning Test
 * Demonstrates 2D block partitioning with collapse(2) for N-dimensional
 * support.
 *
 * This test verifies that:
 * 1. 2D arrays can be partitioned into blocks along both dimensions
 * 2. partition_indices correctly capture both i and j indices
 * 3. DbPartitioning handles N-D block sizes properly
 *
 * Access pattern: 2D blocked iteration with BLOCK_I x BLOCK_J tiles
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "arts/Utils/Testing/CartsTest.h"

/// Grid and block sizes
#define N 64
#define M 64
#define BLOCK_I 8
#define BLOCK_J 8

/// Sequential reference implementation
static void compute_seq(double A[N][M], double B[N][M]) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < M; j++) {
      B[i][j] = A[i][j] * 2.0 + (double)(i + j);
    }
  }
}

/// Parallel implementation with 2D block partitioning
static void compute_parallel(double A[N][M], double B[N][M]) {
  int i, j;

  /// 2D block partitioning: blocks of BLOCK_I x BLOCK_J
  /// This creates partition_indices = [%i, %j] for 2D partitioning
#pragma omp parallel for collapse(2) schedule(static, 1)
  for (i = 0; i < N; i += BLOCK_I) {
    for (j = 0; j < M; j += BLOCK_J) {
      /// Process a BLOCK_I x BLOCK_J tile
      for (int bi = 0; bi < BLOCK_I && (i + bi) < N; bi++) {
        for (int bj = 0; bj < BLOCK_J && (j + bj) < M; bj++) {
          int ii = i + bi;
          int jj = j + bj;
          B[ii][jj] = A[ii][jj] * 2.0 + (double)(ii + jj);
        }
      }
    }
  }
}

/// Checksum function
static double checksum(double B[N][M]) {
  double sum = 0.0;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < M; j++) {
      sum += B[i][j];
    }
  }
  return sum;
}

int main(void) {
  CARTS_TIMER_START();

  printf("N-D Block Test: %d x %d grid, %dx%d blocks\n", N, M, BLOCK_I,
         BLOCK_J);
  printf("Testing 2D block partitioning with collapse(2)\n");

  /// Allocate 2D arrays
  static double A[N][M];
  static double B_seq[N][M];
  static double B_par[N][M];

  /// Initialize array A
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < M; j++) {
      A[i][j] = (double)(i * M + j);
      B_seq[i][j] = 0.0;
      B_par[i][j] = 0.0;
    }
  }

  /// Run sequential version
  compute_seq(A, B_seq);
  double seq_sum = checksum(B_seq);

  /// Run parallel version
  compute_parallel(A, B_par);
  double par_sum = checksum(B_par);

  CARTS_TIMER_END();

  printf("Sequential checksum: %.2f\n", seq_sum);
  printf("Parallel checksum:   %.2f\n", par_sum);

  /// Verify results
  double tolerance = 1e-6;
  int errors = 0;
  for (int i = 0; i < N && errors < 10; i++) {
    for (int j = 0; j < M && errors < 10; j++) {
      if (fabs(B_seq[i][j] - B_par[i][j]) > tolerance) {
        printf("Mismatch at [%d][%d]: seq=%.2f, par=%.2f\n", i, j, B_seq[i][j],
               B_par[i][j]);
        errors++;
      }
    }
  }

  if (errors == 0 && fabs(seq_sum - par_sum) < tolerance) {
    CARTS_TEST_PASS();
    return 0;
  } else {
    printf("Verification failed: %d mismatches\n", errors);
    CARTS_TEST_FAIL();
    return 1;
  }
}
