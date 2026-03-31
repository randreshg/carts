/* 263: parallel for with collapse(2) -- 2D loop collapsing */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 64;
  int *A = (int *)malloc(N * N * sizeof(int));

#pragma omp parallel for collapse(2)
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      A[i * N + j] = i + j;

  long sum = 0;
  for (int i = 0; i < N * N; i++)
    sum += A[i];

  /* sum of (i+j) for i=0..N-1, j=0..N-1 = N*N*(N-1) */
  long expected = (long)N * N * (N - 1);
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
