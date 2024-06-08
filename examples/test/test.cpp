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

// int main() {
//   // Generate a random number between 10 and 10
//   int shared_number = rand();
//   int random_number = rand() % 10 + 10;
//   #pragma omp parallel
//   {
//     #pragma omp task firstprivate(random_number)
//     {
//       printf("I think the number is %d/%d\n",
//              shared_number, random_number);
//       shared_number--;
//     }
//   }
//   printf("The final number is %d - %d.\n", shared_number, random_number);
//   return 0;
// }

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
    shared_number++;
    #pragma omp task firstprivate(number) shared(shared_number)
    {
      printf("I think the number is %d - %d.\n", number, shared_number);
      number++;
    }
  }
  printf("The final number is %d - % d.\n", number, random_number);
  return 0;
}


// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>

// int32_t ** allocateMatrix (int32_t l){
//   int32_t **p = (int32_t **)malloc(sizeof(int32_t *) * l);
//   for (int32_t i=0; i < l; i++){
//     p[i] = (int32_t *) malloc(sizeof(int32_t) * l);
//     for (int32_t j=0; j < l; j++){
//       p[i][j] = i + j;
//     }
//   }

//   return p;
// }

// int main (int argc, char *argv[]){
//   if (argc < 2){
//     return 1;
//   }
//   int32_t l = atoi(argv[1]);
//   int32_t **m = allocateMatrix(l);

//   for (int32_t i=0; i < l; i++){
//     for (int32_t j=0; j < l; j++){
//       printf("[%d][%d] = %d\n", i, j, m[i][j]);
//     }
//   }

//   for (int32_t i=0; i < l; i++){
//     free(m[i]);
//   }
//   free(m);

//   return 0;
// }
