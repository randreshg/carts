/* 269: multiple parallel regions sharing data -- verify data persists */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));

/* region 1: initialize */
#pragma omp parallel for
  for (int i = 0; i < N; i++)
    A[i] = i;

/* region 2: double each element */
#pragma omp parallel for
  for (int i = 0; i < N; i++)
    A[i] *= 2;

/* region 3: add index */
#pragma omp parallel for
  for (int i = 0; i < N; i++)
    A[i] += i;

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  /* A[i] = 2*i + i = 3*i, sum = 3*N*(N-1)/2 */
  long expected = 3L * N * (N - 1) / 2;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
