/* 02: private variable inside parallel for */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int tmp = 0;

#pragma omp parallel for private(tmp)
  for (int i = 0; i < N; i++) {
    tmp = i * 3;
    tmp = tmp + 1;
    A[i] = tmp;
  }

  /* sum = sum(i*3+1, i=0..1023) = 3*523776 + 1024 = 1572352 */
  long sum = 0;
  for (int i = 0; i < N; i++)
    sum += A[i];

  printf("sum = %ld (expected 1572352)\n", sum);
  free(A);
  return (sum != 1572352);
}
