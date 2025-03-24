// Generate standard MLIR dialect
/// cgeist matrix.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > matrix.mlir

// Passes
/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis,create-events,convert-arts-to-llvm,arts-codegen &> matrix-arts.mlir

/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --affine-cfg --affine-scalrep  --affine-loop-coalescing --lower-affine --cse --canonicalize &> matrix-opt.mlir

/// Single pass
/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --affine-cfg --affine-scalrep  --affine-loop-coalescing --lower-affine --cse --canonicalize --convert-polygeist-to-llvm --cse --canonicalize &> matrix-llvm.mlir

#include <stdlib.h>
#include <stdio.h>
#define N 10

int main(int argc, char *argv[]) {
  double A[N][N], B[N][N];
  int test = rand() % 100;
  
  #pragma omp parallel
  {
    #pragma omp single
    {
      /// Compute the A matrix.
      for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
          /// Each task computes one A[i][j].
          #pragma omp task firstprivate(i, j) depend(out : A[i][j])
          {
            A[i][j] = i * 1.0 + test;
          }
        }
      }

      /// Compute the B matrix using A.
      /// For row zero, B[0][j] only depends on A[0][j].
      for (int j = 0; j < N; j++) {
        #pragma omp task firstprivate(j) depend(in : A[0][j]) depend(out : B[0][j])
        {
          B[0][j] = A[0][j];
        }
      }

      /// For rows 1..N-1, B[i][j] depends on A[i][j] and the previous row A[i-1][j].
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

  /// Print the computed matrices.
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("A[%d][%d] = %f   ", i, j, A[i][j]);
    }
    printf("\n");
  }
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("B[%d][%d] = %f   ", i, j, B[i][j]);
    }
    printf("\n");
  }
}
