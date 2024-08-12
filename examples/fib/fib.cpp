// Code 1
// clang++ -fopenmp -O2 -Rpass=arts-transform fib.cpp -o fib
// clang++ -fopenmp -Rpass=arts-transform -mllvm -debug -O2 -g3 fib.cpp -o fib
// &> fib.txt

// clang++ -fopenmp -Rpass=arts-transform -mllvm
// -debug-only=arts-transform,arts-ir-builder -O3 -g1 fib.cpp -o fib &>
// fibOutput.ll

// clang++ -fopenmp -Rpass=arts-transform -mllvm -debug-only=arts-transform
// -mllvm -arts-transform-print-module-before -O2 fib.cpp -o fib

// Generate LLVM IR
// clang++ -fopenmp -S -emit-llvm -O3 -g1 fib.cpp -o fib.ll

#include <omp.h>
#include <stdio.h>

int fib(int n) {
  int i, j;
  if (n < 2)
    return n;
  else {
    /// EDT 1
    {
      /// EDT 2
      #pragma omp task shared(i)
      i = fib(n - 1);

      /// EDT 3
      #pragma omp task shared(j)
            j = fib(n - 2);
    }

#pragma omp taskwait
    return i + j;
  }
}

int main() {
  int n = rand();
  int i = rand();
  omp_set_dynamic(0);
  omp_set_num_threads(4);

/// EDT 0
#pragma omp parallel firstprivate(n)
  {
    #pragma omp single
    printf("fib(%d) = %d\n", n, fib(n));
  }

  printf("Done %d, %d\n", n, i);
}