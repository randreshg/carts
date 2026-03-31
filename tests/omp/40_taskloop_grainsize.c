/* 40_taskloop_grainsize.c - taskloop grainsize(100) */
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

#pragma omp parallel
  {
#pragma omp single
    {
#pragma omp taskloop grainsize(100)
      for (i = 0; i < N; i++)
        A[i] = i * 2;
    }
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  long expected = (long)N * (long)(N - 1);
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);

  return (sum == expected) ? 0 : 1;
}
