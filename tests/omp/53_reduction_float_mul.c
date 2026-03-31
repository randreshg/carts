/* 53_reduction_float_mul.c - float product reduction(*:prod) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 20;
  float *A = (float *)malloc(N * sizeof(float));
  int i;

  for (i = 0; i < N; i++)
    A[i] = 1.0f + 0.01f * (float)i;

  float prod = 1.0f;

#pragma omp parallel for reduction(* : prod)
  for (i = 0; i < N; i++)
    prod *= A[i];

  /* Compute expected serially */
  float expected = 1.0f;
  for (i = 0; i < N; i++)
    expected *= A[i];

  float diff = prod - expected;
  if (diff < 0)
    diff = -diff;

  printf("prod = %f (expected %f)\n", (double)prod, (double)expected);
  free(A);

  return (diff < 0.001f) ? 0 : 1;
}
