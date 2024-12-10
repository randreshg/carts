- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
Found OpenMP Directive (OMPParallelDirective) at taskwithdeps.cpp:36:1
Parallel Directive found at 36
Found OpenMP Directive (OMPSingleDirective) at taskwithdeps.cpp:38:5
Single Directive found at 38
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:42:13
Task Directive found at 42
- - - - - - - - - - - - - - - 
  Processing task directive...
    Dependency type: 1
    Dependency variable: 0x55965156d7f0
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:46:13
Task Directive found at 46
- - - - - - - - - - - - - - - 
  Processing task directive...
    Dependency type: 0
    Dependency variable: 0x5596515d1b58
    Dependency variable: 0x5596515d1c28
    Dependency type: 1
    Dependency variable: 0x5596515d1d28
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at taskwithdeps.cpp:46:13 with call: edt_function_1(i, B, A);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_1(int i, double * B, double * A);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_1(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * B __attribute__((annotate("omp.default"))), 
  double * A __attribute__((annotate("omp.default")))) {

}


  Replaced directive at taskwithdeps.cpp:42:13 with call: edt_function_2(i, A);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_2(int i, double * A);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_2(
  int i __attribute__((annotate("omp.firstprivate"))), 
  double * A __attribute__((annotate("omp.default")))) {

}


  Replaced directive at taskwithdeps.cpp:38:5 with call: edt_function_3(N, A, B);

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


  Replaced directive at taskwithdeps.cpp:36:1 with call: edt_function_4(N, A, B);

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
CreateASTConsumer with file: taskwithdeps.cpp
Directive: parallel
  Location: taskwithdeps.cpp:36:1
  Directive: single
    Location: taskwithdeps.cpp:38:5
  Directive: task
    Location: taskwithdeps.cpp:42:13
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:46:13
    Dependencies: No
  Number of Threads: 1
Rewritten code saved to taskwithdeps_transformed.cpp
