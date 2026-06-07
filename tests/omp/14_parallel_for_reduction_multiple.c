/* 14: reduction(+:s1,s2) two reduction variables */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; i++)
    A[i] = i;

  long s1 = 0;
  long s2 = 0;

#pragma omp parallel for reduction(+ : s1, s2)
  for (int i = 0; i < N; i++) {
    s1 += A[i];
    s2 += A[i] * A[i];
  }

  free(A);

  /* s1 = N*(N-1)/2 = 523776, s2 = N*(N-1)*(2N-1)/6 = 357389824 */
  printf("s1 = %ld (expected 523776)\n", s1);
  printf("s2 = %ld (expected 357389824)\n", s2);
  return (s1 != 523776) || (s2 != 357389824);
}
