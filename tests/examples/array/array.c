#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "arts/Utils/Testing/CartsTest.h"
// #define DEBUG 0

int main(int argc, char *argv[]) {
  CARTS_TIMER_START();
  if (argc < 2) {
    printf("Usage: %s N\n", argv[0]);
    return 1;
  }
  int N = atoi(argv[1]);
  int *A = (int *)malloc(N * sizeof(int));
  int *B = (int *)malloc(N * sizeof(int));
  srand(time(NULL));

#if DEBUG
  printf("Initializing arrays A and B with size %d\n", N);
#endif

#pragma omp parallel
  {
#pragma omp single
    {
      for (int i = 0; i < N; i++) {
#pragma omp task depend(inout : A[i])
        {
          A[i] = i;
#if DEBUG
          printf("Task %d - 0: Initializing A[%d] = %d\n", i, i, A[i]);
#endif
        }

        if (i == 0) {
#pragma omp task depend(in : A[i]) depend(inout : B[i])
          {
#if DEBUG
            printf("Task %d - 1 -> Input: A[%d] = %d\n", i, i, A[i]);
#endif
            B[i] = A[i] + 5;
#if DEBUG
            printf("Task %d - 1: Computing B[%d] = %d\n", i, i, B[i]);
#endif
          }
        } else {
#pragma omp task depend(in : A[i]) depend(in : B[i - 1]) depend(inout : B[i])
          {
#if DEBUG
            printf("Task %d - 2 -> Input: A[%d] = %d, B[%d] = %d\n", i, i, A[i],
                   i - 1, B[i - 1]);
#endif
            B[i] = A[i] + B[i - 1] + 5;
#if DEBUG
            printf("Task %d - 2: Computing B[%d] = %d\n", i, i, B[i]);
#endif
          }
        }
      }
    }
  }

#if DEBUG
  printf("Final arrays:\n");
  for (int i = 0; i < N; i++)
    printf("A[%d] = %d, B[%d] = %d\n", i, A[i], i, B[i]);
#endif

  // Verify: A[i] = i, B[i] = sum(A[0..i]) + 5*(i+1)
  int correct = 1;
  for (int i = 0; i < N; i++) {
    int expected_B = (i == 0) ? A[0] + 5 : A[i] + B[i - 1] + 5;
    // Check iteratively: B[i] = i*(i+1)/2 + 5*(i+1)
    int sum_A = i * (i + 1) / 2;
    expected_B = sum_A + 5 * (i + 1);
    if (A[i] != i || B[i] != expected_B) {
      correct = 0;
      break;
    }
  }

  free(A);
  free(B);

  if (correct) {
    CARTS_TEST_PASS();
  } else {
    CARTS_TEST_FAIL("array computation mismatch");
  }
}
