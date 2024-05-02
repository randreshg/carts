// Code 1
// clang++ -fopenmp -O2 -Rpass=arts-transform example.cpp -o example
// clang++ -fopenmp -Rpass=arts-transform -mllvm -debug -O2 -g3 example.cpp -o
// example &> exampleOutput.ll
/// clang++ -fopenmp -Rpass=arts-transform -mllvm -arts-enable -mllvm
/// -debug-only=arts-transform,arts-ir-builder -O3 -g0 example.cpp -o example &>
/// exampleOutput.ll

/// clang++ -fopenmp -Rpass=arts-transform -mllvm -arts-enable -mllvm
/// -debug-only=arts-transform,arts-ir-builder,attributor -O3 -g0 example.cpp -o
/// example &> exampleOutput.ll

/// Compiler explorer
// -fno-discard-value-names -fopenmp -O3 -g0 -emit-llvm
// Generate LLVM IR
// clang++ -fopenmp -S -emit-llvm -O3 -g0 example.cpp -o example.ll

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
// #include <stdlib.h>

int main() {
  // Generate a random number between 10 and 10
  int number = 1;
  int shared_number = 10000;
  int random_number = rand() % 10 + 10;
  int NewRandom = rand();
  #pragma omp parallel
  {
    #pragma omp task firstprivate(random_number, NewRandom)
    {
      printf("I think the number is %d/%d. with %d -- %d\n", number,
             shared_number, random_number, NewRandom);
      number++;
      shared_number--;
    }
    #pragma omp task firstprivate(number)
    {
      printf("I think the number is %d.\n", number);
      number++;
    }
  }
  printf("The final number is %d - % d.\n", number, random_number);
  return 0;
}
