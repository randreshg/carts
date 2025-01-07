// Code 1

// #include <cstdlib>
// #include <stdio.h>

// #include <omp.h>
// #include <stdlib.h>
// #include <time.h>

/// clang++ -fopenmp -std=c++17 taskwithdeps.cpp  -Xclang -plugin  -Xclang omp-plugin -fplugin=/home/randres/projects/arts/.install/arts/lib/libOpenMPPlugin.so -mllvm -debug-only=omp-plugin -S &> output.ll

/// cgeist taskwithdeps.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include --raise-scf-to-affine &> taskwithdeps.mlir

// Convert OpenMP to ARTS
/// carts-opt taskwithdeps.mlir --convert-openmp-to-arts &> taskwithdeps-arts.mlir
/// carts-opt taskwithdeps.mlir --convert-openmp-to-arts -debug-only=convert-openmp-to-arts &> taskwithdeps-arts.mlir

/// Convert ARTS to Funcs
/// carts-opt taskwithdeps-arts.mlir --convert-arts-to-funcs -debug-only=convert-arts-to-funcs,arts-codegen &> taskwithdeps-func.mlir

/// Optimize
/// polygeist-opt outputaffine.mlir --cse --affine-cfg --affine-scalrep --polygeist-mem2reg &> optimized.mlir

#include <stdio.h>
#include <stdlib.h>
void compute(int N, double *A, double *B) {
  double test = rand() % 100;
  #pragma omp parallel
  {
    #pragma omp single
    {
      for (int i = 0; i < N; i++) {
        // Task 1: Compute A[i]
        #pragma omp task depend(out : A[i])
          A[i] = i * 1.0 + test;

        // Task 2: Compute B[i] based on A[i] and A[i-1] (inter-loop dependency)
        #pragma omp task depend(in : A[i], A[i - 1]) depend(out : B[i])
          B[i] = A[i] + (i > 0 ? A[i - 1] : 0);

      }
    }
  }
}
