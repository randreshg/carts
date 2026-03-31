/* 212: parallel with code after for -- all threads execute post-for code */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int done[16];
  int i;

  for (i = 0; i < 16; i++)
    done[i] = 0;

  int nthreads = 0;

#pragma omp parallel num_threads(4)
  {
    int tid = omp_get_thread_num();
#pragma omp single
    nthreads = omp_get_num_threads();

#pragma omp for
    for (int j = 0; j < N; j++)
      A[j] = j + 1;

    /* code after for: implicit barrier at end of for, then all run this */
    done[tid] = 1;
  }

  int done_count = 0;
  for (i = 0; i < nthreads; i++)
    done_count += done[i];

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  long expected = (long)N * (N + 1) / 2;
  int ok = (sum == expected) && (done_count == nthreads);
  printf("sum = %ld (expected %ld), done = %d/%d\n", sum, expected, done_count,
         nthreads);
  return ok ? 0 : 1;
}
