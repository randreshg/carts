// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

static void __attribute__((annotate("omp.task"))) edt_function_1(int res);
static void __attribute__((annotate("omp.task"))) edt_function_2(int res,
                                                                 int y);
static void __attribute__((annotate("omp.task"))) edt_function_3(int res,
                                                                 int x);
static void __attribute__((annotate("omp.single")))
edt_function_4(int x, int res, int y);
static void __attribute__((annotate("omp.parallel")))
edt_function_5(int x, int res, int y);
static void __attribute__((annotate("omp.task"))) edt_function_6(int y);
static void __attribute__((annotate("omp.task"))) edt_function_7(int x);
static void __attribute__((annotate("omp.task"))) edt_function_8(int res);
static void __attribute__((annotate("omp.task"))) edt_function_9(int res, int x,
                                                                 int y);
static void __attribute__((annotate("omp.single")))
edt_function_10(int res, int x, int y);
static void __attribute__((annotate("omp.parallel")))
edt_function_11(int res, int x, int y);

/// clang++ -fopenmp -std=c++17 taskwithdeps.cpp  -Xclang -plugin  -Xclang
/// omp-plugin
/// -fplugin=/home/randres/projects/carts/.install/carts/lib/libOpenMPPlugin.so
/// -mllvm -debug-only=omp-plugin -S &> output.ll

__attribute__((noinline)) void long_computation(int &x) {
  int i;
  for (i = 0; i < 1000000; i++) {
    x += i;
  }
}

__attribute__((noinline)) void short_computation(int &x) {
  int i;
  for (i = 0; i < 1000; i++) {
    x += i;
  }
}

/// EDT 1 - carts.edt

int main() {
  srand(time(NULL));
  int x = rand(), y = rand(), res = 0;
  edt_function_11(res, x, y);

  edt_function_5(x, res, y);

  return 0;
}

/// PARALLEL 1
static void __attribute__((annotate("omp.parallel")))
edt_function_11(int res __attribute__((annotate("omp.default"))),
                int x __attribute__((annotate("omp.default"))),
                int y __attribute__((annotate("omp.default")))) {

  edt_function_10(res, x, y);
}

static void __attribute__((annotate("omp.single")))
edt_function_10(int res __attribute__((annotate("omp.default"))),
                int x __attribute__((annotate("omp.default"))),
                int y __attribute__((annotate("omp.default")))) {

  edt_function_9(res, x, y);

  edt_function_8(res);

  edt_function_7(x);

  edt_function_6(y);
}

static void __attribute__((annotate("omp.task")))
edt_function_9(int res __attribute__((annotate("omp.default.depend.out"))),
               int x __attribute__((annotate("omp.default.depend.out"))),
               int y __attribute__((annotate("omp.default.depend.out")))) {

  res = 0;
  x = rand();
  y = rand();
}

static void __attribute__((annotate("omp.task")))
edt_function_8(int res __attribute__((annotate("omp.default.depend.out")))) {

  res = 0;
}

static void __attribute__((annotate("omp.task")))
edt_function_7(int x __attribute__((annotate("omp.default.depend.out")))) {

  long_computation(x);
}

static void __attribute__((annotate("omp.task")))
edt_function_6(int y __attribute__((annotate("omp.default.depend.out")))) {

  short_computation(y);
}

/// PARALLEL 2
static void __attribute__((annotate("omp.parallel")))
edt_function_5(int x __attribute__((annotate("omp.default"))),
               int res __attribute__((annotate("omp.default"))),
               int y __attribute__((annotate("omp.default")))) {

  edt_function_4(x, res, y);
}

static void __attribute__((annotate("omp.single")))
edt_function_4(int x __attribute__((annotate("omp.default"))),
               int res __attribute__((annotate("omp.default"))),
               int y __attribute__((annotate("omp.default")))) {

  edt_function_3(res, x);

  edt_function_2(res, y);

  edt_function_1(res);
}

static void __attribute__((annotate("omp.task")))
edt_function_3(int res __attribute__((annotate("omp.default"))),
               int x __attribute__((annotate("omp.default.depend.in")))) {

  res += x;
  x++;
}

static void __attribute__((annotate("omp.task")))
edt_function_2(int res __attribute__((annotate("omp.default"))),
               int y __attribute__((annotate("omp.default.depend.in")))) {

  res += y;
}

static void __attribute__((annotate("omp.task")))
edt_function_1(int res __attribute__((annotate("omp.default.depend.in")))) {

  printf("%d", res);
}