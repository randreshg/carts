/* 76_ternary.c - parallel for with ternary operator */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  int *C = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++) {
    B[i] = i * 2;
    C[i] = i * 3;
  }

#pragma omp parallel for
  for (i = 0; i < N; i++) {
    A[i] = (i > N / 2) ? B[i] : C[i];
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* i <= N/2: A[i] = 3*i; i > N/2: A[i] = 2*i */
  int h = N / 2;
  long sum_lo = 3L * (long)h * (long)(h + 1) / 2; /* sum(3*i, i=0..h) */
  long sum_hi = 0;
  for (i = h + 1; i < N; i++)
    sum_hi += 2L * i;
  long expected = sum_lo + sum_hi;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  free(C);
  return (sum != expected);
}
