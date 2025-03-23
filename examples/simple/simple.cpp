
#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

int main() {
  // Generate a random number between 10 and 10
  srand(time(NULL));
  int shared_number = rand() % 100 + 1;
  int random_number = rand() % 10 + 1;
  printf("EDT 1: The initial number is %d/%d\n", shared_number, random_number);

  /// EDT 0
  /// 0: random_number, 1: shared_number
  #pragma omp parallel
  {
      #pragma omp single
      {
        printf("EDT 0: The number is %d/%d\n", shared_number, random_number);
        /// EDT 3
        /// 0: shared_number 
        #pragma omp task firstprivate(random_number)
        {
          shared_number++;
          random_number++;
          printf("EDT 3: The number is %d/%d\n", shared_number, random_number);
        }
      }
  } 

  /// EDT 2
  shared_number++;
  printf("EDT 2: The final number is %d - %d.\n", shared_number, random_number);
  return 0;
}
