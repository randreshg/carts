- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
CreateASTConsumer with file: taskwithdeps.cpp
Found OpenMP Directive (OMPParallelDirective) at taskwithdeps.cpp:33:3
- - - - - - - - - - - - - - - - - - - 
Parallel Directive found at 33
Found OpenMP Directive (OMPSingleDirective) at taskwithdeps.cpp:35:5
- - - - - - - - - - - - - - - - - - - 
Single Directive found at 35
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:38:7
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 38
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:45:7
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 45
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:52:7
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 52
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at taskwithdeps.cpp:52:7 with call: edt_function_1(b);

  Outlined function declaration:
static void __attribute__((annotate("omp.task deps(in: b)"))) edt_function_1(int b);
  Outlined function definition:
static void __attribute__((annotate("omp.task deps(in: b)"))) edt_function_1(
  int b __attribute__((annotate("omp.shared")))) {

          printf("Task 3: Final value of b=%d\n", b);
      
}


  Replaced directive at taskwithdeps.cpp:45:7 with call: edt_function_2(a, b);

  Outlined function declaration:
static void __attribute__((annotate("omp.task deps(in: a)  deps(out: b)"))) edt_function_2(int a, int b);
  Outlined function definition:
static void __attribute__((annotate("omp.task deps(in: a)  deps(out: b)"))) edt_function_2(
  int a __attribute__((annotate("omp.shared"))), 
  int b __attribute__((annotate("omp.shared")))) {

          printf("Task 2: Reading a=%d and updating b\n", a);
          b = a + 5;
      
}


  Replaced directive at taskwithdeps.cpp:38:7 with call: edt_function_3(a);

  Outlined function declaration:
static void __attribute__((annotate("omp.task deps(out: a)"))) edt_function_3(int a);
  Outlined function definition:
static void __attribute__((annotate("omp.task deps(out: a)"))) edt_function_3(
  int a __attribute__((annotate("omp.shared")))) {

          printf("Task 1: Initializing a\n");
          a = 10;
      
}


  Replaced directive at taskwithdeps.cpp:35:5 with call: edt_function_4(a, b);

  Outlined function declaration:
static void __attribute__((annotate("omp.single"))) edt_function_4(int a, int b);
  Outlined function definition:
static void __attribute__((annotate("omp.single"))) edt_function_4(
  int a __attribute__((annotate("omp.default"))), 
  int b __attribute__((annotate("omp.default")))) {

      // Task 1: Initializes 'a'
      edt_function_3(a);


      // Task 2: Depends on Task 1 (reads 'a') and modifies 'b'
      edt_function_2(a, b);


      // Task 3: Depends on Task 2 (reads 'b')
      edt_function_1(b);

      
}


  Replaced directive at taskwithdeps.cpp:33:3 with call: edt_function_5(a, b);

  Outlined function declaration:
static void __attribute__((annotate("omp.parallel"))) edt_function_5(int a, int b);
  Outlined function definition:
static void __attribute__((annotate("omp.parallel"))) edt_function_5(
  int a __attribute__((annotate("omp.default"))), 
  int b __attribute__((annotate("omp.default")))) {

    edt_function_4(a, b);
 // End of single
  
}



- - - - - - - - - - - - - - - - - -
Finished CARTS - OpenMPPluginAction
- - - - - - - - - - - - - - - - - -
Rewritten code saved to taskwithdeps_transformed.cpp
Directive: parallel
  Location: taskwithdeps.cpp:33:3
  Directive: single
    Location: taskwithdeps.cpp:35:5
  Directive: task
    Location: taskwithdeps.cpp:38:7
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:45:7
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:52:7
    Dependencies: No
  Number of Threads: 1
