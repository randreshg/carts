#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if(argc < 2) {
    printf("Usage: %s <matrix_size>\n", argv[0]);
    return EXIT_FAILURE;
  }
  const int N = atoi(argv[1]);

  /// Allocate matrices
  double **A = (double **)malloc(N * sizeof(double *));
  double **B = (double **)malloc(N * sizeof(double *));
  for (int i = 0; i < N; i++) {
    A[i] = (double *)malloc(N * sizeof(double));
    B[i] = (double *)malloc(N * sizeof(double));
  }

  /// Parallel region
#pragma omp parallel
  {
#pragma omp single
    {
      /// Initialize matrix A
      /// Compute each row of matrix A in a separate task.
      for (int i = 0; i < N; i++) {
#pragma omp task firstprivate(i) depend(out : A[i])
        {
          for (int j = 0; j < N; j++) {
            A[i][j] = i + j;
          }
        }
      }

/// Compute row 0 of B using the entire row A[0].
#pragma omp task depend(in : A[0]) depend(out : B[0])
      {
        for (int j = 0; j < N; j++) {
          B[0][j] = A[0][j];
        }
      }

      /// Compute rows 1..N-1 of B.
      /// Each B row i depends on both A row i and A row i-1.
      for (int i = 1; i < N; i++) {
#pragma omp task firstprivate(i) depend(in : A[i], A[i - 1]) depend(out : B[i])
        {
          for (int j = 0; j < N; j++) {
            B[i][j] = A[i][j] + A[i - 1][j];
          }
        }
      }
    }
  }


  /// Verification check for matrix B.
  int error = 0;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      double expected;
      if (i == 0)
        expected = i + j;
      else
        expected = (i + j) + ((i - 1) + j);

      if (B[i][j] != expected) {
        printf("Verification failed at B[%d][%d]: got %f, expected %f.\n", i, j,
               B[i][j], expected);
        error++;
      }
    }
  }

  /// Clean up
  for (int i = 0; i < N; i++) {
    free(A[i]);
    free(B[i]);
  }
  free(A);
  free(B);

  /// Print the verification result
  if (error == 0)
    printf("Verification succeeded: All B elements are correct.\n");
  else
    printf("Verification encountered %d error(s).\n", error);

  return 0;
}
