#include <omp.h>
#include <stdio.h>

/*
cgeist matrixmul.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > matrixmul.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=datablock,datablock-analysis,create-datablocks &> matrixmul-arts.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --datablock --cse -debug-only=edt-invariant-code-motion,datablock,create-datablocks &> matrixmul-arts.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --cse --polygeist-mem2reg --canonicalize-polygeist --datablock --cse --canonicalize-scf-for --canonicalize-polygeist --polygeist-mem2reg --edt-pointer-rematerialization --create-events --create-epochs --canonicalize-polygeist --convert-arts-to-llvm --canonicalize-polygeist --cse --convert-polygeist-to-llvm --cse -debug-only=convert-arts-to-llvm &> matrixmul-arts.mlir

mlir-translate --mlir-to-llvmir matrixmul-arts.mlir &> matrixmul-arts.ll

clang matrixmul-arts.ll -O3 -g0 -march=native -o matrixmul -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm
*/

int main() {
  int shared = 0; // Initialize shared variable
  int num_threads = 4;

  printf("Running with %d threads.\n", num_threads);

#pragma omp parallel num_threads(num_threads) shared(shared)
  {
    int priv1; // Each thread gets its own priv1
    int x = omp_get_thread_num();
    priv1 = x; // Assign thread-specific value

    if (x % 2 == 0) {
      printf("Thread %d (priv1=%d) ----> EVEN\n", x, priv1);
    } else {
      printf("Thread %d (priv1=%d) ----> ODD\n", x, priv1);
    }

    // Safely increment shared variable
#pragma omp atomic
    shared++;
  }

  // The final value of shared should be the initial value + num_threads
  printf("Final shared value: %d (expected %d)\n", shared, num_threads);

  return 0;
}
