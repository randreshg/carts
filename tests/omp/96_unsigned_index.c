/* 96_unsigned_index.c - parallel for with unsigned loop variable */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  unsigned N = (argc > 1) ? (unsigned)atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  unsigned i;

#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] = (int)(i * 2);

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* sum = 2*(0+1+...+1023) = 1047552 */
  long expected = 1047552;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
