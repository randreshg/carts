/* 280: parallel for with fewer iterations than threads */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int A[3];
  A[0] = A[1] = A[2] = 0;

/* 3 iterations but 8 threads -- 5 threads get nothing */
#pragma omp parallel for num_threads(8)
  for (int i = 0; i < 3; i++)
    A[i] = (i + 1) * 10;

  int sum = A[0] + A[1] + A[2];
  int expected = 10 + 20 + 30;
  printf("sum = %d (expected %d)\n", sum, expected);
  return (sum != expected);
}
