/* 102_jacobi_2d.c - 5-point Jacobi stencil */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 64;
  int T = 10;
  double *A = (double *)malloc(N * N * sizeof(double));
  double *B = (double *)malloc(N * N * sizeof(double));
  int i, j, t;

  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++) {
      A[i * N + j] = (double)((i + j) % 100);
      B[i * N + j] = 0.0;
    }

  for (t = 0; t < T; t++) {
#pragma omp parallel for
    for (i = 1; i < N - 1; i++)
      for (j = 1; j < N - 1; j++)
        B[i * N + j] =
            0.2 * (A[i * N + j] + A[(i - 1) * N + j] + A[(i + 1) * N + j] +
                   A[i * N + (j - 1)] + A[i * N + (j + 1)]);

    /* Swap A and B */
    double *tmp = A;
    A = B;
    B = tmp;
  }

  double checksum = 0.0;
  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++)
      checksum += A[i * N + j];

  printf("checksum = %f\n", checksum);
  free(A);
  free(B);
  return 0;
}
