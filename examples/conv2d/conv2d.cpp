#include <cstdio>
#include <cstdlib>
#include <omp.h>

#define BLOCK_SIZE 1024
#define KERNEL_SIZE 3
#define TASK 4
#define TOTAL_SIZE_PER_ROW (BLOCK_SIZE * TASK)

double **conv2d(double **C, double **A, double **K, int size, int ksize,
                int stride, int &gm) {
  int i, j, k, l, m, n;
  m = 0;
  for (i = 0; i < size - ksize; i += stride) {
    n = 0;
    for (j = 0; j < size - ksize; j += stride) {
      double tmp = 0.0;
      for (k = 0; k < ksize; ++k) {
        for (l = 0; l < ksize; ++l) {
          tmp += A[i + k][j + k] * K[k][l];
        }
      }
      C[m][n++] = tmp;
    }
    ++m;
  }
  gm = m;
  return C;
}

double **conv2dOMP(double **C, double **A, double **K, int size, int ksize,
                   int stride, int &gm) {
#pragma omp parallel
  {
#pragma omp single
    {
#pragma omp task // T1
      {
        int i, j, k, l, m, n, sz, len;
        m = 0;
        len = BLOCK_SIZE;
        for (i = 0; i < len; i++) {
          n = 0;
          for (j = 0; j < size - ksize; j++) {
            double tmp = 0.0;
            for (k = 0; k < ksize; ++k) {
              for (l = 0; l < ksize; ++l) {
                tmp += A[i + k][j + k] * K[k][l];
              }
            }
            C[m][n++] = tmp;
          }
          ++m;
        }
      }

#pragma omp task // T2
      {
        int i, j, k, l, m, n, sz, len;
        m = BLOCK_SIZE;
        len = 2 * BLOCK_SIZE;
        for (i = BLOCK_SIZE; i < len; i++) {
          n = 0;
          for (j = 0; j < size - ksize; j++) {
            double tmp = 0.0;
            for (k = 0; k < ksize; ++k) {
              for (l = 0; l < ksize; ++l) {
                tmp += A[i + k][j + k] * K[k][l];
              }
            }
            C[m][n++] = tmp;
          }
          ++m;
        }
      }

#pragma omp task // T3
      {
        int i, j, k, l, m, n, sz, len;
        m = 2 * BLOCK_SIZE;
        len = 3 * BLOCK_SIZE;
        for (i = 2 * BLOCK_SIZE; i < len; i++) {
          n = 0;
          for (j = 0; j < size - ksize; j++) {
            double tmp = 0.0;
            for (k = 0; k < ksize; ++k) {
              for (l = 0; l < ksize; ++l) {
                tmp += A[i + k][j + k] * K[k][l];
              }
            }
            C[m][n++] = tmp;
          }
          ++m;
        }
      }

#pragma omp task depend(out : gm) // T4
      {
        int i, j, k, l, m, n, sz, len;
        m = 3 * BLOCK_SIZE;
        len = size - ksize;
        for (i = 3 * BLOCK_SIZE; i < len; i++) {
          n = 0;
          for (j = 0; j < size - ksize; j++) {
            double tmp = 0.0;
            for (k = 0; k < ksize; ++k) {
              for (l = 0; l < ksize; ++l) {
                tmp += A[i + k][j + k] * K[k][l];
              }
            }
            C[m][n++] = tmp;
          }
          ++m;
        }
        gm = m;
      }
    }
  }
  return C;
}

double **setMM(int n) {
  int i, j;
  double **N = (double **)malloc(sizeof(double *) * n);
  if (!N)
    exit(11);
  for (i = 0; i < n; ++i) {
    N[i] = (double *)malloc(sizeof(double) * n);
    if (!N[i])
      exit(13);
    for (j = 0; j < n; ++j) {
      N[i][j] = 0.0;
    }
  }
  return N;
}

double **setMM1(int n) {
  int i, j;
  double **N = (double **)malloc(sizeof(double *) * n);
  if (!N)
    exit(11);
  for (i = 0; i < n; ++i) {
    N[i] = (double *)malloc(sizeof(double) * n);
    if (!N[i])
      exit(12);
    for (j = 0; j < n; ++j) {
      N[i][j] = 1.0;
    }
  }
  return N;
}

int checkMM(double **C, int size) {
  int i, j, err = 0;
  for (i = 0; i < size; ++i)
    for (j = 0; j < size; ++j)
      if ((int)(C[i][j] * 1000) != (KERNEL_SIZE * KERNEL_SIZE))
        err++; // Transform the double to integer to make sure that the
               // comparison works
  return err;
}

int main(int argc, char **argv) {
  double **C, **A, **Kernel;
  double serTime = 0.0, ompTime = 0.0;
  int fin, ser, err;
  C = setMM(TOTAL_SIZE_PER_ROW);
  A = setMM1(TOTAL_SIZE_PER_ROW);
  Kernel = setMM1(KERNEL_SIZE);
  printf("Allocated arrays: %p %p %p\n", C, A, Kernel);
  ompTime = -omp_get_wtime();
  C = conv2dOMP(C, A, Kernel, TOTAL_SIZE_PER_ROW, KERNEL_SIZE, 1, fin);
  ompTime += omp_get_wtime();
  err = checkMM(C, fin);
  if (err)
    printf("Success!\n");
  else
    printf("Failure with %d errors\n", err);
  serTime = -omp_get_wtime();
  C = conv2d(C, A, Kernel, TOTAL_SIZE_PER_ROW, KERNEL_SIZE, 1, ser);
  serTime += omp_get_wtime();
  printf("Size of resulting matrix: Ser [%d x %d] vs Par [%d x %d]\n", ser, ser,
         fin, fin);
  printf("Serial Time %lf vs Paralell Time %lf\n", serTime, ompTime);
  return 0;
}
