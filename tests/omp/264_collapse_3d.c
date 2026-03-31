/* 264: parallel for with collapse(3) -- 3D loop collapsing */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 16;
  int *A = (int *)malloc(N * N * N * sizeof(int));

#pragma omp parallel for collapse(3)
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
      for (int k = 0; k < N; k++)
        A[i * N * N + j * N + k] = i + j + k;

  long sum = 0;
  for (int idx = 0; idx < N * N * N; idx++)
    sum += A[idx];

  /* sum of (i+j+k) = 3 * N^2 * N*(N-1)/2 = 3*N^3*(N-1)/2 */
  long expected = 3L * N * N * N * (N - 1) / 2;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
