// Code 1
// clang++ -fopenmp -O2 -Rpass=arts-transform example.cpp -o example
// clang++ -fopenmp -Rpass=arts-transform -mllvm -debug -O2 -g3 example.cpp -o  example &> exampleOutput.ll 
/// clang++ -fopenmp -Rpass=arts-transform -mllvm -arts-enable -mllvm -debug-only=arts-transform,arts-ir-builder -O3 -g0 example.cpp -o example &> exampleOutput.ll

/// clang++ -fopenmp -Rpass=arts-transform -mllvm -arts-enable -mllvm -debug-only=arts-transform,arts-ir-builder,attributor -O3 -g0 example.cpp -o example &> exampleOutput.ll

/// Compiler explorer
// -fno-discard-value-names -fopenmp -O3 -g0 -emit-llvm
// Generate LLVM IR
// clang++ -fopenmp -S -emit-llvm -O3 -g0 example.cpp -o example.ll

#include <cstdlib>
#include <stdio.h>

#include <iostream>
// #include <stdlib.h>

int main() {
  // Generate a random number between 10 and 10
  int number = 1;
  int shared_number = 10000;
  int random_number = rand() % 10 + 10;
  int NewRandom = rand();
  // int random_number = rand() % 10 + 10;
  // if (random_number%2 == 0) {
  //   for (int i = 0; i < random_number; i++) {
  //     int random = rand() % 10 + 10;
  //     number = number + random;
  //   }
  // }
  // parallel_function(number);
  // #pragma omp parallel firstprivate(random_number, random)
   #pragma omp parallel
    {
      // for(int i = 0; i < number; i++) {
      //   int random = rand() % 10 + 10;
      //   number = number + random;
      // }

      #pragma omp task firstprivate(random_number, NewRandom)
      {
        printf("I think the number is %d/%d. with %d -- %d\n", number, shared_number, random_number, NewRandom);
        number++;
        shared_number--;
      }
      #pragma omp task firstprivate(number)
      {
        printf("I think the number is %d.\n", number);
        // number++;
      }

      -------
      
    }

  ///-------
  // for (int i = 0; i < random_number; i++) {
  //   int random = rand() % 10 + 10;
  //   number = number + random;
  //   if(number == 12)
  //     return 0;
  // }
  // random_number = rand()+ number;
  // if(random_number == 12)
  //   return 0;
  printf("The final number is %d - % d.\n", number, random_number);
  return 0;
}

// extern int **a;
// extern int N;
// extern int x;
// extern int y;

// int main() {
//   int i, j;
//   for (i = 0; i < N; i++) {
//     for (j = 0; j < N; j++) {
//       if (i < j) {
//         a[i][j] += x * y + x * y;
//       } else {
//         a[i][j] -= x * x - y * y;
//       }
//     }
//   }
//   for (i = 0; i < N; i++) {
//     a[i][i] = 1;
//   }
// }