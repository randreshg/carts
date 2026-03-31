/* 82_task_spawns_parallel.c - task containing a parallel for region */
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
#pragma omp task
      {
#pragma omp parallel for
        for (i = 0; i < N; i++)
          A[i] = i + 1;
      }
#pragma omp taskwait
    }
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* sum = 1+2+...+1024 = 1024*1025/2 = 524800 */
  long expected = 524800;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
