/* 06: schedule(static, 64) with explicit chunk size */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));

#pragma omp parallel for schedule(static, 64)
  for (int i = 0; i < N; i++) {
    A[i] = i + 10;
  }

  /* sum = sum(i+10, i=0..1023) = 523776 + 10240 = 534016 */
  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  printf("sum = %ld (expected 534016)\n", sum);
  free(A);
  return (sum != 534016);
}
