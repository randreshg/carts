// Code 1
// clang++ -fopenmp -O0 -Rpass=openmp-opt fib.cpp -o fib
// clang++ -fopenmp -Rpass=openmp-opt -mllvm -debug -O0 fib.cpp -o fib &> fib.txt
// clang++ -fopenmp -Rpass=openmp-opt -mllvm -debug-only=openmp-opt -O0 fib.cpp -o fib &> fib.txt
// clang++ -fopenmp -Rpass=openmp-opt -mllvm -debug-only=openmp-opt -mllvm -openmp-opt-print-module-before -O0 fib.cpp -o fib

// Generate LLVM IR
// clang++ -fopenmp -S -emit-llvm -O0 -g0 fib.cpp -o fib.ll


/// ARTS includes
#include "arts.h"

/// Other
#include <stdio.h>
#include <omp.h>

/// APPROACH
/// I can iterate through the whole code to identify the regions of code that
/// require an EDT. I can then reserve the GUIDs for the EDTs and create them. 
/// So I can create the EDTs before the code is executed. Something I have to
/// consider is that reserving the GUIDS requires a node (I can reserve
/// the GUIDs in the same node where the EDTs will be created)

/// In this process I must identify private and shared variables. 
/// I can use the captured variables of the OpenMP regions to identify this
/// information
/// This would allow me to create a graph of the EDTs and dependencies. 
/// To signal the EDTs I have to consider the size of the data. 
/// If the data is small (< int64), I can signal the value
/// directly using artsSignalEdt, artsSignalEdtValue, artsSignalEdtPtr.
/// The decision of which to use depends on the lifetime of the data (whether it is
/// private or shared, we can also consider more variable lifetimes if the memory model
/// allows it).
/// If the data is large, I can use a distributed data structure -> DataBlock

/// Inside the EDT function, based on the parameters, dependencies and OpenMP captured
/// variables, I can replace the variables with the information the EDT requires.

/// For a parallel region, I create an EDT. The function of the EDT is the parallel
/// outlined region. If not inside a single region, I need to create multiple EDTS that
/// will perform the work.

/// For a single region, I simply outline the code inside the parent EDT (usually the
/// parallel region)

/// For a task, I create an EDT. The function of the EDT is the task outlined region.

/// For a function call we need to consider whether it is a void function or not. If it
/// is a void function, we can just call it. If it is not, we need to create an EDT that
/// will be the consumer of the result of the function. The definition of the function
/// will receive the EDT to signal when the function is done.

/// Open questions:
/// - What happens when the function is not known? We can assume the function is safe to call...

void task_0_call1_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  artsGuid_t doneGuid = paramv[0];
  int i = depv[0].guid;
  artsSignalEdtValue(doneGuid, 0, i);
}

void task_0_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  artsGuid_t doneGuid = paramv[1];
  int value = paramv[0];

  /// Reserve the GUIDs for the EDTs
  artsGuid_t fib_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  artsGuid_t fib_done_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  
  //// FIB - Call 1
  // int i = fib(n-1);
  uint32_t fib_paramc = 2;
  uint64_t fib_paramv[] = {value, fib_done_guid};
  uint32_t fib_depc = 0;
  artsEdtCreateWithGuid(fib_edt, fib_guid,
                        fib_paramc, fib_paramv, fib_depc);

  //// FIB DONE
  uint32_t fib_done_paramc = 1;
  uint64_t fib_done_paramv[] = {doneGuid};
  uint32_t fib_done_depc = 1 ;
  artsEdtCreateWithGuid(task_0_call1_edt, fib_done_guid,
                        fib_done_paramc, fib_done_paramv, fib_done_depc);
}

void task_1_call1_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  artsGuid_t doneGuid = paramv[0];
  int i = depv[0].guid;
  artsSignalEdtValue(doneGuid, 0, i);
}

