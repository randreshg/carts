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
  int priv1 = 0, priv2 = 0;
  int shared = 123123;
  int test = 0;
#pragma omp parallel num_threads(4) shared(shared) private(test)
  {
    int x = omp_get_thread_num();
    priv1 = x;
    char *msg = NULL;
    if (x % 2 == 0) {
      printf("Hello from thread %d ----> EVEN\n", x);
    }
    if (x % 2 == 1) {
      printf("Hello from thread %d ----> ODD\n", x);
    }
    shared++;
  }
  printf("priv: %d\n", priv1);
  return 0;
}
