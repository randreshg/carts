/* 73_conditional_write.c - parallel for with conditional write */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++) {
    A[i] = 0;
    B[i] = (i % 2 == 0) ? i : -i;
  }

#pragma omp parallel for
  for (i = 0; i < N; i++) {
    if (B[i] > 0)
      A[i] = B[i];
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* B[i]>0 when i is even and i>0: i=2,4,...,1022 */
  /* sum = 2+4+...+1022 = 2*(1+2+...+511) = 2*511*512/2 = 261632 */
  long expected = 261632;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  return (sum != expected);
}
