// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>


__attribute__((noinline))
void long_computation(int &x) {
  int i;
  for (i = 0; i < 1000000; i++) {
    x += i;
  }
}

__attribute__((noinline))
void short_computation(int &x) {
  int i;
  for (i = 0; i < 1000; i++) {
    x += i;
  }
}

/// EDT 1 - carts.edt
int main() {
  srand(time(NULL));
  int x = rand(), y = rand(), res = 0;
  #pragma omp parallel
  #pragma omp single
  {
      // #pragma omp task depend(out: res,x,y) //T0 
      // {
      //     res = 0;
      //     x = rand();
      //     y = rand();
      // }
      #pragma omp task depend(out: res) //T0
          res = 0;
      #pragma omp task depend(out: x) //T1
          long_computation(x);
      #pragma omp task depend(out: y) //T2
          short_computation(y);
      #pragma omp task depend(in: x)
          res += x;
      #pragma omp task depend(in: y)
          res += y;
      #pragma omp task depend(in: res) //T5
          printf("%d", res);
  }
  return 0;
}
