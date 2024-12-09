- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
Found OpenMP Directive (OMPTaskDirective) at simplefn.cpp:16:3
Task Directive found at 16
- - - - - - - - - - - - - - - 
  Processing task directive...
  Collecting captured variables...
Found OpenMP Directive (OMPParallelDirective) at simplefn.cpp:34:3
Parallel Directive found at 34
  Collecting captured variables...
Found OpenMP Directive (OMPSingleDirective) at simplefn.cpp:36:5
  (Unhandled directive type)
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at simplefn.cpp:34:3 with call: edt_function_1(shared_number, random_number);

  Outlined function declaration:
static void __attribute__((annotate("omp.parallel"))) edt_function_1(int shared_number, int random_number);
  Outlined function definition:
static void __attribute__((annotate("omp.parallel"))) edt_function_1(
  int shared_number __attribute__((annotate("omp.default"))), 
  int random_number __attribute__((annotate("omp.default")))) {

    #pragma omp single 
    {
      printf("EDT 0: The number is %d/%d\n", shared_number, random_number);
      /// EDT 0 - @carts.edt
      taskFunction(shared_number, random_number);
    }
  
}


  Replaced directive at simplefn.cpp:16:3 with call: edt_function_2(random_number, shared_number);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_2(int & random_number, int & shared_number);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_2(
  int & random_number __attribute__((annotate("omp.firstprivate"))), 
  int & shared_number __attribute__((annotate("omp.shared")))) {

    shared_number++;
    random_number++;
    printf("EDT 1: The number is %d/%d\n", shared_number, random_number);
  
}



- - - - - - - - - - - - - - - - - -
Finished CARTS - OpenMPPluginAction
- - - - - - - - - - - - - - - - - -
CreateASTConsumer with file: simplefn.cpp
Directive: task
  Location: simplefn.cpp:16:3
  Dependencies: No
Directive: parallel
  Location: simplefn.cpp:34:3
  Number of Threads: 1
Rewritten code saved to simplefn_transformed.cpp
