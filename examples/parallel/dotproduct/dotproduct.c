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

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 1000

int main() {
  int a[N], b[N];
  long long dot_product = 0;
  int i;

  /// Initialize arrays
  for (i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
  }

  printf("Performing parallel dot product...\n");

  /// Parallel loop for dot product calculation
  #pragma omp parallel for reduction(+:dot_product)
  for (i = 0; i < N; i++) {
    dot_product += (long long)a[i] * b[i];
  }

  printf("Parallel dot product finished.\n");
  printf("Dot product: %lld\n", dot_product);

  /// Verify results
  long long expected_dot_product = 0;
  for (i = 0; i < N; i++) {
    expected_dot_product += (long long)a[i] * b[i];
  }

  if (dot_product != expected_dot_product) {
      printf("Error: Parallel result (%lld) does not match expected result (%lld)\n", dot_product, expected_dot_product);
      return 1;
  } else {
      printf("Result verified successfully.\n");
  }


  return 0;
}
