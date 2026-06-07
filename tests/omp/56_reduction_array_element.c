/* 56_reduction_array_element.c - reduction(+:sum) where sum=A[0] */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = i + 1;

  int sum = 0;

#pragma omp parallel for reduction(+ : sum)
  for (i = 0; i < N; i++)
    sum += A[i];

  A[0] = sum;

  int expected = N * (N + 1) / 2;
  printf("A[0] = %d (expected %d)\n", A[0], expected);
  int ok = (A[0] == expected);
  free(A);

  return ok ? 0 : 1;
}
