/* 15: 1D 3-point stencil A[i] = (B[i-1]+B[i]+B[i+1])/3 */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++)
    B[i] = i * 3; /* divisible by 3 for integer arithmetic */

#pragma omp parallel for
  for (int i = 1; i < N - 1; i++) {
    A[i] = (B[i - 1] + B[i] + B[i + 1]) / 3;
  }

  /* boundary */
  A[0] = B[0];
  A[N - 1] = B[N - 1];

  /* A[i] = i*3 for all i, sum = 3*sum(i,0..1023) = 3*523776 = 1571328 */
  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  printf("sum = %ld (expected 1571328)\n", sum);
  free(A);
  free(B);
  return (sum != 1571328);
}
