#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1D stencil computation: output[i] = input[i-1] + input[i] + input[i+1]
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s N\n", argv[0]);
    return 1;
  }

  int N = atoi(argv[1]);
  if (N < 3) {
    printf("N must be at least 3\n");
    return 1;
  }

  int *input = (int *)malloc(N * sizeof(int));
  int *output = (int *)malloc(N * sizeof(int));

  // Initialize
  for (int i = 0; i < N; i++) {
    input[i] = i + 1;
    output[i] = 0;
  }

  printf("1D Stencil: N=%d\n", N);

#pragma omp parallel
  {
#pragma omp single
    {
      // Each task computes output[i] from input[i-1:i+2] (3-point stencil)
      // Tasks can run in parallel as long as their read ranges don't overlap
      // with write locations
      for (int i = 1; i < N - 1; i++) {
        // This task reads input[i-1], input[i], input[i+1]
        // and writes to output[i]
#pragma omp task depend(in : input[i - 1], input[i], input[i + 1])             \
    depend(out : output[i])
        {
          printf("Task %d: Computing output[%d] from input[%d:%d]\n", i, i,
                 i - 1, i + 2);
          output[i] = input[i - 1] + input[i] + input[i + 1];
          printf("Task %d: output[%d] = %d + %d + %d = %d\n", i, i,
                 input[i - 1], input[i], input[i + 1], output[i]);
        }
      }

      // Verify task that reads all outputs
#pragma omp task depend(in : output[0])
      {
        printf("\nVerification:\n");
        for (int i = 1; i < N - 1; i++) {
          int expected = (i - 1 + 1) + (i + 1) + (i + 1 + 1);
          printf("output[%d] = %d (expected %d) %s\n", i, output[i], expected,
                 output[i] == expected ? "PASS" : "FAIL");
        }
      }
    }
  }

  free(input);
  free(output);
  return 0;
}
