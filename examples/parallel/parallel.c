#include <omp.h>
#include <stdio.h>

int main() {
  int priv1 = 0, priv2 = 0;
  int shared = 123123;
  int test = 0;
#pragma omp parallel num_threads(4) shared(shared) private(test)
  {
    int x = omp_get_thread_num();
    priv1 = x;
    char *msg = NULL;
    if (x % 2 == 0) {
      printf("Hello from thread %d ----> EVEN\n", x);
    }
    if (x % 2 == 1) {
      printf("Hello from thread %d ----> ODD\n", x);
    }
    shared++;
  }
  printf("priv: %d\n", priv1);
  return 0;
}
