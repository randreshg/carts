// Generate standard MLIR dialect
/// cgeist taskwithdeps.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  &> taskwithdeps.mlir

// Passes
/// carts-opt taskwithdeps.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis,create-events,convert-arts-to-llvm,arts-codegen &> taskwithdeps-arts.mlir

// Optimizations
/// carts-opt taskwithdeps.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --affine-cfg --affine-scalrep --affine-loop-invariant-code-motion  --affine-loop-coalescing --cse  --canonicalize &> taskwithdeps-arts.mlir

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>


/// EDT 1 - arts.edt
int main() {
  int a = 0;
  int b = 0;

  #pragma omp parallel
  {
    #pragma omp single
    {
      // Task 1: Initializes 'a'
      #pragma omp task depend(out: a)
      {
          printf("Task 1: Initializing a\n");
          a = 10;
      }

      // Task 2: Depends on Task 1 (reads 'a') and modifies 'b'
      #pragma omp task depend(in: a) depend(out: b)
      {
          printf("Task 2: Reading a=%d and updating b\n", a);
          b = a + 5;
      }

      // Task 3: Depends on Task 2 (reads 'b')
      #pragma omp task depend(in: b)
      {
          printf("Task 3: Final value of b=%d\n", b);
      }
    } // End of single
  } // End of parallel
  return 0;
}
