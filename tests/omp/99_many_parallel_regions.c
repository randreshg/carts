/* 99_many_parallel_regions.c - 10 consecutive parallel for regions */
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

#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;
#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] += 1;

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* Each element incremented 10 times: sum = 10 * 1024 = 10240 */
  long expected = 10240;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
