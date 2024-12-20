// Code 1

// #include <cstdlib>
// #include <stdlib.h>

/// cgeist taskwithdeps_annotaded.c -S  -I/usr/include --raise-scf-to-affine &> outputaffine.mlir
/// polygeist-opt outputaffine.mlir --cse --affine-cfg --affine-scalrep --polygeist-mem2reg &> optimized.mlir

void arts_acquire(void * ptr);
void arts_signal(void * ptr);
void arts_parallel_0(int N, double *A, double *B);
void arts_single_0(int N, double *A, double *B);
void arts_task_0(int i, double *A, double *B);
void arts_task_1(int i, double *A, double * B);

void compute(int N, double *A, double *B) {
  arts_parallel_0(N, A, B);
}

void arts_parallel_0(int N, double *A, double *B) {
  arts_single_0(N, A, B);
}

void arts_single_0(int N, double *A, double *B) {
  for (int i = 0; i < N; i++) {
    // Task 1: Compute A[i]
    arts_task_0(i, A, B);

    // Task 2: Compute B[i] based on A[i] and A[i-1] (inter-loop dependency)
    arts_task_1(i, A, B);
  }
}

void arts_task_0(int i, double *A, double *B) {
  A[i] = i * 1.0;
  arts_signal(&A[i]);
}

void arts_task_1(int i, double *A, double * B) {
  arts_acquire(&A[i]);
  arts_acquire(&A[i - 1]);
  B[i] = A[i] + (i > 0 ? A[i - 1] : 0);
  arts_signal(&B[i]);
}
