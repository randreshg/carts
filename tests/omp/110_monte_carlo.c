/* 110_monte_carlo.c - parallel Monte Carlo pi estimation using reduction */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1048576;
  long count = 0;
  int i;

#pragma omp parallel for reduction(+ : count)
  for (i = 0; i < N; i++) {
    /* Deterministic pseudo-random using LCG per iteration */
    unsigned int seed = (unsigned int)i * 1103515245u + 12345u;
    seed = seed * 1103515245u + 12345u;
    double x = (double)(seed & 0x7FFFFFFF) / (double)0x7FFFFFFF;
    seed = seed * 1103515245u + 12345u;
    double y = (double)(seed & 0x7FFFFFFF) / (double)0x7FFFFFFF;
    if (x * x + y * y <= 1.0)
      count++;
  }

  double pi_est = 4.0 * (double)count / (double)N;
  double diff = pi_est - 3.14159265358979;
  if (diff < 0)
    diff = -diff;

  printf("pi ~ %f (count=%ld, N=%d)\n", pi_est, count, N);
  /* Allow generous tolerance since it is pseudo-random */
  int ok = (diff < 0.1);
  printf("ok = %d (diff = %f)\n", ok, diff);
  return !ok;
}
