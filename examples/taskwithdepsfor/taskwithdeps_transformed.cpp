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



// __attribute__((noinline))
// void long_computation(int &x) {
//   int i;
//   for (i = 0; i < 1000000; i++) {
//     x += i;
//   }
// }

// __attribute__((noinline))
// void short_computation(int &x) {
//   int i;
//   for (i = 0; i < 1000; i++) {
//     x += i;
//   }
// }

/// EDT 1 - carts.edt

void func(int N, double *A) {
  for (int i = 0; i < N; ++i) {
    A[i] = i * 1.0;
  }
}

void compute(int N, double *A, double *B) {
edt_function_4(N, A, B);

}

// int main() {
//   srand(time(NULL));
//   int x = rand(), y = rand(), res = 0;
//   #pragma omp parallel
//   #pragma omp single
//   {
//       #pragma omp task depend(out: res,x,y) //T0 
//       {
//           res = 0;
//           x = rand();
//           y = rand();
//       }
//       #pragma omp task depend(out: res) //T0
//           res = 0;
//       #pragma omp task depend(out: x) //T1
//           long_computation(x);
//       #pragma omp task depend(out: y) //T2
//           short_computation(y);
//   }
//   #pragma omp parallel
//   #pragma omp single
//   {
//       #pragma omp task depend(in: x)
//       {
//         res += x;
//         x++;
//       }
//       #pragma omp task depend(in: y)
//           res += y;
//       #pragma omp task depend(in: res) //T5
//           printf("%d", res);
//   }
//   return 0;
// }

static void __attribute__((annotate("omp.task"))) edt_function_1(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * B __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default")))) {

}

static void __attribute__((annotate("omp.task"))) edt_function_2(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * A __attribute__((annotate("omp.default")))) {

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

