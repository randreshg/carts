#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <size>\n", argv[0]);
    return 1;
  }
  int N = atoi(argv[1]);
  int *a = (int *)malloc(N * sizeof(int));
  int *b = (int *)malloc(N * sizeof(int));
  int *c = (int *)malloc(N * sizeof(int));

  // Initialize arrays
  for (int i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
  }
  /// Print initial values
  printf("Initial values:\n");
  for (int i = 0; i < N; i++)
    printf("a[%d] = %d, b[%d] = %d\n", i, a[i], i, b[i]);

  /// Parallel region with worksharing loop - vector addition
  printf("Parallel region:\n");
#pragma omp parallel for schedule(static)
  for (int i = 0; i < N; i++)
    c[i] = a[i] + b[i];

  printf("Parallel region completed\n");

  /// Results:
  bool success = true;
  for (int i = 0; i < N; i++) {
    if (c[i] != a[i] + b[i]) {
      success = false;
      break;
    }
  }
  printf("Results:\n");
  for (int i = 0; i < N; i++)
    printf("c[%d] = %d, a[%d] = %d, b[%d] = %d\n", i, c[i], i, a[i], i, b[i]);
  printf("Success: %s\n", success ? "true" : "false");
  
  free(a);
  free(b);
  free(c);

  return 0;
}
