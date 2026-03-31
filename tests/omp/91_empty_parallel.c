/* 91_empty_parallel.c - empty parallel region */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int x = 42;

#pragma omp parallel
  {
  }

  printf("x = %d (expected 42)\n", x);
  return (x != 42);
}
