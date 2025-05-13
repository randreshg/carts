#include <omp.h>
#include <stdio.h>

int fib(int n) {
  int i, j;
  if (n < 2)
    return n;
  else {
    #pragma omp task shared(i)
      i = fib(n - 1);

    #pragma omp task shared(j)
      j = fib(n - 2);

    #pragma omp taskwait
        return i + j;
  }
}

int main(int argc, char *argv[]) {
  int n = 5;
  if (argc > 1) {
    n = atoi(argv[1]);
  }
  
  double start = omp_get_wtime();
  #pragma omp parallel firstprivate(n)
  #pragma omp single
    printf("fib(%d) = %d\n", n, fib(n));
  double end = omp_get_wtime();
  printf("Time taken: %f seconds\n", end - start);

}
