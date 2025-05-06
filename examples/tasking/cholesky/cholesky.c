#include <math.h>
#include <omp.h>
#include <stdio.h>

#define BLOCK_SIZE 16
#define TOLERANCE 1e-6
#define N 256

int main() {
  double A[N * N];
  double L_parallel[N * N];
  double L_sequential[N * N];

  // Generate positive-definite matrix (A = L*L^T)
  for (int i = 0; i < N; i++) {
    for (int j = 0; j <= i; j++) {
      L_parallel[i * N + j] = (double)rand() / RAND_MAX;
      L_parallel[j * N + i] = L_parallel[i * N + j];
    }
    L_parallel[i * N + i] += N;
  }

  // Compute A = L*L^T
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i * N + j] = 0.0;
      for (int k = 0; k < N; k++) {
        A[i * N + j] += L_parallel[i * N + k] * L_parallel[j * N + k];
      }
    }
  }

  // Copy A to L_parallel and L_sequential
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      L_parallel[i * N + j] = A[i * N + j];
      L_sequential[i * N + j] = A[i * N + j];
    }
  }

  // Parallel Cholesky
  double start = omp_get_wtime();
  #pragma omp parallel
  #pragma omp single
  {
    for (int k = 0; k < N; k += BLOCK_SIZE) {
      int kb = (k + BLOCK_SIZE > N) ? N - k : BLOCK_SIZE;

      #pragma omp task depend(inout : L_parallel[k * N])
      {
        for (int i = k; i < k + kb; i++) {
          for (int j = k; j < i; j++) {
            L_parallel[i * N + i] -= L_parallel[i * N + j] * L_parallel[i * N + j];
          }
          L_parallel[i * N + i] = sqrt(L_parallel[i * N + i]);

          for (int j = i + 1; j < k + kb; j++) {
            for (int m = k; m < i; m++) {
              L_parallel[j * N + i] -= L_parallel[j * N + m] * L_parallel[i * N + m];
            }
            L_parallel[j * N + i] /= L_parallel[i * N + i];
          }
        }
      }

      for (int j = k + kb; j < N; j += BLOCK_SIZE) {
        int jb = (j + BLOCK_SIZE > N) ? N - j : BLOCK_SIZE;
        #pragma omp task depend(in : L_parallel[k * N + k]) \
                   depend(inout : L_parallel[j * N + k]) \
                   depend(inout : L_parallel[j * N + j])
        {
          for (int i = k; i < k + kb; i++) {
            for (int i = k; i < k + kb; i++) {
              for (int col = j; col < j + jb; col++) {
                for (int m = k; m < i; m++) {
                  L_parallel[col * N + i] -= L_parallel[col * N + m] * L_parallel[m * N + i];
                }
                L_parallel[col * N + i] /= L_parallel[i * N + i];
              }
            }
          }
        }
      }

      for (int j = k + kb; j < N; j += BLOCK_SIZE) {
        int jb = (j + BLOCK_SIZE > N) ? N - j : BLOCK_SIZE;
        for (int i = j; i < N; i += BLOCK_SIZE) {
          int ib = (i + BLOCK_SIZE > N) ? N - i : BLOCK_SIZE;
          #pragma omp task depend(in : L_parallel[j * N + k], L_parallel[i * N + k]) \
                     depend(inout : L_parallel[i * N + j])
          {
            for (int col = j; col < j + jb; col++) {
              for (int row = i; row < i + ib; row++) {
                if (row >= col) {
                  for (int m = k; m < k + kb; m++) {
                    L_parallel[row * N + col] -= L_parallel[row * N + m] * L_parallel[col * N + m];
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  double parallel_time = omp_get_wtime() - start;

  // Sequential Cholesky
  start = omp_get_wtime();
  for (int k = 0; k < N; k++) {
    L_sequential[k * N + k] = sqrt(L_sequential[k * N + k]);
    for (int i = k + 1; i < N; i++) {
      L_sequential[i * N + k] /= L_sequential[k * N + k];
    }
    for (int j = k + 1; j < N; j++) {
      for (int i = j; i < N; i++) {
        L_sequential[i * N + j] -= L_sequential[i * N + k] * L_sequential[j * N + k];
      }
    }
  }
  double sequential_time = omp_get_wtime() - start;

  // Verify results
  int parallel_correct = 1;
  int sequential_correct = 1;
  
  // Verify parallel result
  for (int i = 0; i < N && parallel_correct; i++) {
    for (int j = 0; j <= i; j++) {
      double sum = 0.0;
      for (int k = 0; k <= j; k++) {
        sum += L_parallel[i * N + k] * L_parallel[j * N + k];
      }
      if (fabs(A[i * N + j] - sum) > TOLERANCE) {
        parallel_correct = 0;
        break;
      }
    }
  }

  // Verify sequential result
  for (int i = 0; i < N && sequential_correct; i++) {
    for (int j = 0; j <= i; j++) {
      double sum = 0.0;
      for (int k = 0; k <= j; k++) {
        sum += L_sequential[i * N + k] * L_sequential[j * N + k];
      }
      if (fabs(A[i * N + j] - sum) > TOLERANCE) {
        sequential_correct = 0;
        break;
      }
    }
  }

  printf("Parallel Cholesky %s!\n", parallel_correct ? "verification passed" : "verification failed");
  printf("Sequential Cholesky %s!\n", sequential_correct ? "verification passed" : "verification failed");
  printf("Parallel time: %.2fs\nSequential time: %.2fs\n", parallel_time, sequential_time);

  return 0;
}
