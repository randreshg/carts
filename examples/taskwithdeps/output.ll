- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
CreateASTConsumer with file: taskwithdeps.cpp
Found OpenMP Directive (OMPParallelDirective) at taskwithdeps.cpp:33:3
- - - - - - - - - - - - - - - - - - - 
Parallel Directive found at 33
Found OpenMP Directive (OMPSingleDirective) at taskwithdeps.cpp:35:7
- - - - - - - - - - - - - - - - - - - 
Single Directive found at 35
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:38:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 38
    Processing dependencies...
    Dependency type: 1
    Variables: a, 
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:45:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 45
    Processing dependencies...
    Dependency type: 0
    Variables: a, 
    Processing dependencies...
    Dependency type: 1
    Variables: b, 
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:52:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 52
    Processing dependencies...
    Dependency type: 0
    Variables: b, 
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at taskwithdeps.cpp:52:9 with call: edt_function_1(b);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_1(int b);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_1(
  int b __attribute__((annotate("omp.shared.depend.in")))) {

            printf("Task 3: Final value of b=%d\n", b);
        
}


  Replaced directive at taskwithdeps.cpp:45:9 with call: edt_function_2(a, b);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_2(int a, int b);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_2(
  int a __attribute__((annotate("omp.shared.depend.in"))), 
  int b __attribute__((annotate("omp.shared.depend.out")))) {

            printf("Task 2: Reading a=%d and updating b\n", a);
            b = a + 5;
        
}


  Replaced directive at taskwithdeps.cpp:38:9 with call: edt_function_3(a);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_3(int a);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_3(
  int a __attribute__((annotate("omp.shared.depend.out")))) {

            printf("Task 1: Initializing a\n");
            a = 10;
        
}


  Replaced directive at taskwithdeps.cpp:35:7 with call: edt_function_4(a, b);

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
    Location: taskwithdeps.cpp:35:7
  Directive: task
    Location: taskwithdeps.cpp:38:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:45:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:52:9
    Dependencies: No
  Number of Threads: 1
