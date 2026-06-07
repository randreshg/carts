/* 242: task inside parallel for iteration -- each iteration spawns a task */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 64;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = 0;

#pragma omp parallel num_threads(4)
  {
#pragma omp for
    for (int j = 0; j < N; j++) {
#pragma omp task firstprivate(j) shared(A)
      {
        A[j] = j * j;
      }
    }
/* implicit barrier at end of for -- but tasks may not be done */
#pragma omp taskwait
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* sum of j^2, j=0..N-1 = N*(N-1)*(2N-1)/6 */
  long expected = (long)N * (N - 1) * (2 * N - 1) / 6;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
