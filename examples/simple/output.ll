- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
Found OpenMP Directive (OMPParallelDirective) at simple.cpp:25:3
Parallel Directive found at 25
Found OpenMP Directive (OMPTaskDirective) at simple.cpp:31:7
Task Directive found at 31
- - - - - - - - - - - - - - - 
  Processing task directive...
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Outlined body before:
#pragma omp task firstprivate(random_number)
      {
        shared_number++;
        random_number++;
        printf("EDT 3: The number is %d/%d\n", shared_number, random_number);
      }
  Outlined body:

        shared_number++;
        random_number++;
        printf("EDT 3: The number is %d/%d\n", shared_number, random_number);
      
  Replaced directive at simple.cpp:31:7 with call: edt_function_1(shared_number, random_number);

  Outlined function declaration:
static void __attribute__((annotate("omp.task"))) edt_function_1(int shared_number, int random_number);
  Outlined function definition:
static void __attribute__((annotate("omp.task"))) edt_function_1(
  int shared_number __attribute__((annotate("omp.default"))), 
  int random_number __attribute__((annotate("omp.firstprivate")))) {

        shared_number++;
        random_number++;
        printf("EDT 3: The number is %d/%d\n", shared_number, random_number);
      
}


  Outlined body before:
#pragma omp parallel
  {
      printf("EDT 0: The number is %d/%d\n", shared_number, random_number);

      /// EDT 3
      /// 0: shared_number 
      edt_function_1(shared_number, random_number);

  
  }
  Outlined body:

      printf("EDT 0: The number is %d/%d\n", shared_number, random_number);

      /// EDT 3
      /// 0: shared_number 
      edt_function_1(shared_number, random_number);

  
  
  Replaced directive at simple.cpp:25:3 with call: edt_function_2(shared_number, random_number);

  Outlined function declaration:
static void __attribute__((annotate("omp.parallel"))) edt_function_2(int shared_number, int random_number);
  Outlined function definition:
static void __attribute__((annotate("omp.parallel"))) edt_function_2(
  int shared_number __attribute__((annotate("omp.default"))), 
  int random_number __attribute__((annotate("omp.default")))) {

      printf("EDT 0: The number is %d/%d\n", shared_number, random_number);

      /// EDT 3
      /// 0: shared_number 
      edt_function_1(shared_number, random_number);

  
  
}



- - - - - - - - - - - - - - - - - -
Finished CARTS - OpenMPPluginAction
- - - - - - - - - - - - - - - - - -
CreateASTConsumer with file: simple.cpp
Directive: parallel
  Location: simple.cpp:25:3
  Directive: task
    Location: simple.cpp:31:7
    Dependencies: No
  Number of Threads: 1
Rewritten code saved to simple_transformed.cpp
