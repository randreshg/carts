#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK_SIZE 16
#define TOLERANCE 1e-6

/// Dynamic Cholesky decomposition - size N is a parameter
void cholesky_decomposition(int N) {
  srand(time(NULL));

  // Use VLAs (Variable Length Arrays) for dynamic sizing
  double A[N * N];
  double L_parallel[N * N];
  double L_sequential[N * N];
  double L_orig[N * N];

  /// Generate a positive-definite matrix A = L_orig * L_orig^T
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      /// Avoid zeros on off-diagonal
      if (j <= i)
        L_orig[i * N + j] = (double)rand() / RAND_MAX + 0.01;
      /// L_orig is lower triangular
      else
        L_orig[i * N + j] = 0.0;
    }
    /// Strengthen diagonal for positive definiteness
    L_orig[i * N + i] += 1.0;
  }

  for (int i = 0; i < N; i++) {
    /// A is symmetric, compute its lower triangle
    for (int j = 0; j <= i; j++) {
      A[i * N + j] = 0.0;
      for (int k_sum = 0; k_sum <= j; k_sum++)
        A[i * N + j] += L_orig[i * N + k_sum] * L_orig[j * N + k_sum];
    }
  }

  /// Fill upper triangle of A by symmetry
  for (int i = 0; i < N; i++) {
    for (int j = i + 1; j < N; j++)
      A[i * N + j] = A[j * N + i];
  }

  /// Initialize matrices for Cholesky decomposition
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      L_parallel[i * N + j] = A[i * N + j];
      L_sequential[i * N + j] = A[i * N + j];
    }
  }

  printf("Starting Cholesky decomposition for N=%d...\n", N);

  double start_time = omp_get_wtime();

  /// Parallel Cholesky Decomposition (Blocked)
#pragma omp parallel
  {
#pragma omp master
    {
      for (int k = 0; k < N; k += BLOCK_SIZE) {
        int block_size = (k + BLOCK_SIZE > N) ? (N - k) : BLOCK_SIZE;

/// Panel factorization (diagonal block)
#pragma omp task depend(inout : L_parallel[k * N + k])
        {
          for (int i = k; i < k + block_size; i++) {
            /// Diagonal element
            for (int j = k; j < i; j++) {
              L_parallel[i * N + i] -= L_parallel[i * N + j] * L_parallel[i * N + j];
            }
            if (L_parallel[i * N + i] < 0.0) L_parallel[i * N + i] = 0.0;
            L_parallel[i * N + i] = sqrt(L_parallel[i * N + i]);

            /// Off-diagonal in this column
            for (int j = i + 1; j < k + block_size; j++) {
              for (int k_sum = k; k_sum < i; k_sum++) {
                L_parallel[j * N + i] -= L_parallel[j * N + k_sum] * L_parallel[i * N + k_sum];
              }
              if (L_parallel[i * N + i] != 0.0)
                L_parallel[j * N + i] /= L_parallel[i * N + i];
              else
                L_parallel[j * N + i] = 0.0;
            }
          }
        }

        /// Update trailing submatrix (below diagonal block)
        for (int j = k + block_size; j < N; j += BLOCK_SIZE) {
          int j_block_size = (j + BLOCK_SIZE > N) ? (N - j) : BLOCK_SIZE;

#pragma omp task depend(in : L_parallel[k * N + k]) \
    depend(inout : L_parallel[j * N + k])
          {
            for (int i = j; i < j + j_block_size; i++) {
              for (int col = k; col < k + block_size; col++) {
                for (int k_sum = k; k_sum < col; k_sum++) {
                  L_parallel[i * N + col] -= L_parallel[i * N + k_sum] * L_parallel[col * N + k_sum];
                }
                if (L_parallel[col * N + col] != 0.0)
                  L_parallel[i * N + col] /= L_parallel[col * N + col];
                else
                  L_parallel[i * N + col] = 0.0;
              }
            }
          }
        }
      }
    }
  }

  double parallel_time = omp_get_wtime() - start_time;

  /// Sequential Cholesky for verification
  start_time = omp_get_wtime();

  for (int i = 0; i < N; i++) {
    for (int j = 0; j <= i; j++) {
      double sum = 0.0;
      for (int k = 0; k < j; k++) {
        sum += L_sequential[i * N + k] * L_sequential[j * N + k];
      }

      if (i == j) {
        double val = L_sequential[i * N + i] - sum;
        if (val < 0.0) val = 0.0;
        L_sequential[i * N + i] = sqrt(val);
      } else {
        if (L_sequential[j * N + j] != 0.0)
          L_sequential[i * N + j] = (L_sequential[i * N + j] - sum) / L_sequential[j * N + j];
        else
          L_sequential[i * N + j] = 0.0;
      }
    }
  }

  double sequential_time = omp_get_wtime() - start_time;

  /// Verify correctness
  double max_error = 0.0;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j <= i; j++) {
      double error = fabs(L_parallel[i * N + j] - L_sequential[i * N + j]);
      if (error > max_error) max_error = error;
    }
  }

  const char *status = (max_error < TOLERANCE) ? "verification passed" : "verification FAILED";
  printf("Cholesky %s! (Max error: %.2e)\n", status, max_error);
  printf("Parallel time: %.4fs\nSequential time: %.4fs\n", parallel_time, sequential_time);
  printf("Result: %s\n", (max_error < TOLERANCE) ? "CORRECT" : "INCORRECT");
}

int main() {
  // Test with different sizes
  int sizes[] = {64, 128, 256};

  for (int i = 0; i < 3; i++) {
    printf("\n========================================\n");
    printf("Testing with N = %d\n", sizes[i]);
    printf("========================================\n");
    cholesky_decomposition(sizes[i]);
  }

  return 0;
}
