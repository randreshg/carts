/* 243: taskloop with reduction */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = i + 1;

  long sum = 0;

#pragma omp parallel num_threads(4)
  {
#pragma omp single
    {
#pragma omp taskloop reduction(+ : sum)
      for (i = 0; i < N; i++)
        sum += A[i];
    }
  }

  long expected = (long)N * (N + 1) / 2;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
