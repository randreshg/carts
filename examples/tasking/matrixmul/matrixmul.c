#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

// #define N 10
// #define BS 5

/*
cgeist matrixmul.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > matrixmul.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --symbol-dce -debug-only=convert-openmp-to-arts &> matrixmul-arts.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --symbol-dce &> matrixmul-arts.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --db --cse -debug-only=db,create-datablocks &> matrixmul-arts.mlir

carts-opt matrixmul.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize-polygeist --loop-invariant-code-motion --canonicalize-polygeist --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize-polygeist --create-datablocks --canonicalize-polygeist --db --cse --canonicalize-scf-for --canonicalize-polygeist --polygeist-mem2reg --edt-pointer-rematerialization --create-epochs --canonicalize-polygeist --convert-arts-to-llvm --canonicalize-polygeist --cse --convert-polygeist-to-llvm --cse -debug-only=convert-arts-to-llvm &> matrixmul-arts.mlir

mlir-translate --mlir-to-llvmir matrixmul-arts.mlir &> matrixmul-arts.ll

clang matrixmul-arts.ll -O3 -g0 -march=native -o matrixmul -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm

python3 run_comparison.py \
  --problem_sizes "10 5" \
  --iterations_per_size 5 \
  --target_examples matrixmul \
  --example_base_dirs "tasking" \
  --output_file matrixmul_results.json \
  --csv_output_file matrixmul_benchmark.csv \
  --graph_output_file matrixmul_graph.png \
  --md_output_file matrixmul_summary.md
*/
#define TOLERANCE 1e-6

int main(int argc, char *argv[]) {
  // Check for the correct number of arguments
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <matrix_size> <block_size>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const int N = atoi(argv[1]);
  const int BS = atoi(argv[2]);
  if (N <= 0 || BS <= 0) {
    fprintf(stderr, "Matrix size and block size must be positive integers.\n");
    return EXIT_FAILURE;
  }

  float A[N][N], B[N][N], C_parallel[N][N], C_sequential[N][N];

  // Initialize matrices
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i][j] = (float)(i + j);
      B[i][j] = (float)(i - j);
      C_parallel[i][j] = 0.0f;
      C_sequential[i][j] = 0.0f;
    }
  }

  // Parallel computation
  double t_start = omp_get_wtime();
  #pragma omp parallel
  #pragma omp single
  {
    for (int i = 0; i < N; i += BS) {
      for (int j = 0; j < N; j += BS) {
        for (int k = 0; k < N; k += BS) {
          #pragma omp task depend(in : A[i][k], B[k][j]) depend(inout : C_parallel[i][j])
          {
            // printf("Thread %d is working on block (%d, %d, %d) to (%d, %d, %d)\n",
            //        omp_get_thread_num(), i, j, k, i + BS, j + BS, k + BS);
            for (int ii = i; ii < i + BS; ii++)
              for (int jj = j; jj < j + BS; jj++)
                for (int kk = k; kk < k + BS; kk++) {
                  C_parallel[ii][jj] += A[ii][kk] * B[kk][jj];
                }
          }
        }
      }
    }
  }
  double t_end = omp_get_wtime();
  printf("Finished in %f seconds\n", t_end - t_start);

  // Sequential computation
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      float sum = 0.0f;
      for (int k = 0; k < N; k++) {
        sum += A[i][k] * B[k][j];
      }
      C_sequential[i][j] = sum;
    }
  }

  // Verification
  int verified = 1;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      if (fabs(C_parallel[i][j] - C_sequential[i][j]) > TOLERANCE) {
        printf("Mismatch at (%d,%d): Parallel=%.2f, Sequential=%.2f\n", i, j,
               C_parallel[i][j], C_sequential[i][j]);
        verified = 0;
      }
    }
  }

  if (verified) {
    printf("Result: CORRECT\n");
  } else {
    printf("Result: INCORRECT\n");
  }
  fflush(stdout);

  return 0;
}