/* 282: parallel for with malloc/free inside loop body */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 256;
  int M = 16;
  long *results = (long *)malloc(N * sizeof(long));

#pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int *tmp = (int *)malloc(M * sizeof(int));
    for (int j = 0; j < M; j++)
      tmp[j] = i + j;

    long s = 0;
    for (int j = 0; j < M; j++)
      s += tmp[j];
    results[i] = s;
    free(tmp);
  }

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += results[i];

  /* results[i] = sum(i+j, j=0..M-1) = M*i + M*(M-1)/2 */
  /* total = M*N*(N-1)/2 + N*M*(M-1)/2 = M*N*(N-1+M-1)/2 = M*N*(N+M-2)/2 */
  long expected = (long)M * N * (N + M - 2) / 2;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(results);
  return (sum != expected);
}
