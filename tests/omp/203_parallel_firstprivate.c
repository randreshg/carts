/* 203: parallel with firstprivate -- initial value inherited from parent */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
  int A[64];
  int i;
  int base = 100;

  for (i = 0; i < 64; i++)
    A[i] = 0;

  int nthreads = 0;

#pragma omp parallel firstprivate(base) num_threads(8)
  {
    int tid = omp_get_thread_num();
#pragma omp single
    nthreads = omp_get_num_threads();

    /* base starts as 100 in each thread */
    base += tid;
    A[tid] = base; /* 100+tid */
  }

  long sum = 0;
  for (i = 0; i < nthreads; i++)
    sum += A[i];

  /* sum = sum(100+tid) = 100*n + n*(n-1)/2 */
  long expected = 100L * nthreads + (long)nthreads * (nthreads - 1) / 2;
  printf("sum = %ld (expected %ld)\n", sum, expected);

  /* verify base is unchanged in parent */
  if (base != 100) {
    printf("FAIL: base modified to %d\n", base);
    return 1;
  }

  return (sum != expected);
}
