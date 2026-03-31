/* 251: zero iteration parallel for -- N=0 */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = 0;
  int sum = 42;

#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < N; i++)
    sum += i;

  /* no iterations, sum stays at 42 */
  printf("sum = %d (expected 42)\n", sum);
  return (sum != 42);
}
