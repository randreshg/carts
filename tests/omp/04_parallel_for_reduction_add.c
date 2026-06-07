/* 04: reduction(+:sum) over array */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++)
    A[i] = i;

  long sum = 0;

#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < N; i++) {
    sum += A[i];
  }

  free(A);

  /* expected: N*(N-1)/2 = 1024*1023/2 = 523776 */
  printf("sum = %ld (expected 523776)\n", sum);
  return (sum != 523776);
}