void task_1_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  artsGuid_t doneGuid = paramv[1];
  int value = paramv[0];

  /// Reserve the GUIDs for the EDTs
  artsGuid_t fib_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  artsGuid_t fib_done_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  
  //// FIB - Call 1
  // int i = fib(n-1);
  uint32_t fib_paramc = 2;
  uint64_t fib_paramv[] = {value, fib_done_guid};
  uint32_t fib_depc = 0;
  artsEdtCreateWithGuid(fib_edt, fib_guid,
                        fib_paramc, fib_paramv, fib_depc);

  //// FIB DONE
  uint32_t fib_done_paramc = 1;
  uint64_t fib_done_paramv[] = {doneGuid};
  uint32_t fib_done_depc = 1 ;
  artsEdtCreateWithGuid(task_1_call1_edt, fib_done_guid,
                        fib_done_paramc, fib_done_paramv, fib_done_depc);
}

void task_wait_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  artsGuid_t doneGuid = paramv[0];
  int i = depv[0].guid;
  int j = depv[1].guid;
  int result = i+j;
  artsSignalEdtValue(doneGuid, 0, result);
}

void fib_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  int n = paramv[0];
  artsGuid_t doneGuid = paramv[1];

  int i, j;
  if (n<2)
    artsSignalEdtValue(doneGuid, 0, n);
  else {
    /// Reserve the GUIDs for the EDTs
    artsGuid_t task_0_guid = artsReserveGuidRoute(ARTS_EDT, 0);
    artsGuid_t task_1_guid = artsReserveGuidRoute(ARTS_EDT, 0);
    artsGuid_t task_wait_guid = artsReserveGuidRoute(ARTS_EDT, 0);
    
    // #pragma omp task shared(i) firstprivate(n)
    // i=fib(n-1);
    uint32_t task_0_paramc = 2;
    uint64_t task_0_paramv[] = {n-1, task_wait_guid};
    uint32_t task_0_depc = 0;
    artsEdtCreateWithGuid(task_0_edt, task_0_guid,
                          task_0_paramc, task_0_paramv, task_0_depc);

    // #pragma omp task shared(j) firstprivate(n)
    // j=fib(n-2);
    uint32_t task_1_paramc = 2;
    uint64_t task_1_paramv[] = {n-2, task_wait_guid};
    uint32_t task_1_depc = 0;
    artsEdtCreateWithGuid(task_1_edt, task_1_guid,
                          task_1_paramc, task_1_paramv, task_1_depc);

    /// The omptaskwait represents a barrier for the tasks
    /// If this the last region and the function is not void, send the done signal
    // #pragma omp taskwait
    uint32_t task_wait_paramc = 1;
    uint64_t task_wait_paramv[] = {doneGuid};
    uint32_t task_wait_depc = 2;
    artsEdtCreateWithGuid(task_wait_edt, task_wait_guid,
                          task_wait_paramc, task_wait_paramv, task_wait_depc);
  }
}

void parallel_0_done_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  int *result = (int *)depv[0].ptr;
  int n = paramv[0];
  printf ("fib(%d) = %d\n", n, *result);
}

void parallel_0_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  /// We need to get the variables from the parameters and dependencies of the EDT
  unsigned int n = paramv[0];

  /// Reserve the GUIDs for the EDTs
  artsGuid_t fib_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  artsGuid_t fib_done_guid = artsReserveGuidRoute(ARTS_EDT, 0);

  //// FIB
  uint32_t fib_paramc = 1;
  uint64_t fib_paramv[] = {n, fib_done_guid};
  uint32_t fib_depc = 0;
  artsEdtCreateWithGuid(fib_edt, fib_guid,
                        fib_paramc, fib_paramv, fib_depc);

  //// FIB DONE
  uint32_t fib_done_paramc = 1;
  uint64_t fib_done_paramv[] = {n};
  uint32_t fib_done_depc = 1 ;
  artsEdtCreateWithGuid(parallel_0_done_edt, fib_done_guid,
                        fib_done_paramc, fib_done_paramv, fib_done_depc);
}


int main_edt() {
  int n = 10;

  // omp_set_dynamic(0);
  // omp_set_num_threads(4);

  /// Reserve the GUIDs for the EDTs
  artsGuid_t parallel_0_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  //// Fill the parameters and dependencies of the EDTs
  uint32_t parallel_0_paramc = 1;
  uint64_t parallel_0_paramv[] = {n};
  uint32_t parallel_0_depc = 0;
  artsEdtCreateWithGuid(parallel_0_edt, parallel_0_guid,
                        parallel_0_paramc, parallel_0_paramv, parallel_0_depc);
}

int main(int argc, char** argv) {
  artsRT(argc, argv);
  
  return 0;
}

