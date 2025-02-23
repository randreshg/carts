// Code 1

// #include <cstdlib>
// #include <stdio.h>

// #include <omp.h>
// #include <stdlib.h>
// #include <time.h>

/// AFFINE
/// cgeist matrix.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  --raise-scf-to-affine  &> matrix.mlir
/// --affine-data-copy-generate 

// Convert to MLIR standard dialect
/// cgeist matrix.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  &> matrix.mlir

// -------------------------------------------------------------------------------------------------------------
// Convert OpenMP to ARTS
/// carts-opt matrix.mlir --lower-affine  &> matrix-arts.mlir
/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --datablock --cse --canonicalize &> matrix-arts.mlir
/// carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis &> matrix-arts.mlir

// Try to raise to affine
/// carts-opt matrix-datablock.mlir --raise-scf-to-affine --affine-cfg --affine-scalrep &> matrix-affine.mlir
/// carts-opt matrix-datablockmlir --affine-data-copy-generate &> matrix-affine-pipeline.mlir

// DataBlock pass
/// carts-opt matrix-arts.mlir --datablock --cse &> matrix-datablock.mlir
/// carts-opt matrix-arts.mlir --datablock --cse -debug-only=datablock,datablock-analysis &> matrix-datablock.mlir

/// Create ARTS events
/// carts-opt matrix-arts.mlir --create-events --cse &> matrix-events.mlir
/// carts-opt matrix-arts.mlir --create-events --cse -debug-only=datablock-analysis,create-events &> matrix-events.mlir

/// Convert ARTS to Funcs
/// carts-opt matrix-events.mlir --convert-arts-to-funcs --cse --canonicalize -debug-only=convert-arts-to-funcs,arts-codegen,edt-analysis &> matrix-func.mlir

/// Optimize
/// polygeist-opt outputaffine.mlir --cse --affine-cfg --affine-scalrep --polygeist-mem2reg &> optimized.mlir

// polygeist-opt matrix.mlir --convert-polygeist-to-llvm  &> optimized.mlir

// polygeist-opt matrix-events.mlir --affine-cfg &> optimized.mlir

#include <stdlib.h>
#include <stdio.h>
#define N 100

void compute() {
  double A[N][N], B[N][N];
  int test = rand() % 100;

  #pragma omp parallel
  {
    #pragma omp single
    {
      // Compute each row of matrix A in a separate task.
      for (int i = 0; i < N; i++) {
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
}

