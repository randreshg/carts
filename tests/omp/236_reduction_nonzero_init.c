/* 236: reduction with non-zero initial value */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int sum = 1000; /* non-zero initial value */

#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < N; i++)
    sum += i + 1;

  /* expected = 1000 + N*(N+1)/2 */
  int expected = 1000 + N * (N + 1) / 2;
  printf("sum = %d (expected %d)\n", sum, expected);
  return (sum != expected);
}
