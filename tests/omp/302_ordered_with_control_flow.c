/* 302: ordered region with control flow inside (tests ExecuteRegionOp wrapper)
 */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 32;
  int *A = (int *)malloc(N * sizeof(int));
  int pos = 0;

#pragma omp parallel for ordered schedule(static, 1) num_threads(4)
  for (int i = 0; i < N; i++) {
#pragma omp ordered
    {
      /* control flow inside ordered region to test ExecuteRegionOp */
      if (i % 2 == 0)
        A[pos] = i * 10;
      else
        A[pos] = i * 10 + 1;
      pos++;
    }
  }

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  /* sum = sum of (i*10) for even i + sum of (i*10+1) for odd i */
  long expected = 0;
  for (int i = 0; i < N; i++)
    expected += (i % 2 == 0) ? i * 10 : i * 10 + 1;

  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  return (sum != expected);
}
