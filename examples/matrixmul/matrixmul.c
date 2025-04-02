
/// cgeist matrixmul.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > matrixmul.mlir
/// carts-opt matrixmul.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --create-epochs --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize  -debug-only=convert-arts-to-llvm -o matrixmul.ll


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define N 4
// Block size (must divide N)
#define BS 2
#define TOLERANCE 1e-6

void matmul_depend(float A[N][N], float B[N][N], float C[N][N]) {
  int i, j, k, ii, jj, kk;
  for (i = 0; i < N; i += BS) {
    for (j = 0; j < N; j += BS) {
      for (k = 0; k < N; k += BS) {
         #pragma omp task private(ii, jj, kk)                  \
            depend(in : A[i : BS][k : BS], B[k : BS][j : BS])  \
            depend(inout : C[i : BS][j : BS])
         {
            for (ii = i; ii < i + BS; ii++)
               for (jj = j; jj < j + BS; jj++)
                  for (kk = k; kk < k + BS; kk++)
                     C[ii][jj] += A[ii][kk] * B[kk][jj];
            
         }
        
      }
    }
  }
}

void matmul_sequential(float A[N][N], float B[N][N], float C[N][N]) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      float sum = 0.0f;
      for (int k = 0; k < N; k++) {
        sum += A[i][k] * B[k][j];
      }
      C[i][j] = sum;
    }
  }
}

void print_matrix(const char *name, float mat[N][N]) {
  printf("%s:\n", name);
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%6.2f ", mat[i][j]);
    }
    printf("\n");
  }
  printf("\n");
}

int verify(float par[N][N], float seq[N][N]) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      if (fabs(par[i][j] - seq[i][j]) > TOLERANCE) {
        printf("Mismatch at (%d,%d): Parallel=%.2f, Sequential=%.2f\n", i, j,
               par[i][j], seq[i][j]);
        return 0;
      }
    }
  }
  return 1;
}

int main() {
  float A[N][N], B[N][N], C_parallel[N][N], C_sequential[N][N];

  // Initialize matrices with sample values
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i][j] = (float)(i + j);
      B[i][j] = (float)(i - j);
    }
  }

// Parallel computation
#pragma omp parallel
#pragma omp single
  matmul_depend(A, B, C_parallel);

  // Sequential computation
  matmul_sequential(A, B, C_sequential);

  // Verification
  if (verify(C_parallel, C_sequential)) {
    printf("Verification PASSED\n");
  } else {
    printf("Verification FAILED\n");
  }

  // Print matrices
  print_matrix("Matrix A", A);
  print_matrix("Matrix B", B);
  print_matrix("Parallel Result", C_parallel);
  print_matrix("Sequential Result", C_sequential);

  return 0;
}