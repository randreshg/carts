/* 107_stream_triad.c - A[i] = B[i] + s*C[i] */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  double *A = (double *)malloc(N * sizeof(double));
  double *B = (double *)malloc(N * sizeof(double));
  double *C = (double *)malloc(N * sizeof(double));
  double s = 3.0;
  int i;

  for (i = 0; i < N; i++) {
    B[i] = (double)(i + 1);
    C[i] = (double)(i * 2);
  }

#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] = B[i] + s * C[i];

  double sum = 0.0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* A[i] = (i+1) + 3*(2i) = 7i + 1, sum = 7*1023*1024/2 + 1024 = 3666432 + 1024
   * = 3667456 */
  double expected = 3667456.0;
  printf("sum = %f (expected %f)\n", sum, expected);
  free(A);
  free(B);
  free(C);
  double diff = sum - expected;
  if (diff < 0)
    diff = -diff;
  return (diff > 0.1);
}
