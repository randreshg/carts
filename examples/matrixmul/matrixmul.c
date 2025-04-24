#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

// #define N 10
// #define BS 5
#define TOLERANCE 1e-6

int main(int argc, char *argv[]) {
  // Check for the correct number of arguments
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <matrix_size> <block_size>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const int N = atoi(argv[1]);
  const int BS = atoi(argv[2]);
  if (N <= 0 || BS <= 0) {
    fprintf(stderr, "Matrix size and block size must be positive integers.\n");
    return EXIT_FAILURE;
  }

  float A[N][N], B[N][N], C_parallel[N][N], C_sequential[N][N];

  // Initialize matrices
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i][j] = (float)(i + j);
      B[i][j] = (float)(i - j);
      C_parallel[i][j] = 0.0f;
      C_sequential[i][j] = 0.0f;
    }
  }

  // Parallel computation
#pragma omp parallel
#pragma omp single
  {
    for (int i = 0; i < N; i += BS) {
      for (int j = 0; j < N; j += BS) {
        for (int k = 0; k < N; k += BS) {
          #pragma omp task
          {
            for (int ii = i; ii < i + BS; ii++)
              for (int jj = j; jj < j + BS; jj++)
                for (int kk = k; kk < k + BS; kk++) {
                  C_parallel[ii][jj] += A[ii][kk] * B[kk][jj];
                }
          }
        }
      }
    }
  }

  // Sequential computation
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      float sum = 0.0f;
      for (int k = 0; k < N; k++) {
        sum += A[i][k] * B[k][j];
      }
      C_sequential[i][j] = sum;
    }
  }

  // Verification
  int verified = 1;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      if (fabs(C_parallel[i][j] - C_sequential[i][j]) > TOLERANCE) {
        printf("Mismatch at (%d,%d): Parallel=%.2f, Sequential=%.2f\n", i, j,
               C_parallel[i][j], C_sequential[i][j]);
        verified = 0;
      }
    }
  }

  // Print matrices
  printf("Matrix A:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%6.2f ", A[i][j]);
    }
    printf("\n");
  }

  printf("\nMatrix B:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%6.2f ", B[i][j]);
    }
    printf("\n");
  }

  printf("\nMatrix C (Parallel):\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%6.2f ", C_parallel[i][j]);
    }
    printf("\n");
  }

  printf("\nMatrix C (Sequential):\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%6.2f ", C_sequential[i][j]);
    }
    printf("\n");
  }
  printf("\n");

  printf("----------------------\n");
  printf("%s\n", verified ? "Verification PASSED" : "Verification FAILED");
  printf("----------------------\n");

  return 0;
}