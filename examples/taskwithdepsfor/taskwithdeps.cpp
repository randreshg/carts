// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

/// EDT 1 - carts.edt

void func(int N, double *A) {
  for (int i = 0; i < N; ++i) {
    A[i] = i * 1.0;
  }
}

void compute(int N, double *A, double *B) {
  #pragma omp parallel
  {
    #pragma omp single
    {
      for (int i = 0; i < N; i++) {
        // Task 1: Compute A[i]
        #pragma omp task depend(out : A[i])
                A[i] = i * 1.0;

        // Task 2: Compute B[i] based on A[i] and A[i-1] (inter-loop dependency)
        #pragma omp task depend(in : A[i], A[i - 1]) depend(out : B[i])
          B[i] = A[i] + (i > 0 ? A[i - 1] : 0);
      }
    }
  }
}
