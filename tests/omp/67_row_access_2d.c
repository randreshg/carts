/* 67_row_access_2d.c - for i: for j: C[i*N+j] = ... (row-major) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 64;
  int *A = (int *)malloc(N * N * sizeof(int));
  int *B = (int *)malloc(N * N * sizeof(int));
  int *C = (int *)malloc(N * N * sizeof(int));
  int i, j;

  for (i = 0; i < N * N; i++) {
    A[i] = i;
    B[i] = i * 2;
    C[i] = 0;
  }

#pragma omp parallel for private(j)
  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++)
      C[i * N + j] = A[i * N + j] + B[i * N + j];

  int pass = 1;
  for (i = 0; i < N * N; i++) {
    if (C[i] != i + i * 2) {
      pass = 0;
      break;
    }
  }

  printf("%s\n", pass ? "PASS" : "FAIL");
  free(A);
  free(B);
  free(C);

  return pass ? 0 : 1;
}
