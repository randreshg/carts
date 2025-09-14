#include <stdio.h>
#include <stdlib.h>
#define N 10

int main() {
  double A[N][N], B[N][N];
#pragma omp parallel
  {
#pragma omp single
    {
      /// Compute each row of matrix A in a separate task.
      for (int i = 0; i < N; i++) {
// #pragma omp task firstprivate(i) depend(out: A[i])
#pragma omp task firstprivate(i)
        {
          for (int j = 0; j < N; j++) {
            A[i][j] = i + j;
          }
        }
      }

/// Compute row 0 of B using the entire row A[0].
// #pragma omp task depend(in: A[0]) depend(out: B[0])
#pragma omp task
      {
        for (int j = 0; j < N; j++) {
          B[0][j] = A[0][j];
        }
      }

      /// Compute rows 1..N-1 of B.
      /// Each B row i depends on both A row i and A row i-1.
      for (int i = 1; i < N; i++) {
// #pragma omp task firstprivate(i) depend(in: A[i], A[i-1]) depend(out: B[i])
#pragma omp task firstprivate(i)
        {
          for (int j = 0; j < N; j++) {
            B[i][j] = A[i][j] + A[i - 1][j];
          }
        }
      }
    }
  }

  /// Print the computed matrices
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

  /// Verification check for matrix B.
  int error = 0;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      double expected;
      if (i == 0) {
        expected = i + j;
      } else {
        expected = (i + j) + ((i - 1) + j);
      }
      if (B[i][j] != expected) {
        printf("Verification failed at B[%d][%d]: got %f, expected %f.\n", i, j,
               B[i][j], expected);
        error++;
      }
    }
  }
  if (error == 0) {
    printf("Verification succeeded: All B elements are correct.\n");
  } else {
    printf("Verification encountered %d error(s).\n", error);
  }

  return 0;
}
