/* 231: reduction with multiplication */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  /* keep N small to avoid overflow: 12! = 479001600 fits in int */
  int N = 12;
  long product = 1;

#pragma omp parallel for reduction(* : product)
  for (int i = 1; i <= N; i++)
    product *= i;

  /* N! = 479001600 */
  long expected = 479001600L;
  printf("product = %ld (expected %ld)\n", product, expected);
  return (product != expected);
}
