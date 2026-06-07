/* 227: loop generating tasks -- many tasks from a for loop */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 256;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = 0;

#pragma omp parallel num_threads(4)
  {
#pragma omp single
    {
      for (int j = 0; j < N; j++) {
#pragma omp task firstprivate(j)
        {
          /* each task does a small computation */
          int val = 0;
          for (int k = 0; k <= j; k++)
            val += 1;
          A[j] = val; /* = j+1 */
        }
      }
#pragma omp taskwait
    }
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  long expected = (long)N * (N + 1) / 2;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
