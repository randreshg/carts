#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 1000

int main() {
  int a[N], b[N];
  long long dot_product = 0;
  int i;

  /// Initialize arrays
  for (i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
  }

  printf("Performing parallel dot product...\n");

  /// Parallel loop for dot product calculation
  #pragma omp parallel for reduction(+:dot_product)
  for (i = 0; i < N; i++) {
    dot_product += (long long)a[i] * b[i];
  }

  printf("Parallel dot product finished.\n");
  printf("Dot product: %lld\n", dot_product);

  /// Verify results
  long long expected_dot_product = 0;
  for (i = 0; i < N; i++) {
    expected_dot_product += (long long)a[i] * b[i];
  }

  if (dot_product != expected_dot_product) {
      printf("Error: Parallel result (%lld) does not match expected result (%lld)\n", dot_product, expected_dot_product);
      return 1;
  } else {
      printf("Result verified successfully.\n");
  }


  return 0;
}
