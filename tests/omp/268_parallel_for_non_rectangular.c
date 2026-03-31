/* 268: triangular loop -- inner bound depends on outer index */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 128;
  /* lower triangular matrix */
  int *A = (int *)malloc(N * N * sizeof(int));
  int i, j;

  for (i = 0; i < N * N; i++)
    A[i] = 0;

#pragma omp parallel for private(j)
  for (i = 0; i < N; i++) {
    for (j = 0; j <= i; j++) {
      A[i * N + j] = i + j + 1;
    }
  }

  long sum = 0;
  for (i = 0; i < N * N; i++)
    sum += A[i];

  /* sum of (i+j+1) for 0<=j<=i<N */
  /* = sum_i( sum_j=0..i (i+j+1) ) = sum_i( (i+1)*(i+1) + i*(i+1)/2 ) */
  /* = sum_i( (i+1)*(i+1) + i*(i+1)/2 ) */
  /* = sum_i( (i+1)*((i+1) + i/2) ) = sum_i( (i+1)*(3i+2)/2 ) */
  /* compute expected serially */
  long expected = 0;
  for (i = 0; i < N; i++)
    for (j = 0; j <= i; j++)
      expected += i + j + 1;

  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
