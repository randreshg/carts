// Code 1

// #include <cstdlib>
// #include <stdio.h>

// #include <omp.h>
// #include <stdlib.h>
// #include <time.h>

// Convert to MLIR standard dialect
/// cgeist 
/// cgeist matrix.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  --raise-scf-to-affine  &> matrix.mlir

// Convert OpenMP to ARTS
/// carts-opt matrix.mlir --convert-openmp-to-arts --canonicalize &> matrix-arts.mlir
/// carts-opt matrix.mlir --convert-openmp-to-arts --canonicalize -debug-only=convert-openmp-to-arts,edt-analysis &> matrix-arts.mlir

// DataBlock pass
/// carts-opt matrix-arts.mlir --datablock --cse &> matrix-datablock.mlir
/// carts-opt matrix-arts.mlir --datablock --cse -debug-only=datablock,datablock-analysis &> matrix-datablock.mlir

/// Create ARTS events
/// carts-opt matrix-arts.mlir --create-events --cse &> matrix-events.mlir
/// carts-opt matrix-arts.mlir --create-events --cse -debug-only=datablock-analysis,create-events &> matrix-events.mlir

/// Convert ARTS to Funcs
/// carts-opt matrix-events.mlir --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-arts-to-llvm,arts-codegen,edt-analysis &> matrix-func.mlir

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
      // Compute the A matrix.
      for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
          // Each task computes one A[i][j].
          #pragma omp task firstprivate(i, j) depend(out : A[i][j])
          {
            A[i][j] = i * 1.0 + test;
          }
        }
      }
      
      // Compute the B matrix using A.
      // For row zero, B[0][j] only depends on A[0][j].
      for (int j = 0; j < N; j++) {
        #pragma omp task firstprivate(j) depend(in : A[0][j]) depend(out : B[0][j])
        {
          B[0][j] = A[0][j];
        }
      }
      // For rows 1..N-1, B[i][j] depends on A[i][j] and the previous row A[i-1][j].
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