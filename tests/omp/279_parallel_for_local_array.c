/* 279: parallel for with local (stack) array inside loop body */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));

#pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int local[4]; /* stack array, private to each iteration */
    local[0] = i;
    local[1] = i + 1;
    local[2] = i + 2;
    local[3] = i + 3;
    A[i] = local[0] + local[1] + local[2] + local[3]; /* 4i + 6 */
  }

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  /* A[i] = 4i + 6, sum = 4*N*(N-1)/2 + 6*N = 2*N*(N-1) + 6*N = 2*N*(N+2) */
  long expected = 2L * N * (N + 2);
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
