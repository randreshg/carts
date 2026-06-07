/* 83_sequential_parallel_regions.c - three parallel for regions with serial
 * code between */
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

#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] = i;

  /* Serial section */
  int scale = 3;

#pragma omp parallel for
  for (i = 0; i < N; i++)
    B[i] = A[i] * scale;

  /* Another serial section */
  int offset = 10;

#pragma omp parallel for
  for (i = 0; i < N; i++)
    C[i] = B[i] + offset;

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += C[i];

  /* C[i] = 3*i + 10, sum = 3*1023*1024/2 + 10*1024 = 1571328 + 10240 = 1581568
   */
  long expected = 1581568;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  free(C);
  return (sum != expected);
}
