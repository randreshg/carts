#include <omp.h>
#include <stdio.h>

/*
cgeist addition.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > addition.mlir

carts-opt addition.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --canonicalize-polygeist --datablock --cse -debug-only=convert-openmp-to-arts &> addition-arts.mlir

carts-opt addition.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=datablock,datablock-analysis,create-datablocks &> addition-arts.mlir

carts-opt addition.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=datablock,datablock-analysis,create-datablocks &> addition-arts.mlir

carts-opt addition.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=edt-invariant-code-motion,datablock,create-datablocks &> addition-arts.mlir

carts-opt addition.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --cse --polygeist-mem2reg --canonicalize-polygeist --datablock --cse --canonicalize-scf-for --canonicalize-polygeist --polygeist-mem2reg --edt-pointer-rematerialization --create-events --create-epochs --canonicalize-polygeist --cse -debug-only=convert-arts-to-llvm &> addition-arts.mlir


carts-opt addition.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --cse --polygeist-mem2reg --canonicalize-polygeist --datablock --cse --canonicalize-scf-for --canonicalize-polygeist --polygeist-mem2reg --edt-pointer-rematerialization --create-events --create-epochs --canonicalize-polygeist --convert-arts-to-llvm --canonicalize-polygeist --cse --convert-polygeist-to-llvm --cse -debug-only=convert-arts-to-llvm &> addition-arts.mlir

mlir-translate --mlir-to-llvmir addition-arts.mlir &> addition-arts.ll

clang addition-arts.ll -O3 -g0 -march=native -o addition -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm
*/

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


int main(int argc, char **argv) {
  if(argc < 2) {
    printf("Usage: %s <num_threads>\n", argv[0]);
    return 1;
  }
  const int N = atoi(argv[1]);
  if(N <= 0) {
    printf("Error: N must be a positive integer.\n");
    return 1;
  }
  int a[N], b[N], c[N];

  /// Initialize arrays
  for (int i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
  }

  printf("Performing parallel array addition...\n");

  /// Parallel loop for array addition
  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    printf("Thread %d is working on iteration %d\n", omp_get_thread_num(), i);
    c[i] = a[i] + b[i];
  }

  printf("Parallel addition finished.\n");
  /// Verify results
  bool success = true;
  for (int i = 0; i < N; i++) {
    if (c[i] != (a[i] + b[i])) {
      success = false;
    }
  }

  if (!success) {
    printf("Array addition failed.\n");
  } else {
    printf("Array addition succeeded.\n");
  }

  /// Print C
  printf("C array: ");
  for (int i = 0; i < N; i++) {
    printf("%d ", c[i]);
  }
  printf("\n");


  return 0;
}
