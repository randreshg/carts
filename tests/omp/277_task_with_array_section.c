/* 277: tasks operating on array sections -- chunk-based processing */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 256;
  int *A = (int *)malloc(N * sizeof(int));
  int chunk = 16;
  int i;

  for (i = 0; i < N; i++)
    A[i] = 0;

#pragma omp parallel num_threads(4)
  {
#pragma omp single
    {
      for (int start = 0; start < N; start += chunk) {
        int end = start + chunk;
        if (end > N)
          end = N;
#pragma omp task firstprivate(start, end) shared(A)
        {
          for (int j = start; j < end; j++)
            A[j] = j + 1;
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
