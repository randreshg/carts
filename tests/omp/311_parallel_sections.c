/* 311: parallel sections */
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
    B[i] = 0;
  }

#pragma omp parallel sections num_threads(2)
  {
#pragma omp section
    {
      for (int j = 0; j < N; j++)
        A[j] = j + 1;
    }
#pragma omp section
    {
      for (int j = 0; j < N; j++)
        B[j] = (j + 1) * 2;
    }
  }

  long sumA = 0, sumB = 0;
  for (i = 0; i < N; i++) {
    sumA += A[i];
    sumB += B[i];
  }

  long expA = (long)N * (N + 1) / 2;
  long expB = (long)N * (N + 1);
  printf("sumA = %ld (expected %ld), sumB = %ld (expected %ld)\n", sumA, expA,
         sumB, expB);
  free(A);
  free(B);
  return (sumA != expA) || (sumB != expB);
}
