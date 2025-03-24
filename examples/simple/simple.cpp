
#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

/// cgeist -std=c++17 simple.cpp -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include > simple.mlir

int main() {
  srand(time(NULL));
  int shared_number = rand() % 100 + 1;
  int random_number = rand() % 10 + 1;
  printf("EDT 1: The initial number is %d/%d\n", shared_number, random_number);

  #pragma omp parallel
  {
    #pragma omp single
    {
      printf("EDT 0: The number is %d/%d\n", shared_number, random_number);
      #pragma omp task firstprivate(random_number)
      {
        shared_number++;
        random_number++;
        printf("EDT 3: The number is %d/%d\n", shared_number, random_number);
      }
    }
  } 

  shared_number++;
  printf("EDT 2: The final number is %d - %d.\n", shared_number, random_number);
  return 0;
}
