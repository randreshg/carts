/* 204: parallel with thread-dependent computation using omp_get_thread_num */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = 0;

/* each thread fills a chunk of A using its thread id */
#pragma omp parallel num_threads(4)
  {
    int tid = omp_get_thread_num();
    int nt = omp_get_num_threads();
    int chunk = N / nt;
    int start = tid * chunk;
    int end = (tid == nt - 1) ? N : start + chunk;

    for (int j = start; j < end; j++)
      A[j] = tid + 1;
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* with 4 threads, chunk=256: 256*1 + 256*2 + 256*3 + 256*4 = 256*10 = 2560 */
  /* general: each thread tid gets ~N/nt elements, value=tid+1 */
  /* hard to compute exactly for arbitrary N/nt, verify at N=1024,nt=4 */
  long expected = 2560;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
