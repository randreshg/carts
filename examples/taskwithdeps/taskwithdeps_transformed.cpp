// Code 1

#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>

static void __attribute__((annotate("omp.task"))) edt_function_1(int b);
static void __attribute__((annotate("omp.task"))) edt_function_2(int a, int b);
static void __attribute__((annotate("omp.task"))) edt_function_3(int a);
static void __attribute__((annotate("omp.single"))) edt_function_4(int a,
                                                                   int b);
static void __attribute__((annotate("omp.parallel"))) edt_function_5(int a,
                                                                     int b);

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
int compute() {
  int a = 0;
  int b = 0;

  edt_function_5(a, b);
  // End of parallel
  return 0;
}
static void __attribute__((annotate("omp.task")))
edt_function_1(int b __attribute__((annotate("omp.shared.depend.in")))) {

  printf("Task 3: Final value of b=%d\n", b);
}

static void __attribute__((annotate("omp.task")))
edt_function_2(int a __attribute__((annotate("omp.shared.depend.in"))),
               int b __attribute__((annotate("omp.shared.depend.out")))) {

  printf("Task 2: Reading a=%d and updating b\n", a);
  b = a + 5;
}

static void __attribute__((annotate("omp.task")))
edt_function_3(int a __attribute__((annotate("omp.shared.depend.out")))) {

  printf("Task 1: Initializing a\n");
  a = 10;
}

static void __attribute__((annotate("omp.single")))
edt_function_4(int a __attribute__((annotate("omp.default"))),
               int b __attribute__((annotate("omp.default")))) {

  // Task 1: Initializes 'a'
  edt_function_3(a);

  // Task 2: Depends on Task 1 (reads 'a') and modifies 'b'
  edt_function_2(a, b);

  // Task 3: Depends on Task 2 (reads 'b')
  edt_function_1(b);
}

static void __attribute__((annotate("omp.parallel")))
edt_function_5(int a __attribute__((annotate("omp.default"))),
               int b __attribute__((annotate("omp.default")))) {

  edt_function_4(a, b);
  // End of single
}
