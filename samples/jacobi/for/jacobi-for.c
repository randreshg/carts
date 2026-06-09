/*
 * CARTS Jacobi-For Test Example
 *
 * Original: https://github.com/viroulep/kastors
 * License: GNU LGPL
 * Author: John Burkardt (modified by KaStORS team)
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "carts/utils/testing/CartsTest.h"

static void sweep_seq(int nx, int ny, double dx, double dy, double **f,
                      int itold, int itnew, double **u, double **unew) {
  for (int it = itold + 1; it <= itnew; it++) {
    for (int i = 0; i < nx; i++) {
      for (int j = 0; j < ny; j++)
        u[i][j] = unew[i][j];
    }

    for (int i = 0; i < nx; i++) {
      for (int j = 0; j < ny; j++) {
        if (i == 0 || j == 0 || i == nx - 1 || j == ny - 1) {
          unew[i][j] = f[i][j];
        } else {
          unew[i][j] = 0.25 * (u[i - 1][j] + u[i][j + 1] + u[i][j - 1] +
                               u[i + 1][j] + f[i][j] * dx * dy);
        }
      }
    }
  }
}

/// Parallel version with #pragma omp parallel for
static void sweep(int nx, int ny, double dx, double dy, double **f, int itold,
                  int itnew, double **u, double **unew, int block_size) {
  int i, j, it;

  for (it = itold + 1; it <= itnew; it++) {
#pragma omp parallel for private(j)
    for (i = 0; i < nx; i++) {
      for (j = 0; j < ny; j++)
        u[i][j] = unew[i][j];
    }

#pragma omp parallel for private(j)
    for (i = 0; i < nx; i++) {
      for (j = 0; j < ny; j++) {
        if (i == 0 || j == 0 || i == nx - 1 || j == ny - 1) {
          unew[i][j] = f[i][j];
        } else {
          unew[i][j] = 0.25 * (u[i - 1][j] + u[i][j + 1] + u[i][j - 1] +
                               u[i + 1][j] + f[i][j] * dx * dy);
        }
      }
    }
  }
}

int main(void) {
  CARTS_TIMER_START();

#ifdef SIZE
  int nx = SIZE, ny = SIZE;
#else
  int nx = 100, ny = 100;
#endif
  int itold = 0, itnew = 10;
  int block_size = 10;
  double dx = 1.0 / (nx - 1);
  double dy = 1.0 / (ny - 1);

  printf("Jacobi-For Test: %d x %d grid, %d iterations\n", nx, ny, itnew);
  double **f = (double **)malloc(nx * sizeof(double *));
  double **u = (double **)malloc(nx * sizeof(double *));
  double **unew = (double **)malloc(nx * sizeof(double *));
  double **unew_seq = (double **)malloc(nx * sizeof(double *));

  for (int i = 0; i < nx; i++) {
    f[i] = (double *)malloc(ny * sizeof(double));
    u[i] = (double *)malloc(ny * sizeof(double));
    unew[i] = (double *)malloc(ny * sizeof(double));
    unew_seq[i] = (double *)malloc(ny * sizeof(double));
  }

  for (int i = 0; i < nx; i++) {
    for (int j = 0; j < ny; j++) {
      f[i][j] = ((i + j) % 17 + 1) * 0.01;
      u[i][j] = 0.0;
      unew[i][j] = 0.0;
      unew_seq[i][j] = 0.0;
    }
  }

  printf("Running sequential version for verification...\n");
  sweep_seq(nx, ny, dx, dy, f, itold, itnew, u, unew_seq);

  for (int i = 0; i < nx; i++) {
    for (int j = 0; j < ny; j++) {
      u[i][j] = 0.0;
      unew[i][j] = 0.0;
    }
  }

  printf("Running parallel version with #pragma omp parallel for...\n");
  sweep(nx, ny, dx, dy, f, itold, itnew, u, unew, block_size);

  double error = 0.0;
  double max_error = 0.0;
  for (int i = 0; i < nx; i++) {
    for (int j = 0; j < ny; j++) {
      double diff = unew_seq[i][j] - unew[i][j];
      error += diff * diff;
      if (fabs(diff) > max_error)
        max_error = fabs(diff);
    }
  }
  error = sqrt(error / (nx * ny));

  printf("RMS error: %e\n", error);
  printf("Max error: %e\n", max_error);

  for (int i = 0; i < nx; i++) {
    free(f[i]);
    free(u[i]);
    free(unew[i]);
    free(unew_seq[i]);
  }
  free(f);
  free(u);
  free(unew);
  free(unew_seq);

  if (error < 1e-6)
    CARTS_TEST_PASS();
  else
    CARTS_TEST_FAIL("jacobi-for verification failed");
}
