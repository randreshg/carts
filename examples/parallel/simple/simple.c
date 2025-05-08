#include <omp.h>
#include <stdio.h>

/*
cgeist simple.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > simple.mlir

carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts &> simple-arts.mlir

carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --symbol-dce &> simple-arts.mlir

carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=datablock,datablock-analysis,create-datablocks &> simple-arts.mlir

carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=edt-invariant-code-motion,datablock,create-datablocks &> simple-arts.mlir

carts-opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse --canonicalize-scf-for --canonicalize-polygeist --polygeist-mem2reg --edt-pointer-rematerialization --create-events --create-epochs --canonicalize-polygeist --convert-arts-to-llvm --canonicalize-polygeist --cse --convert-polygeist-to-llvm --cse -debug-only=convert-arts-to-llvm &> simple-arts.mlir

mlir-translate --mlir-to-llvmir simple-arts.mlir &> simple-arts.ll

clang simple-arts.ll -O3 -g0 -march=native -o simple -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm
*/
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

int main() {
  printf("Demonstrating basic parallel region execution.\n");
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    int num_threads = omp_get_num_threads();
    printf("Hello from thread %d of %d\n", tid, num_threads);
    
    for (int i = 0; i < 100000000; i++) {
      if (i % 100000000 == 0) {
        printf("Thread %d is working on iteration %d\n", tid, i);
      }
    }
  }

  printf("Parallel region finished.\n");

  return 0;
}
