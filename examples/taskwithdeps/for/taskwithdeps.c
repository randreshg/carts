// Generate standard MLIR dialect
/// cgeist taskwithdeps.c -fopenmp -O0 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include  &> taskwithdeps.mlir

// Passes
/// carts-opt taskwithdeps.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis,create-events,convert-arts-to-llvm,arts-codegen &> taskwithdeps-arts.mlir

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>


#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s N\n", argv[0]);
    return 1;
  }
  int N = atoi(argv[1]);
  int A[N], B[N];
  srand(time(NULL));

  printf("-----------------\nMain function\n-----------------\n");
  printf("Initializing arrays A and B with size %d\n", N);

  #pragma omp parallel
  {
    #pragma omp single
    {
      for (int i = 0; i < N; i++) {
        #pragma omp task depend(inout: A[i])
        {
          A[i] = i;
          printf("Task %d - 0: Initializing A[%d] = %d\n", i, i, A[i]);
        }

        if (i == 0) {
          #pragma omp task depend(in: A[i]) depend(inout: B[i])
          {
            printf("Task %d - 1 -> Input: A[%d] = %d\n", i, i, A[i]);
            B[i] = A[i] + 5;
            printf("Task %d - 1: Computing B[%d] = %d\n", i, i, B[i]);
          }
        }
        else {
          #pragma omp task depend(in: A[i]) depend(in: B[i - 1]) depend(inout: B[i])
          {
            printf("Task %d - 2 -> Input: A[%d] = %d, B[%d] = %d\n", i, i, A[i], i-1, B[i-1]);
            B[i] = A[i] + B[i-1] + 5;
            printf("Task %d - 2: Computing B[%d] = %d\n", i, i, B[i]);
          }
        }
      }
    }
  }

  printf("Final arrays:\n");
  /// print position 0
  for (int i = 0; i < N; i++)
    printf("A[%d] = %d, B[%d] = %d\n", i, A[i], i, B[i]);

  // free(A);
  // free(B);
  printf("-----------------\nMain function DONE\n-----------------\n");
  return 0;
}
