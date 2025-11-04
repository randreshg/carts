#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

// Simple example demonstrating array section/chunk dependencies
// using the syntax: depend(in : A[start : length][start : length])

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s N BLOCK_SIZE\n", argv[0]);
    printf("  N: Grid size (NxN)\n");
    printf("  BLOCK_SIZE: Size of each chunk\n");
    return 1;
  }

  const int N = atoi(argv[1]);
  const int BLOCK_SIZE = atoi(argv[2]);

  if (N < 2 || BLOCK_SIZE < 1 || N % BLOCK_SIZE != 0) {
    printf("Error: N must be divisible by BLOCK_SIZE\n");
    return 1;
  }

  // Allocate 2D arrays
  double **A = (double **)malloc(N * sizeof(double *));
  double **B = (double **)malloc(N * sizeof(double *));
  for (int i = 0; i < N; i++) {
    A[i] = (double *)malloc(N * sizeof(double));
    B[i] = (double *)malloc(N * sizeof(double));
  }

  printf("Array Chunk Dependencies Test\n");
  printf("Grid size: %dx%d, Block size: %d\n", N, N, BLOCK_SIZE);
  printf("Number of blocks: %dx%d = %d\n\n", N / BLOCK_SIZE, N / BLOCK_SIZE,
         (N / BLOCK_SIZE) * (N / BLOCK_SIZE));

#pragma omp parallel
  {
#pragma omp single
    {
      // Phase 1: Initialize array A in chunks
      // Each task writes to a chunk A[i:BLOCK_SIZE][j:BLOCK_SIZE]
      printf("Phase 1: Initializing array A in chunks...\n");
      for (int i = 0; i < N; i += BLOCK_SIZE) {
        for (int j = 0; j < N; j += BLOCK_SIZE) {
// Task outputs a chunk of A
#pragma omp task firstprivate(i, j) depend(out : A[i : BLOCK_SIZE][j : BLOCK_SIZE])
          {
            printf("  Task Init[%d:%d][%d:%d]: Writing block\n", i,
                   i + BLOCK_SIZE, j, j + BLOCK_SIZE);
            for (int ii = i; ii < i + BLOCK_SIZE; ii++) {
              for (int jj = j; jj < j + BLOCK_SIZE; jj++) {
                A[ii][jj] = (double)(ii * N + jj);
              }
            }
          }
        }
      }

      // Phase 2: Process chunks with dependencies
      // Each B chunk depends on corresponding A chunk plus neighbor chunks
      printf("\nPhase 2: Computing array B from A chunks...\n");
      for (int i = 0; i < N; i += BLOCK_SIZE) {
        for (int j = 0; j < N; j += BLOCK_SIZE) {
          // Determine if we have neighbors
          int has_left = (j > 0);
          int has_right = (j + BLOCK_SIZE < N);
          int has_up = (i > 0);
          int has_down = (i + BLOCK_SIZE < N);

          if (!has_left && !has_right && !has_up && !has_down) {
            // Corner case: single block, only depends on itself
#pragma omp task firstprivate(i, j)                                           \
    depend(in : A[i : BLOCK_SIZE][j : BLOCK_SIZE])                            \
    depend(out : B[i : BLOCK_SIZE][j : BLOCK_SIZE])
            {
              printf("  Task Compute[%d:%d][%d:%d]: center only\n", i,
                     i + BLOCK_SIZE, j, j + BLOCK_SIZE);
              for (int ii = i; ii < i + BLOCK_SIZE; ii++) {
                for (int jj = j; jj < j + BLOCK_SIZE; jj++) {
                  B[ii][jj] = A[ii][jj] * 2.0;
                }
              }
            }
          } else if (has_left && has_right && has_up && has_down) {
            // Interior block: depends on center + 4 neighbors
#pragma omp task firstprivate(i, j)                                           \
    depend(in : A[i : BLOCK_SIZE][j : BLOCK_SIZE],                            \
               A[i - BLOCK_SIZE : BLOCK_SIZE][j : BLOCK_SIZE],                \
               A[i + BLOCK_SIZE : BLOCK_SIZE][j : BLOCK_SIZE],                \
               A[i : BLOCK_SIZE][j - BLOCK_SIZE : BLOCK_SIZE],                \
               A[i : BLOCK_SIZE][j + BLOCK_SIZE : BLOCK_SIZE])                \
    depend(out : B[i : BLOCK_SIZE][j : BLOCK_SIZE])
            {
              printf("  Task Compute[%d:%d][%d:%d]: center + 4 neighbors\n", i,
                     i + BLOCK_SIZE, j, j + BLOCK_SIZE);
              for (int ii = i; ii < i + BLOCK_SIZE; ii++) {
                for (int jj = j; jj < j + BLOCK_SIZE; jj++) {
                  // Average of center and available neighbors
                  B[ii][jj] = A[ii][jj];
                  int count = 1;
                  if (ii > 0) {
                    B[ii][jj] += A[ii - 1][jj];
                    count++;
                  }
                  if (ii < N - 1) {
                    B[ii][jj] += A[ii + 1][jj];
                    count++;
                  }
                  if (jj > 0) {
                    B[ii][jj] += A[ii][jj - 1];
                    count++;
                  }
                  if (jj < N - 1) {
                    B[ii][jj] += A[ii][jj + 1];
                    count++;
                  }
                  B[ii][jj] /= count;
                }
              }
            }
          } else {
            // Edge blocks: partial neighbors
            // For simplicity, just copy
#pragma omp task firstprivate(i, j)                                           \
    depend(in : A[i : BLOCK_SIZE][j : BLOCK_SIZE])                            \
    depend(out : B[i : BLOCK_SIZE][j : BLOCK_SIZE])
            {
              printf("  Task Compute[%d:%d][%d:%d]: edge block\n", i,
                     i + BLOCK_SIZE, j, j + BLOCK_SIZE);
              for (int ii = i; ii < i + BLOCK_SIZE; ii++) {
                for (int jj = j; jj < j + BLOCK_SIZE; jj++) {
                  B[ii][jj] = A[ii][jj];
                }
              }
            }
          }
        }
      }

      // Phase 3: Verification task that reads all chunks
      printf("\nPhase 3: Verification...\n");
#pragma omp task depend(in : B[0 : N][0 : N])
      {
        printf("  Verification task: Reading entire B array\n");
        int errors = 0;
        for (int i = 0; i < N; i++) {
          for (int j = 0; j < N; j++) {
            if (B[i][j] < 0) {
              errors++;
            }
          }
        }

        if (errors == 0) {
          printf("\nVerification: PASSED\n");
          printf("Sample values:\n");
          printf("  A[0][0] = %.2f, B[0][0] = %.2f\n", A[0][0], B[0][0]);
          printf("  A[%d][%d] = %.2f, B[%d][%d] = %.2f\n", N - 1, N - 1,
                 A[N - 1][N - 1], N - 1, N - 1, B[N - 1][N - 1]);
        } else {
          printf("\nVerification: FAILED (%d errors)\n", errors);
        }
      }
    } // End single
  }   // End parallel

  // Cleanup
  for (int i = 0; i < N; i++) {
    free(A[i]);
    free(B[i]);
  }
  free(A);
  free(B);

  return 0;
}
