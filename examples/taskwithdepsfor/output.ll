- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
CreateASTConsumer with file: taskwithdeps.cpp
Found OpenMP Directive (OMPParallelDirective) at taskwithdeps.cpp:19:3
- - - - - - - - - - - - - - - - - - - 
Parallel Directive found at 19
Found OpenMP Directive (OMPSingleDirective) at taskwithdeps.cpp:21:5
- - - - - - - - - - - - - - - - - - - 
Single Directive found at 21
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:25:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 25
    Processing dependencies...
    Dependency type: 1
    Variables: Not a DeclRefExpr

Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:29:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 29
    Processing dependencies...
    Dependency type: 0
    Variables: Not a DeclRefExpr
Not a DeclRefExpr

    Processing dependencies...
    Dependency type: 1
    Variables: Not a DeclRefExpr

- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at taskwithdeps.cpp:29:9 with call: edt_function_1(i, B, A);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_1(int i, double * B, double * A);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_1(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * B __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default")))) {

          B[i] = A[i] + (i > 0 ? A[i - 1] : 0);
}


  Replaced directive at taskwithdeps.cpp:25:9 with call: edt_function_2(i, A);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_2(int i, double * A);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_2(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * A __attribute__((annotate("omp.default")))) {

          A[i] = i * 1.0;
}


  Replaced directive at taskwithdeps.cpp:21:5 with call: edt_function_3(N, A, B);

  Outlined function declaration:
static void __attribute__((annotate("omp.single"))) edt_function_3(int N, double * A, double * B);
  Outlined function definition:
static void __attribute__((annotate("omp.single"))) edt_function_3(
  int N __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default"))), 
  double * B __attribute__((annotate("omp.default")))) {

      for (int i = 0; i < N; i++) {
        // Task 1: Compute A[i]
        edt_function_2(i, A);


        // Task 2: Compute B[i] based on A[i] and A[i-1] (inter-loop dependency)
        edt_function_1(i, B, A);

      }
    
}


  Replaced directive at taskwithdeps.cpp:19:3 with call: edt_function_4(N, A, B);

  Outlined function declaration:
static void __attribute__((annotate("omp.parallel"))) edt_function_4(int N, double * A, double * B);
  Outlined function definition:
static void __attribute__((annotate("omp.parallel"))) edt_function_4(
  int N __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default"))), 
  double * B __attribute__((annotate("omp.default")))) {

    edt_function_3(N, A, B);

  
}



- - - - - - - - - - - - - - - - - -
Finished CARTS - OpenMPPluginAction
- - - - - - - - - - - - - - - - - -
Rewritten code saved to taskwithdeps_transformed.cpp
Directive: parallel
  Location: taskwithdeps.cpp:19:3
  Directive: single
    Location: taskwithdeps.cpp:21:5
  Directive: task
    Location: taskwithdeps.cpp:25:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:29:9
    Dependencies: No
  Number of Threads: 1
