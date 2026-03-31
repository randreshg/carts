/* 104_fdtd_2d.c - 2D FDTD electromagnetic field update */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 64;
  int T = 10;
  double *Ex = (double *)malloc(N * N * sizeof(double));
  double *Ey = (double *)malloc(N * N * sizeof(double));
  double *Hz = (double *)malloc(N * N * sizeof(double));
  int i, j, t;

  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++) {
      Ex[i * N + j] = 0.0;
      Ey[i * N + j] = 0.0;
      Hz[i * N + j] = (double)((i + j) % 50) * 0.01;
    }

  for (t = 0; t < T; t++) {
/* Update Ey */
#pragma omp parallel for
    for (i = 1; i < N; i++)
      for (j = 0; j < N; j++)
        Ey[i * N + j] -= 0.5 * (Hz[i * N + j] - Hz[(i - 1) * N + j]);

/* Update Ex */
#pragma omp parallel for
    for (i = 0; i < N; i++)
      for (j = 1; j < N; j++)
        Ex[i * N + j] -= 0.5 * (Hz[i * N + j] - Hz[i * N + (j - 1)]);

/* Update Hz */
#pragma omp parallel for
    for (i = 0; i < N - 1; i++)
      for (j = 0; j < N - 1; j++)
        Hz[i * N + j] -= 0.7 * (Ex[i * N + (j + 1)] - Ex[i * N + j] +
                                Ey[(i + 1) * N + j] - Ey[i * N + j]);
  }

  double checksum = 0.0;
  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++)
      checksum += Ex[i * N + j] + Ey[i * N + j] + Hz[i * N + j];

  printf("checksum = %f\n", checksum);
  free(Ex);
  free(Ey);
  free(Hz);
  return 0;
}
