/* 42_task_nested.c - outer task spawns inner task */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int A = 0;
  int B = 0;

#pragma omp parallel
  {
#pragma omp single
    {
#pragma omp task shared(A, B)
      {
        A = 10;

#pragma omp task shared(B)
        {
          B = 20;
        }

#pragma omp taskwait
      }

#pragma omp taskwait
    }
  }

  int sum = A + B;
  printf("A=%d B=%d sum=%d (expected 30)\n", A, B, sum);
  return (sum == 30) ? 0 : 1;
}
