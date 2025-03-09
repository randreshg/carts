// Generate standard MLIR dialect
/// cgeist taskwithdeps.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  &> taskwithdeps.mlir

// Passes
/// carts-opt taskwithdeps.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis,create-events,convert-arts-to-llvm,arts-codegen &> taskwithdeps-arts.mlir

// Optimizations
/// carts-opt taskwithdeps.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --affine-cfg --affine-scalrep  --affine-loop-coalescing --lower-affine --cse --canonicalize &> taskwithdeps-arts.mlir

/// Single command
// carts-opt taskwithdeps.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --affine-cfg --affine-scalrep  --affine-loop-coalescing --lower-affine --cse --canonicalize --convert-polygeist-to-llvm --cse --canonicalize &> taskwithdeps-llvm.mlir
// mlir-translate --mlir-to-llvmir taskwithdeps-llvm.mlir > taskwithdeps.ll

/// Translate to LLVM IR
/// carts-opt taskwithdeps-arts.mlir --convert-polygeist-to-llvm --cse --canonicalize &> taskwithdeps-llvm.mlir
/// mlir-translate --mlir-to-llvmir taskwithdeps-llvm.mlir > taskwithdeps.ll
/// clang -O3 taskwithdeps.ll -o taskwithdeps -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm


#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>


/// EDT 1 - arts.edt
int main() {
  printf("-----------------\nMain function\n-----------------\n");
  int a = 0;
  int b = 0;
  srand(time(NULL));

  #pragma omp parallel
  {
    // (__arts_edt_2)
    #pragma omp single
    {
      printf("Main thread: Creating tasks\n");
      // Task 1: Initializes 'a'
      // (__arts_edt_3)
      #pragma omp task depend(inout: a)
      {
          a = rand() % 100;
          printf("Task 1: Initializing a with value %d\n", a);
      }

      // Task 2: Depends on Task 1 (reads 'a') and modifies 'b'
      // (__arts_edt_4)
      /// a is the 2nd one
      #pragma omp task depend(in: a) depend(inout: b)
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

  printf("A: %d, B: %d\n", a, b);
  printf("-----------------\nMain function DONE\n-----------------\n");
  return 0;
}
