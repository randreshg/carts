#include <omp.h>
#include <stdio.h>

int fib(int n) {
  int i, j;
  if (n < 2)
    return n;
  else {
    #pragma omp task shared(i) {
      i = fib(n - 1);
    }
    {
      
    }


    #pragma omp task shared(j)
      j = fib(n - 2);

    #pragma omp taskwait
      return i + j;
  }
}

int main() {
  int n = rand();
  int i = rand();
  omp_set_dynamic(0);
  omp_set_num_threads(4);
  #pragma omp parallel firstprivate(n)
  {
    #pragma omp single
    printf("fib(%d) = %d\n", n, fib(n));
  }

  printf("Done %d, %d\n", n, i);
}
