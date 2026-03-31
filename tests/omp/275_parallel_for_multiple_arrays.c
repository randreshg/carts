/* 275: parallel for operating on 4 arrays simultaneously */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  int *C = (int *)malloc(N * sizeof(int));
  int *D = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++) {
    A[i] = i;
    B[i] = N - i;
  }

/* D = A*B + C where C = A + B */
#pragma omp parallel for
  for (int i = 0; i < N; i++) {
    C[i] = A[i] + B[i];        /* = N */
    D[i] = A[i] * B[i] + C[i]; /* = i*(N-i) + N */
  }

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += D[i];

  /* sum = sum(i*(N-i) + N) = N*sum(i) - sum(i^2) + N^2 */
  /* = N*N*(N-1)/2 - N*(N-1)*(2N-1)/6 + N^2 */
  long expected = 0;
  for (int i = 0; i < N; i++)
    expected += (long)i * (N - i) + N;

  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  free(C);
  free(D);
  return (sum != expected);
}
