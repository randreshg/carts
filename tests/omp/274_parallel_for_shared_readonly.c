/* 274: parallel for with shared read-only array and private write array */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *coeff = (int *)malloc(N * sizeof(int));
  int *result = (int *)malloc(N * sizeof(int));

  /* initialize read-only coefficients */
  for (int i = 0; i < N; i++)
    coeff[i] = i % 10 + 1;

/* parallel for: read coeff, write result */
#pragma omp parallel for
  for (int i = 0; i < N; i++)
    result[i] = coeff[i] * (i + 1);

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += result[i];

  /* compute expected serially */
  long expected = 0;
  for (int i = 0; i < N; i++)
    expected += (long)(i % 10 + 1) * (i + 1);

  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(coeff);
  free(result);
  return (sum != expected);
}
