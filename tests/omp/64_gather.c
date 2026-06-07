/* 64_gather.c - A[i] = B[idx[i]] with precomputed idx */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  int *idx = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++) {
    B[i] = i * 5;
    idx[i] = (i * 7 + 3) % N;
  }

#pragma omp parallel for
  for (i = 0; i < N; i++)
    A[i] = B[idx[i]];

  int pass = 1;
  for (i = 0; i < N; i++) {
    if (A[i] != B[idx[i]]) {
      pass = 0;
      break;
    }
  }

  printf("%s\n", pass ? "PASS" : "FAIL");
  free(A);
  free(B);
  free(idx);

  return pass ? 0 : 1;
}
