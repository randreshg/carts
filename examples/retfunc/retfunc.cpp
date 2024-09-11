// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>



int func1(int shared_number, int random_number) {
  /// EDT 4 (Taskwait) - carts.edt.5
  /// 0: shared_number, 1: random_number
  // {
    printf("** EDT 0: The number is %d/%d\n", shared_number, random_number);

    /// EDT 3 - carts.edt.2
    /// 0: shared_number 
    #pragma omp task firstprivate(random_number) shared(shared_number)
    {
      shared_number++;
      random_number++;
      printf("** EDT 3: The number is %d/%d\n", shared_number, random_number);
    }
  // }
  
  #pragma omp taskwait
}

/// EDT 1 - carts.edt
int main() {
  // Generate a random number between 10 and 10
  srand(time(NULL));
  int shared_number = rand() % 100 + 1;
  int random_number = rand() % 10 + 1;
  printf("** EDT 1: The initial number is %d/%d\n", shared_number, random_number);

  /// EDT 0 - carts.edt.1
  /// 0: shared_number, 1: random_number
  #pragma omp parallel
  {
    #pragma omp single 
    {
      func1(shared_number, random_number);
      

      #pragma omp taskwait

      /// EDT 6 (Taskwait Done) - carts.edt.3
      /// 0: shared_number, 1: random_number
      // {
        /// EDT 5 - carts.edt.4
        /// 0: random_number
        #pragma omp task firstprivate(shared_number) shared(random_number)
        {
          shared_number++;
          printf("** EDT 4: The number is %d/%d\n", shared_number, random_number);
        }
      // }
    }
  } 

  /// EDT 2 - carts.edt.6
  /// 0:random_number, 1: shared_number
  printf("EDT 2: The final number is %d - %d.\n", shared_number, random_number);
  return 0;
}
