/* 258: parallel for with continue in body -- skip even indices */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = 0;

#pragma omp parallel for
  for (i = 0; i < N; i++) {
    if (i % 2 == 0)
      continue;
    A[i] = i;
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* odd indices: 1,3,5,...,N-1 */
  /* count = N/2, sum = N/2 * (1 + N-1) / 2 = N/2 * N/2 = N^2/4 */
  long expected = (long)N * N / 4;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
