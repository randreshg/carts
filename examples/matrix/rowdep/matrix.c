// Generate standard MLIR dialect
/// cgeist matrix.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  &> taskwithdeps.mlir

// Passes
/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis,create-events,convert-arts-to-llvm,arts-codegen &> matrix-arts.mlir

// Optimizations
/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --affine-cfg --affine-scalrep  --affine-loop-coalescing --lower-affine --cse --canonicalize &> matrix-arts.mlir

/// carts-opt matrix-arts.mlir --convert-polygeist-to-llvm --cse --canonicalize &> matrix-llvm.mlir

/// mlir-translate --mlir-to-llvmir matrix-llvm.mlir > matrix.ll

/// clang matrix.ll -O3 -g0 -march=native -o matrix -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm


#include <stdlib.h>
#include <stdio.h>
#define N 10

int main() {
  double A[N][N], B[N][N];
  int test = rand() % 100;

  #pragma omp parallel
  {
    #pragma omp single
    {
      // Compute each row of matrix A in a separate task.
      for (int i = 0; i < N; i++) {
        /// Load events for out dependencies
        #pragma omp task firstprivate(i, test) depend(out: A[i])
        {
          for (int j = 0; j < N; j++) {
            A[i][j] = i * 1.0 + test;
          }
        }
      }

      // Compute row 0 of B using the entire row A[0].
      #pragma omp task depend(in: A[0]) depend(out: B[0])
      {
        for (int j = 0; j < N; j++) {
          B[0][j] = A[0][j];
        }
      }

      // Compute rows 1..N-1 of B.
      // Each B row i depends on both A row i and A row i-1.
      for (int i = 1; i < N; i++) {
        #pragma omp task firstprivate(i) depend(in: A[i], A[i-1]) depend(out: B[i])
        {
          for (int j = 0; j < N; j++) {
            B[i][j] = A[i][j] + A[i-1][j];
          }
        }
      }
    }
  }

  // Print the computed matrices.
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
  
  return 0;
}

