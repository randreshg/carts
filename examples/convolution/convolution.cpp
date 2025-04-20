#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <omp.h>

#define BLOCK_SIZE 4
#define KERNEL_SIZE 2
#define TASK 4
#define TOTAL_SIZE_PER_ROW (BLOCK_SIZE * TASK)

int main(int argc, char **argv) {
  const int output_size = TOTAL_SIZE_PER_ROW - KERNEL_SIZE + 1;
  const int A_size = TOTAL_SIZE_PER_ROW;

  double A[TOTAL_SIZE_PER_ROW][TOTAL_SIZE_PER_ROW];
  double Kernel[KERNEL_SIZE][KERNEL_SIZE];
  double C_parallel[output_size][output_size];
  double C_serial[output_size][output_size];

  // Initialize matrices
  for (int i = 0; i < TOTAL_SIZE_PER_ROW; ++i) {
    for (int j = 0; j < TOTAL_SIZE_PER_ROW; ++j) {
      A[i][j] = 1.0;
    }
  }

  for (int i = 0; i < KERNEL_SIZE; ++i) {
    for (int j = 0; j < KERNEL_SIZE; ++j) {
      Kernel[i][j] = 1.0;
    }
  }

  #pragma omp parallel
  {
    #pragma omp single
    {
      for (int task_id = 0; task_id < TASK; ++task_id) {
        const int start_row = task_id * BLOCK_SIZE;
        printf("Task %d starting at row %d\n", task_id + 1, start_row);
        #pragma omp task firstprivate(start_row, task_id)
        {
          printf("Task %d running\n", task_id + 1);
          for (int i = start_row; i < start_row + BLOCK_SIZE && i < output_size;
               ++i) {
            for (int j = 0; j < output_size; ++j) {
              double tmp = 0.0;
              for (int k = 0; k < KERNEL_SIZE; ++k) {
                for (int l = 0; l < KERNEL_SIZE; ++l) {
                  const int A_row = i + k;
                  const int A_col = j + l;
                  if (A_row < A_size && A_col < A_size) {
                    tmp += A[A_row][A_col] * Kernel[k][l];
                  }
                }
              }
              C_parallel[i][j] = tmp;
            }
          }
        }
      }
    }
  }

  // Serial computation for verification
  for (int i = 0; i < output_size; ++i) {
    for (int j = 0; j < output_size; ++j) {
      double tmp = 0.0;
      for (int k = 0; k < KERNEL_SIZE; ++k) {
        for (int l = 0; l < KERNEL_SIZE; ++l) {
          const int A_row = i + k;
          const int A_col = j + l;
          if (A_row < A_size && A_col < A_size) {
            tmp += A[A_row][A_col] * Kernel[k][l];
          }
        }
      }
      C_serial[i][j] = tmp;
    }
  }

  // Verification
  int errors = 0;
  const double tolerance = 1e-6;
  for (int i = 0; i < output_size; ++i) {
    for (int j = 0; j < output_size; ++j) {
      if (fabs(C_parallel[i][j] - C_serial[i][j]) > tolerance) {
        errors++;
        printf("Mismatch at (%d,%d): Parallel=%.2f vs Serial=%.2f\n", i, j,
               C_parallel[i][j], C_serial[i][j]);
      }
    }
  }

  // Print results
  printf("Matrix A:\n");
  for (int i = 0; i < A_size; ++i) {
    for (int j = 0; j < A_size; ++j) {
      printf("%4.1f ", A[i][j]);
    }
    printf("\n");
  }
  printf("\n");

  printf("Matrix Kernel:\n");
  for (int i = 0; i < KERNEL_SIZE; ++i) {
    for (int j = 0; j < KERNEL_SIZE; ++j) {
      printf("%4.1f ", Kernel[i][j]);
    }
    printf("\n");
  }
  printf("\n");

  printf("Matrix C_parallel:\n");
  for (int i = 0; i < output_size; ++i) {
    for (int j = 0; j < output_size; ++j) {
      printf("%4.1f ", C_parallel[i][j]);
    }
    printf("\n");
  }
  printf("\n");

  printf("Matrix C_serial:\n");
  for (int i = 0; i < output_size; ++i) {
    for (int j = 0; j < output_size; ++j) {
      printf("%4.1f ", C_serial[i][j]);
    }
    printf("\n");
  }
  printf("\n");

  printf("Verification: %d errors\n", errors);
  // printf("Parallel time: %.6f sec\n", end_parallel - start_parallel);
  // printf("Serial time: %.6f sec\n", end_serial - start_serial);

  return errors > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}