#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 32
#define TOLERANCE 1e-6

/// Task-based Cholesky decomposition
void cholesky_omp_task(double *A, int n) {
  #pragma omp parallel
  #pragma omp single
  {
    for (int k = 0; k < n; k += BLOCK_SIZE) {
      int kb = (k + BLOCK_SIZE > n) ? n - k : BLOCK_SIZE;

      // POTRF: Factor diagonal block
      #pragma omp task depend(inout : A[k * n + k])
      {
        for (int i = k; i < k + kb; i++) {
          for (int j = k; j < i; j++) {
            A[i * n + i] -= A[i * n + j] * A[i * n + j];
          }
          A[i * n + i] = sqrt(A[i * n + i]);

          for (int j = i + 1; j < k + kb; j++) {
            for (int m = k; m < i; m++) {
              A[j * n + i] -= A[j * n + m] * A[i * n + m];
            }
            A[j * n + i] /= A[i * n + i];
          }
        }
      }

      // TRSM: Solve triangular systems for submatrix
      for (int j = k + kb; j < n; j += BLOCK_SIZE) {
        int jb = (j + BLOCK_SIZE > n) ? n - j : BLOCK_SIZE;
        #pragma omp task depend(in : A[k * n + k])  \
                         depend(inout : A[j * n + k]) \
                         depend(inout : A[j * n + j])
        {
          for (int i = k; i < k + kb; i++) {
            for (int col = j; col < j + jb; col++) {
              for (int m = k; m < i; m++) {
                A[col * n + i] -= A[col * n + m] * A[i * n + m];
              }
              A[col * n + i] /= A[i * n + i];
            }
          }
        }
      }

      // SYRK: Update trailing submatrix
      for (int j = k + kb; j < n; j += BLOCK_SIZE) {
        int jb = (j + BLOCK_SIZE > n) ? n - j : BLOCK_SIZE;
        for (int i = j; i < n; i += BLOCK_SIZE) {
          int ib = (i + BLOCK_SIZE > n) ? n - i : BLOCK_SIZE;
          #pragma omp task depend(in : A[j * n + k], A[i * n + k]) \
                           depend(inout : A[i * n + j])
          {
            for (int col = j; col < j + jb; col++) {
              for (int row = i; row < i + ib; row++) {
                // Lower triangular only
                if (row >= col) { 
                  for (int m = k; m < k + kb; m++) {
                    A[row * n + col] -= A[row * n + m] * A[col * n + m];
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

// Sequential Cholesky for verification
void cholesky_sequential(double *A, int n) {
  for (int k = 0; k < n; k++) {
    A[k * n + k] = sqrt(A[k * n + k]);
    for (int i = k + 1; i < n; i++) {
      A[i * n + k] /= A[k * n + k];
    }
    for (int j = k + 1; j < n; j++) {
      for (int i = j; i < n; i++) {
        A[i * n + j] -= A[i * n + k] * A[j * n + k];
      }
    }
  }
}

// Verify decomposition by reconstructing matrix: L*L^T should equal original
int verify_cholesky(double *orig, double *L, int n) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j <= i; j++) { // Lower triangular
      double sum = 0.0;
      for (int k = 0; k <= j; k++) {
        sum += L[i * n + k] * L[j * n + k];
      }
      if (fabs(orig[i * n + j] - sum) > TOLERANCE) {
        printf("Mismatch at (%d,%d): Original=%.4f, Reconstructed=%.4f\n", i, j,
               orig[i * n + j], sum);
        return 0;
      }
    }
  }
  return 1;
}

int main() {
  int n = 512;
  double *A = (double *)malloc(n * n * sizeof(double));
  double *L_parallel = (double *)malloc(n * n * sizeof(double));
  double *L_sequential = (double *)malloc(n * n * sizeof(double));

  // Generate positive-definite matrix (A = L*L^T)
  for (int i = 0; i < n; i++) {
    for (int j = 0; j <= i; j++) {
      // Random lower triangular
      L_parallel[i * n + j] = (double)rand() / RAND_MAX;
      // Symmetric
      L_parallel[j * n + i] = L_parallel[i * n + j];
    }
    // Ensure diagonal dominance
    L_parallel[i * n + i] += n; 
  }
  // Compute A = L*L^T
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      A[i * n + j] = 0.0;
      for (int k = 0; k < n; k++) {
        A[i * n + j] += L_parallel[i * n + k] * L_parallel[j * n + k];
      }
    }
  }

  // Copy matrix for parallel and sequential versions
  memcpy(L_parallel, A, n * n * sizeof(double));
  memcpy(L_sequential, A, n * n * sizeof(double));

  // Run decompositions
  double start = omp_get_wtime();
  cholesky_omp_task(L_parallel, n);
  double parallel_time = omp_get_wtime() - start;

  start = omp_get_wtime();
  cholesky_sequential(L_sequential, n);
  double sequential_time = omp_get_wtime() - start;

  // Verify results
  if (verify_cholesky(A, L_parallel, n)) {
    printf("Parallel Cholesky verification passed!\n");
  }
  if (verify_cholesky(A, L_sequential, n)) {
    printf("Sequential Cholesky verification passed!\n");
  }

  printf("Parallel time: %.2fs\nSequential time: %.2fs\n", parallel_time,
         sequential_time);

  free(A);
  free(L_parallel);
  free(L_sequential);
  return 0;
}