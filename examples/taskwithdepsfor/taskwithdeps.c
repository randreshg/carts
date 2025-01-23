// Code 1

// #include <cstdlib>
// #include <stdio.h>

// #include <omp.h>
// #include <stdlib.h>
// #include <time.h>

/// clang++ -fopenmp -std=c++17 taskwithdeps.cpp  -Xclang -plugin  -Xclang omp-plugin -fplugin=/home/randres/projects/arts/.install/arts/lib/libOpenMPPlugin.so -mllvm -debug-only=omp-plugin -S &> output.ll


// Convert to LLVM IR
/// cgeist 
/// cgeist taskwithdeps.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include --raise-scf-to-affine &> taskwithdeps.mlir
/// cgeist taskwithdeps.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include &> taskwithdeps.mlir

// Convert OpenMP to ARTS
/// carts-opt taskwithdeps.mlir --convert-openmp-to-arts --cse --canonicalize &> taskwithdeps-arts.mlir
/// carts-opt taskwithdeps.mlir --convert-openmp-to-arts --cse --canonicalize -debug-only=convert-openmp-to-arts &> taskwithdeps-arts.mlir

/// Convert ARTS to Funcs
/// carts-opt taskwithdeps-arts.mlir --convert-arts-to-funcs -debug-only=convert-arts-to-funcs,arts-codegen &> taskwithdeps-func.mlir

/// Optimize
/// polygeist-opt outputaffine.mlir --cse --affine-cfg --affine-scalrep --polygeist-mem2reg &> optimized.mlir

// polygeist-opt taskwithdeps.mlir --convert-polygeist-to-llvm  &> optimized.mlir

#include <stdlib.h>
#include <stdio.h>
void compute(int N, double *A, double *B) {
  A = (double *)malloc(N * sizeof(double));
  B = (double *)malloc(N * sizeof(double));
  int test = rand() % 100;
  #pragma omp parallel
  {
    #pragma omp single
    {
      for (int i = 0; i < N; i++) {
        // A[i] = i * 1.0 + test;
        // Task 1: Compute A[i]
        #pragma omp task depend(out : A[i])
        {
          // printf("A[%d] = %f\n", i, A[i]);
          A[i] = i * 1.0 + test;
          // printf("A[%d] = %f\n", i, A[i]);
        }

        // Task 2: Compute B[i] based on A[i] and A[i-1] (inter-loop dependency)
        // #pragma omp task depend(in : A[i], A[i - 1]) depend(out : B[i])
        //   B[i] = A[i] + (i > 0 ? A[i - 1] : 0);
      }
    }
  }
  printf("A[0] = %f\n", A[0]);
}


// void compute_non_affine(int N, double *A, double *B, int *indices) {
//   A = (double *)malloc(N * sizeof(double));
//   B = (double *)malloc(N * sizeof(double));
//   indices = (int *)malloc(N * sizeof(int));

//   // Initialize the index mapping
//   for (int i = 0; i < N; i++) {
//     indices[i] = (i * i + 3) % N; // Non-affine mapping
//   }

//   int test = rand() % 100;

//   #pragma omp parallel
//   {
//     #pragma omp single
//     {
//       for (int i = 0; i < N; i++) {
//         // Task 1: Compute A[indices[i]]
//         #pragma omp task depend(out : A[indices[i]])
//           A[indices[i]] = indices[i] * 1.0 + test;

//         // Task 2: Compute B[indices[i]] based on A[indices[i]] and A[indices[j]]
//         // where j is dynamically determined (non-affine relationship)
//         int j = (i + test) % N; // Dynamically determined dependency
//         #pragma omp task depend(in : A[indices[i]], A[indices[j]]) depend(out : B[indices[i]])
//           B[indices[i]] = A[indices[i]] + A[indices[j]];
//       }
//     }
//   }
//   printf("A[0] = %f\n", A[0]);
//   free(A);
//   free(B);
//   free(indices);
// }


