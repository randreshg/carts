/* 37_task_priority.c - task priority(10) { body } */
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
#pragma omp task priority(10)
      {
        int j;
        for (j = 0; j < N / 2; j++)
          A[j] = 1;
      }

#pragma omp task priority(1)
      {
        int j;
        for (j = N / 2; j < N; j++)
          A[j] = 2;
      }

#pragma omp taskwait
    }
  }

  int sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  int expected = N / 2 * 1 + N / 2 * 2;
  printf("sum = %d (expected %d)\n", sum, expected);
  free(A);

  return (sum == expected) ? 0 : 1;
}
