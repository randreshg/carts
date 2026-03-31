/* 272: parallel for reading from one array, writing to another (no alias) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *src = (int *)malloc(N * sizeof(int));
  int *dst = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++)
    src[i] = i + 1;

/* copy with transformation */
#pragma omp parallel for
  for (int i = 0; i < N; i++)
    dst[i] = src[i] * 2 + 1;

  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += dst[i];

  /* dst[i] = 2*(i+1)+1 = 2i+3, sum = 2*N*(N-1)/2 + 3*N = N*(N-1) + 3*N =
   * N*(N+2) */
  long expected = (long)N * (N + 2);
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(src);
  free(dst);
  return (sum != expected);
}
