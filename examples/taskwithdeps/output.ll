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
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:37:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 37
    Processing dependencies...
    Dependency type: 1
    Variables: res, Updating annotation
x, Updating annotation
y, Updating annotation

Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:43:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 43
    Processing dependencies...
    Dependency type: 1
    Variables: res, Updating annotation

Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:45:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 45
    Processing dependencies...
    Dependency type: 1
    Variables: x, Updating annotation

Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:47:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 47
    Processing dependencies...
    Dependency type: 1
    Variables: y, Updating annotation

Found OpenMP Directive (OMPParallelDirective) at taskwithdeps.cpp:52:3
- - - - - - - - - - - - - - - - - - - 
Parallel Directive found at 52
Found OpenMP Directive (OMPSingleDirective) at taskwithdeps.cpp:54:5
- - - - - - - - - - - - - - - - - - - 
Single Directive found at 54
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:56:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 56
    Processing dependencies...
    Dependency type: 0
    Variables: x, Updating annotation

Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:61:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 61
    Processing dependencies...
    Dependency type: 0
    Variables: y, Updating annotation

Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:65:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 65
    Processing dependencies...
    Dependency type: 0
    Variables: res, Updating annotation

- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at taskwithdeps.cpp:65:9 with call: edt_function_1(res);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_1(int res);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_1(
  int res __attribute__((annotate("omp.default.depend.in")))) {

          printf("%d", res);
        
}


  Replaced directive at taskwithdeps.cpp:61:9 with call: edt_function_2(res, y);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_2(int res, int y);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_2(
  int res __attribute__((annotate("omp.default"))), 
  int y __attribute__((annotate("omp.default.depend.in")))) {

          res += y;
        
}


  Replaced directive at taskwithdeps.cpp:56:9 with call: edt_function_3(res, x);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_3(int res, int x);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_3(
  int res __attribute__((annotate("omp.default"))), 
  int x __attribute__((annotate("omp.default.depend.in")))) {

          res += x;
          x++;
        
}


  Replaced directive at taskwithdeps.cpp:54:5 with call: edt_function_4(x, res, y);

  Outlined function declaration:
static void __attribute__((annotate("omp.single"))) edt_function_4(int x, int res, int y);
  Outlined function definition:
static void __attribute__((annotate("omp.single"))) edt_function_4(
  int x __attribute__((annotate("omp.default"))), 
  int res __attribute__((annotate("omp.default"))), 
  int y __attribute__((annotate("omp.default")))) {

        edt_function_3(res, x);

        edt_function_2(res, y);

        edt_function_1(res);

            
    
}


  Replaced directive at taskwithdeps.cpp:52:3 with call: edt_function_5(x, res, y);

  Outlined function declaration:
static void __attribute__((annotate("omp.parallel"))) edt_function_5(int x, int res, int y);
  Outlined function definition:
static void __attribute__((annotate("omp.parallel"))) edt_function_5(
  int x __attribute__((annotate("omp.default"))), 
  int res __attribute__((annotate("omp.default"))), 
  int y __attribute__((annotate("omp.default")))) {

    edt_function_4(x, res, y);

  
}


  Replaced directive at taskwithdeps.cpp:47:9 with call: edt_function_6(y);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_6(int y);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_6(
  int y __attribute__((annotate("omp.default.depend.out")))) {

          short_computation(y);
}


  Replaced directive at taskwithdeps.cpp:45:9 with call: edt_function_7(x);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_7(int x);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_7(
  int x __attribute__((annotate("omp.default.depend.out")))) {

          long_computation(x);
}


  Replaced directive at taskwithdeps.cpp:43:9 with call: edt_function_8(res);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_8(int res);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_8(
  int res __attribute__((annotate("omp.default.depend.out")))) {

          res = 0;
}


  Replaced directive at taskwithdeps.cpp:37:9 with call: edt_function_9(res, x, y);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_9(int res, int x, int y);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_9(
  int res __attribute__((annotate("omp.default.depend.out"))), 
  int x __attribute__((annotate("omp.default.depend.out"))), 
  int y __attribute__((annotate("omp.default.depend.out")))) {

            res = 0;
            x = rand();
            y = rand();
        
}


  Replaced directive at taskwithdeps.cpp:35:5 with call: edt_function_10(res, x, y);

  Outlined function declaration:
static void __attribute__((annotate("omp.single"))) edt_function_10(int res, int x, int y);
  Outlined function definition:
static void __attribute__((annotate("omp.single"))) edt_function_10(
  int res __attribute__((annotate("omp.default"))), 
  int x __attribute__((annotate("omp.default"))), 
  int y __attribute__((annotate("omp.default")))) {

        edt_function_9(res, x, y);

        edt_function_8(res);

        edt_function_7(x);

        edt_function_6(y);

    
}


  Replaced directive at taskwithdeps.cpp:33:3 with call: edt_function_11(res, x, y);

  Outlined function declaration:
static void __attribute__((annotate("omp.parallel"))) edt_function_11(int res, int x, int y);
  Outlined function definition:
static void __attribute__((annotate("omp.parallel"))) edt_function_11(
  int res __attribute__((annotate("omp.default"))), 
  int x __attribute__((annotate("omp.default"))), 
  int y __attribute__((annotate("omp.default")))) {

    edt_function_10(res, x, y);

  
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
    Location: taskwithdeps.cpp:37:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:43:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:45:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:47:9
    Dependencies: No
  Number of Threads: 1
Directive: parallel
  Location: taskwithdeps.cpp:52:3
  Directive: single
    Location: taskwithdeps.cpp:54:5
  Directive: task
    Location: taskwithdeps.cpp:56:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:61:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:65:9
    Dependencies: No
  Number of Threads: 1
