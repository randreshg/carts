/* 278: parallel for with pointer arithmetic in body */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++)
    B[i] = i * 2 + 1;

#pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int *p = B + i;
    A[i] = *p + 1;
  }

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  /* A[i] = B[i] + 1 = 2i + 2, sum = 2*N*(N-1)/2 + 2*N = N*(N+1) */
  long expected = (long)N * (N + 1);
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  return (sum != expected);
}
