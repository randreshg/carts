/* 276: parallel for where scalar computed before region is used inside */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));

  /* scalars computed before parallel region */
  int alpha = N / 4;
  int beta = N / 2;
  int gamma = alpha + beta;

#pragma omp parallel for
  for (int i = 0; i < N; i++)
    A[i] = alpha * i + beta - gamma;

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  /* A[i] = alpha*i + beta - gamma = alpha*i + beta - (alpha+beta) = alpha*i -
     alpha = alpha*(i-1) */
  /* sum = alpha * (sum(i) - N) = alpha * (N*(N-1)/2 - N) = alpha * N * (N-3) /
   * 2 */
  long expected = 0;
  for (int i = 0; i < N; i++)
    expected += (long)alpha * i + beta - gamma;

  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
