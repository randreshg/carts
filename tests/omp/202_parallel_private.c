/* 202: parallel with private variable -- each thread computes locally */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
  int A[64];
  int i;
  int tmp = 999; /* should NOT be visible inside parallel as private */

  for (i = 0; i < 64; i++)
    A[i] = 0;

  int nthreads = 0;

#pragma omp parallel private(tmp) num_threads(8)
  {
    int tid = omp_get_thread_num();
#pragma omp single
    nthreads = omp_get_num_threads();

    /* tmp is private -- uninitialized per spec, we assign it */
    tmp = tid * 3 + 1;
    A[tid] = tmp;
  }

  long sum = 0;
  for (i = 0; i < nthreads; i++)
    sum += A[i];

  /* A[tid] = 3*tid+1, sum = 3*(0+1+...+(n-1)) + n = 3*n*(n-1)/2 + n */
  long expected = 3L * nthreads * (nthreads - 1) / 2 + nthreads;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  return (sum != expected);
}
