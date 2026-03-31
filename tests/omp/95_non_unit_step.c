/* 95_non_unit_step.c - parallel for with stride 3 */
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
  for (i = 0; i < N; i += 3)
    A[i] = i;

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* i = 0,3,6,...,1023: last = 1023. count = 342 values */
  /* sum = 3*(0+1+2+...+341) = 3*341*342/2 = 174933 */
  long expected = 174933;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
