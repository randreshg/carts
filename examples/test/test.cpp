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
#include <time.h>

/// EDT 1
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
    /// pragma omp single
    /// EDT 3
    // if(random_number%2 == 0) {
    printf("EDT 0: The number is %d/%d\n", shared_number, random_number);
  
    // 0: shared_number 
    /// EDT 3
#pragma omp task firstprivate(random_number)
    {
      shared_number++;
      random_number++;
      printf("EDT 3: The number is %d/%d\n", shared_number, random_number);
    }
    // }

    /// EDT 4
    // #pragma omp task firstprivate(random_number)
    // {
    //   printf("The number is %d\n", random_number);
    // }
  }

  /// EDT 2
  shared_number++;
  printf("EDT 2: The final number is %d - %d.\n", shared_number, random_number);
  return 0;
}

// ///
// int main() {
//   // Generate a random number between 10 and 10
//   int shared_number = rand();
//   int random_number = rand() % 10 + 10;
//   /// EDT 1
//   #pragma omp parallel
//   {
//     // #pragma omp single
//     // if(random_number == 0) {
//       /// EDT 3
//       {
//         ///EDT 5
//         #pragma omp task firstprivate(random_number)
//         {

//           printf("I think the number is %d/%d\n",
//                 shared_number, random_number);
//           ///EDT 7
//           // #pragma omp task shared(shared_number)
//           // shared_number--;
//         }

//         /// EDT 6
//         {
//         }
//       }

//       // #taskwait
//       /// EDT 4
//       {

//       }
//     // }

//   }

//   /// EDT 2
//   printf("The final number is %d - %d.\n", shared_number, random_number);
//   return 0;
// }