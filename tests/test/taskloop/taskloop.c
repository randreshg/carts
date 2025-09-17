#include <stdio.h>
#include <stdlib.h>

int fibonacci(int n) {
  if (n <= 1)
    return n;
  return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(int argc, char **argv) {
  int N = atoi(argv[1]);
  int *results = (int *)malloc(N * sizeof(int));

/// Taskloop example
#pragma omp parallel
  {
#pragma omp single
    {
#pragma omp taskloop
      for (int i = 0; i < N; i++) {
        results[i] = fibonacci(i);
      }
    }
  }

  /// Print results
  printf("Fibonacci numbers:\n");
  for (int i = 0; i < N; i++) {
    printf("fib(%d) = %d\n", i, results[i]);
  }

  free(results);
  return 0;
}
