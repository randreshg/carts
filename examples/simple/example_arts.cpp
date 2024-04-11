//  clang++ -O3 -fno-discard-value-names -S -emit-llvm example_arts.cpp -o example_arts.ll

// clang++ 
// clang++ -O3 example_arts.cpp -o example_arts libcarts.so
//

// Code 1
#include <stdio.h>
#include "arts.h"
#include "artsRT.h"

/// Crete as many EDTs as workers of the node
void artsParallelEdtCreateWithGuid(
  artsEdt_t funcPtr, uint32_t paramc, uint64_t * paramv, uint32_t depc, unsigned int route) {
    /// Should we create an event here?
    for(unsigned int i = 0; i < artsGetTotalWorkers(); i++) {
      artsGuid_t edtGuid = artsReserveGuidRoute(ARTS_EDT, route);
      artsEdtCreateWithGuid(funcPtr, edtGuid, paramc, paramv, depc);
    }
    
}

void task_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  unsigned int number = paramv[0];
  printf("I think the number is %d.\n", number);
  // number++;
}

void parallel_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  /// We need to get the variables from the parameters and dependencies of the EDT
  unsigned int number = paramv[0];
  /// Reserve the GUIDs for the EDTs
  artsGuid_t task_guid = artsReserveGuidRoute(ARTS_EDT, 0);
  /// FIB
  uint32_t task_paramc = 1;
  uint64_t task_paramv[1] = {static_cast<uint64_t>(number)};
  uint32_t task_depc = 0;
  artsEdtCreateWithGuid(task_edt, task_guid,
                        task_paramc, task_paramv, task_depc);
}

void main_edt(uint32_t paramc, uint64_t *paramv, uint32_t depc, artsEdtDep_t depv[]) {
  int number = 1;
  int random_number = rand() % 10 + 10;

  /// Fill the parameters and dependencies of the EDTs
  uint32_t parallel_paramc = 1;
  uint64_t parallel_paramv[] = {static_cast<uint64_t>(number)};
  uint32_t parallel_depc = 0;
  artsParallelEdtCreateWithGuid(
    parallel_edt, parallel_paramc, parallel_paramv, parallel_depc, 0);

  printf("The final number is %d - % d.\n", number, random_number);
}

void initPerNode(unsigned int nodeId, int argc, char** argv) {
  PRINTF("Node %u argc %u\n", nodeId, argc);
}

void initPerWorker(unsigned int nodeId, unsigned int workerId, int argc, char** argv) {
  if(!nodeId && !workerId) {
    PRINTF("NodeID %u - WorkerID %u\n", nodeId, workerId);
    uint64_t *args;
    artsGuid_t guid = artsEdtCreate(main_edt, 0, 0, args, 0);
  }
}

int main(int argc, char** argv) {
  artsRT(argc, argv);
  
  return 0;
}