#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  int sum = 0;
  int data[100];

  // Initialize array
  for (int i = 0; i < 100; i++)
    data[i] = i;

  printf("Starting parallel region with single and for constructs.\n");

#pragma omp parallel
  {
#pragma omp single
    {
      int tid = omp_get_thread_num();
      printf("Single region executed by thread %d\n", tid);
      sum += 1000;
    }
#pragma omp for
    for (int i = 0; i < 100; i++)
      data[i] = data[i] * 2;
  }

  printf("Parallel region finished.\n");
  printf("Sum value: %d\n", sum);
  printf("First element: %d, Last element: %d\n", data[0], data[99]);

  return 0;
}
