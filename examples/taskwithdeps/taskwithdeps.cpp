// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

/// clang++ -fopenmp -std=c++17 taskwithdeps.cpp  -Xclang -plugin  -Xclang omp-plugin -fplugin=/home/randres/projects/carts/.install/carts/lib/libOpenMPPlugin.so -mllvm -debug-only=omp-plugin -S &> output.ll

__attribute__((noinline))
void long_computation(int &x) {
  int i;
  for (i = 0; i < 1000000; i++) {
    x += i;
  }
}

__attribute__((noinline))
void short_computation(int &x) {
  int i;
  for (i = 0; i < 1000; i++) {
    x += i;
  }
}

/// EDT 1 - carts.edt
int compute() {
  int a = 0;
  int b = 0;

  #pragma omp parallel
  {
      #pragma omp single
      {
        // Task 1: Initializes 'a'
        #pragma omp task shared(a) depend(out: a)
        {
            printf("Task 1: Initializing a\n");
            a = 10;
        }

        // Task 2: Depends on Task 1 (reads 'a') and modifies 'b'
        #pragma omp task shared(a, b) depend(in: a) depend(out: b)
        {
            printf("Task 2: Reading a=%d and updating b\n", a);
            b = a + 5;
        }

        // Task 3: Depends on Task 2 (reads 'b')
        #pragma omp task shared(b) depend(in: b)
        {
            printf("Task 3: Final value of b=%d\n", b);
        }
      } // End of single
  } // End of parallel
  return 0;
}