// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

static void __attribute__((annotate("omp.task"))) edt_function_1(int i, double * B, double * A);
static void __attribute__((annotate("omp.task"))) edt_function_2(int i, double * A);
static void __attribute__((annotate("omp.single"))) edt_function_3(int N, double * A, double * B);
static void __attribute__((annotate("omp.parallel"))) edt_function_4(int N, double * A, double * B);


void compute(int N, double *A, double *B) {
  edt_function_4(N, A, B);

}

static void __attribute__((annotate("omp.task deps(in: A[i], A[i - 1]) deps(out: B[i]"))) edt_function_1(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * B __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default")))) {

          B[i] = A[i] + (i > 0 ? A[i - 1] : 0);
}

static void __attribute__((annotate("omp.task"))) edt_function_2(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * A __attribute__((annotate("omp.default")))) {

          A[i] = i * 1.0;
}

static void __attribute__((annotate("omp.single"))) edt_function_3(
  int N __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default"))), 
  double * B __attribute__((annotate("omp.default")))) {

      for (int i = 0; i < N; i++) {
        // Task 1: Compute A[i]
        edt_function_2(i, A);


        // Task 2: Compute B[i] based on A[i] and A[i-1] (inter-loop dependency)
        edt_function_1(i, B, A);

      }
    
}

static void __attribute__((annotate("omp.parallel"))) edt_function_4(
  int N __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default"))), 
  double * B __attribute__((annotate("omp.default")))) {

    edt_function_3(N, A, B);

  
}

