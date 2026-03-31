/* 262: parallel for with lastprivate -- value from last iteration */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int last_val = -1;

#pragma omp parallel for lastprivate(last_val)
  for (int i = 0; i < N; i++)
    last_val = i * 3;

  /* last iteration: i=N-1, last_val = (N-1)*3 */
  int expected = (N - 1) * 3;
  printf("last_val = %d (expected %d)\n", last_val, expected);
  return (last_val != expected);
}
