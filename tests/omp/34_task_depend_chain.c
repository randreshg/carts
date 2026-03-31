/* 34_task_depend_chain.c - 3 tasks: write->modify->read via depends */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int A = 0;
  int B = 0;

#pragma omp parallel
  {
#pragma omp single
    {
#pragma omp task depend(out : A)
      {
        A = 100;
      }

#pragma omp task depend(in : A) depend(out : B)
      {
        B = A + 50;
      }

#pragma omp task depend(in : B)
      {
        A = B * 2;
      }

#pragma omp taskwait
    }
  }

  printf("A = %d, B = %d (expected 300, 150)\n", A, B);
  return (A == 300 && B == 150) ? 0 : 1;
}
