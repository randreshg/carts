// Code 1
// clang++ -fopenmp -O2 -Rpass=arts-transform example.cpp -o example
// clang++ -fopenmp -Rpass=arts-transform -mllvm -debug -O2 -g3 example.cpp -o
// example &> exampleOutput.ll
/// clang++ -fopenmp -Rpass=arts-transform -mllvm -arts-enable -mllvm
/// -debug-only=arts-transform,arts-ir-builder -O3 -g0 example.cpp -o example &>
/// exampleOutput.ll

/// clang++ -fopenmp -Rpass=arts-transform -mllvm -arts-enable -mllvm
/// -debug-only=carts,arts-analyzer -O3 -g0 example.cpp -o
/// example &> exampleOutput.ll

/// Compiler explorer
// -fno-discard-value-names -fopenmp -O3 -g0 -emit-llvm
// Generate LLVM IR
// clang++ -fopenmp -S -emit-llvm -O3 -g0 example.cpp -o example.ll

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>

/// EDT 0
int main() {
  // Generate a random number between 10 and 10
  int shared_number = rand();
  int random_number = rand() % 10 + 10;
  /// EDT 1
  #pragma omp parallel
  {
    // #pragma omp single
    
    ///EDT 3
    #pragma omp task shared(random_number)
    {
      printf("I think the number is %d/%d\n",
             shared_number, random_number);
      ///EDT 2
      // #pragma omp task shared(shared_number)
      shared_number--;
    }

    // #taskwait
  }

  /// EDT 2
  printf("The final number is %d - %d.\n", shared_number, random_number);
  return 0;
}
