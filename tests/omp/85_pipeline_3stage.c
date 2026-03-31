/* 85_pipeline_3stage.c - 3 tasks chained via depend: stage1->stage2->stage3 */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  int *C = (int *)malloc(N * sizeof(int));
  int i;

#pragma omp parallel
  {
#pragma omp single
    {
/* Stage 1: initialize A */
#pragma omp task depend(out : A[0])
      {
        int j;
        for (j = 0; j < N; j++)
          A[j] = j;
      }

/* Stage 2: A -> B (double) */
#pragma omp task depend(in : A[0]) depend(out : B[0])
      {
        int j;
        for (j = 0; j < N; j++)
          B[j] = A[j] * 2;
      }

/* Stage 3: B -> C (add 5) */
#pragma omp task depend(in : B[0]) depend(out : C[0])
      {
        int j;
        for (j = 0; j < N; j++)
          C[j] = B[j] + 5;
      }

#pragma omp taskwait
    }
  }

  long sum = 0;
  for (i = 0; i < N; i++)
    sum += C[i];

  /* C[i] = 2*i + 5, sum = 2*1023*1024/2 + 5*1024 = 1047552 + 5120 = 1052672 */
  long expected = 1052672;
  printf("sum = %ld (expected %ld)\n", sum, expected);
  free(A);
  free(B);
  free(C);
  return (sum != expected);
}
