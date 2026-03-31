/* 281: parallel for with double (floating point) array */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  double *A = (double *)malloc(N * sizeof(double));

#pragma omp parallel for
  for (int i = 0; i < N; i++)
    A[i] = (double)(i + 1) * 0.5;

  double sum = 0.0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  /* sum = 0.5 * (1+2+...+N) = 0.5 * N*(N+1)/2 = N*(N+1)/4.0 */
  double expected = (double)N * (N + 1) / 4.0;
  double diff = sum - expected;
  if (diff < 0)
    diff = -diff;

  printf("sum = %f (expected %f)\n", sum, expected);
  free(A);
  return (diff > 0.001) ? 1 : 0;
}
