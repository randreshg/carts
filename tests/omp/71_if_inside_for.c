/* 71_if_inside_for.c - parallel for with if/else inside loop body */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

#pragma omp parallel for
  for (i = 0; i < N; i++) {
    if (i % 2 == 0)
      A[i] = 1;
    else
      A[i] = -1;
  }

  int sum = 0;
  for (i = 0; i < N; i++)
    sum += A[i];

  /* 512 * 1 + 512 * (-1) = 0 */
  printf("sum = %d (expected 0)\n", sum);
  free(A);
  return (sum != 0);
}
