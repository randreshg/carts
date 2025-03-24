#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define N 10

int main(int argc, char *argv[]) {
  double A[N][N], B[N][N];
  srand(time(NULL));
  const int random = rand() % 101;

  #pragma omp parallel
  {
    #pragma omp single
    {
      /// Compute the A matrix
      for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
          /// Each task computes one A[i][j].
          #pragma omp task firstprivate(i, j) depend(out : A[i][j])
          {
            A[i][j] = i * 1.0 + random;
          }
        }
      }
      
      /// Compute the B matrix using A.
      /// For row zero, B[0][j] only depends on A[0][j]
      for (int j = 0; j < N; j++) {
        #pragma omp task firstprivate(j) depend(in : A[0][j]) depend(out : B[0][j])
        {
          B[0][j] = A[0][j];
        }
      }
      
      /// For rows 1..N-1, B[i][j] depends on A[i][j] and A[i-1][j].
      for (int i = 1; i < N; i++) {
        for (int j = 0; j < N; j++) {
          #pragma omp task firstprivate(i, j) depend(in : A[i][j], A[i-1][j]) depend(out : B[i][j])
          {
            B[i][j] = A[i][j] + A[i-1][j];
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
  
  /// Verification of the result.
  int correct = 1;
  for (int j = 0; j < N; j++) {
    if (B[0][j] != A[0][j]) {
      correct = 0;
    }
  }
  for (int i = 1; i < N; i++) {
    for (int j = 0; j < N; j++) {
      if (B[i][j] != A[i][j] + A[i-1][j]) {
        correct = 0;
      }
    }
  }
  if (correct) {
    printf("\nVerification: PASSED\n");
  } else {
    printf("\nVerification: FAILED\n");
  }
  
  return 0;
}
