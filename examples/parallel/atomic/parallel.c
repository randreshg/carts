#include <omp.h>
#include <stdio.h>

int main() {
  int shared = 0;
  int num_threads = 4;

  printf("Running with %d threads.\n", num_threads);

#pragma omp parallel num_threads(num_threads) shared(shared)
  {
    int x = omp_get_thread_num();
    int priv1 = x;

    if (x % 2 == 0) {
      printf("Thread %d (priv1=%d) ----> EVEN\n", x, priv1);
    } else {
      printf("Thread %d (priv1=%d) ----> ODD\n", x, priv1);
    }

    // Safely increment shared variable
#pragma omp atomic
    shared++;
  }

  // The final value of shared should be the initial value + num_threads
  printf("Final shared value: %d (expected %d)\n", shared, num_threads);

  return 0;
}
