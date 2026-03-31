/* 310: nowait clause on parallel for */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  int i;

#pragma omp parallel num_threads(4)
  {
#pragma omp for nowait
    for (i = 0; i < N; i++)
      A[i] = i;

#pragma omp for
    for (i = 0; i < N; i++)
      B[i] = A[i] * 2;
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += B[i];

  long expected = (long)N * (N - 1);
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  return (sum != expected);
}
