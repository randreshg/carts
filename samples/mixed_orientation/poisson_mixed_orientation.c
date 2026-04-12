#include <stdlib.h>

#define NX 64
#define NY 64

static void rhs(int nx, int ny, double **f) {
  int i, j;

#pragma omp parallel for private(i)
  for (j = 0; j < ny; ++j) {
    for (i = 0; i < nx; ++i) {
      f[i][j] = (double)(i + j);
    }
  }
}

static void sweep(int nx, int ny, double **f, double **u, double **unew) {
  int i, j;

#pragma omp parallel for private(j)
  for (i = 0; i < nx; ++i) {
    for (j = 0; j < ny; ++j) {
      u[i][j] = unew[i][j];
    }
  }

#pragma omp parallel for private(j)
  for (i = 0; i < nx; ++i) {
    for (j = 0; j < ny; ++j) {
      if (i == 0 || j == 0 || i == nx - 1 || j == ny - 1) {
        unew[i][j] = f[i][j];
      } else {
        unew[i][j] = 0.25 * (u[i - 1][j] + u[i][j + 1] + u[i][j - 1] +
                             u[i + 1][j] + f[i][j]);
      }
    }
  }
}

int main(void) {
  double **f = (double **)malloc(NX * sizeof(double *));
  double **u = (double **)malloc(NX * sizeof(double *));
  double **unew = (double **)malloc(NX * sizeof(double *));

  for (int i = 0; i < NX; ++i) {
    f[i] = (double *)malloc(NY * sizeof(double));
    u[i] = (double *)malloc(NY * sizeof(double));
    unew[i] = (double *)malloc(NY * sizeof(double));
  }

  rhs(NX, NY, f);
  sweep(NX, NY, f, u, unew);

  for (int i = 0; i < NX; ++i) {
    free(f[i]);
    free(u[i]);
    free(unew[i]);
  }
  free(f);
  free(u);
  free(unew);

  return 0;
}
