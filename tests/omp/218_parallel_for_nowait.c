/* 218: parallel with for nowait -- no barrier between consecutive fors */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));

#pragma omp parallel num_threads(4)
  {
/* nowait: threads proceed to second for without waiting */
/* safe because A and B are independent */
#pragma omp for nowait
    for (int i = 0; i < N; i++)
      A[i] = i + 1;

#pragma omp for
    for (int i = 0; i < N; i++)
      B[i] = i * 2;
  }

  long sumA = 0, sumB = 0;
  for (int i = 0; i < N; i++) {
    sumA += A[i];
    sumB += B[i];
  }

  long expA = (long)N * (N + 1) / 2;
  long expB = (long)N * (N - 1);
  printf("sumA = %ld (expected %ld), sumB = %ld (expected %ld)\n", sumA, expA,
         sumB, expB);
  free(A);
  free(B);
  return (sumA != expA) || (sumB != expB);
}
