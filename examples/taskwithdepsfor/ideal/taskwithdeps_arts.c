

// clang taskwithdeps_arts.c -O3 -g0 -march=native -I/home/randres/projects/carts/.install/arts/include -o taskwithdeps_arts -L/home/randres/projects/carts/.install/arts/lib -larts -lrdmacm
#include "arts.h"
#include "artsRT.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Task 1: Compute A[i]
void computeA(uint32_t paramc, uint64_t *paramv, uint32_t depc,
              artsEdtDep_t depv[]) {
  // paramv[0] = i, paramv[1] = A[i] eventGuid
  unsigned int i = (unsigned int)paramv[0];
  artsGuid_t eventGuid = (artsGuid_t)paramv[12];

  // Access the DataBlock for A[i]
  double *A_i = (double *)depv[0].ptr;

  // Compute A[i]
  A_i[0] = i * 1.0;

  // Signal the event associated with A[i] after computation
  artsEventSatisfySlot(eventGuid, depv[0].guid, 0);
}

// Task 2: Compute B[i]
void computeB(uint32_t paramc, uint64_t *paramv, uint32_t depc,
              artsEdtDep_t depv[]) {
  // paramv[0] = i, paramv[1] = B[i] dbGuid
  unsigned int i = (unsigned int)paramv[0];

  // Access the DataBlocks for A[i], A[i-1] (via events), and B[i]
  double *A_i = (double *)depv[0].ptr;
  double A_im1_val = 0.0;
  if (i > 0) {
    double *A_im1 = (double *)depv[1].ptr;
    A_im1_val = A_im1[0];
  }
  double *B_i = (double *)depv[2].ptr;

  // Compute B[i]
  B_i[0] = A_i[0] + A_im1_val;
}

void printDataBlockA(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                     artsEdtDep_t depv[]) {
  double *data = (double *)depv[0].ptr;
  for (int i = 0; i < paramv[0]; i++) {
    printf("A[%d]: %f\n", i, data[i]);
  }
}

void printDataBlockB(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                     artsEdtDep_t depv[]) {
  double *data = (double *)depv[0].ptr;
  for (int i = 0; i < paramv[0]; i++) {
    printf("B[%d]: %f\n", i, data[i]);
  }
}

/// Parallel EDT
void parallelEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                 artsEdtDep_t depv[]) {
  printf("---------- Parallel EDT started ----------\n");
  /// Context
  unsigned int currentNode = artsGetCurrentNode();
  artsGuid_t epochGuid = artsGetCurrentEpochGuid();
  artsArrayDb_t *A_array = (artsArrayDb_t *)depv[0].ptr;
  artsArrayDb_t *B_array = (artsArrayDb_t *)depv[1].ptr;

  /// Parameters
  int N = paramv[0];
  artsGatherArrayDb(A_array, printDataBlockA, 0, 1, paramv, 0);
  artsGatherArrayDb(B_array, printDataBlockB, 0, 1, paramv, 0);

  // Preallocate all DataBlocks and events
  artsGuid_t *A_event = (artsGuid_t *)artsMalloc(N * sizeof(artsGuid_t));
  for (int i = 0; i < N; i++) {
    A_event[i] = artsEventCreate(currentNode, 1); // latchCount = 1
  }
  // // Create tasks for A[i] and B[i]
  // for (int i = 0; i < N; i++) {
  //   // Task 1: Compute A[i]
  //   uint64_t Aparams[] = {(uint64_t)i, (uint64_t)A_event[i]};
  //   artsGuid_t A_edt = artsEdtCreateWithEpoch((artsEdt_t)computeA,
  //   currentNode,
  //                                             2, Aparams, 1, epochGuid);
  //   artsGetFromArrayDb(A_edt, 0, A_array, i);

  //   // Task 2: Compute B[i]
  //   uint64_t Bparams[] = {(uint64_t)i};
  //   // Dependencies: A[i], (A[i-1] if i > 0), and B[i]
  //   int depCount = (i > 0) ? 3 : 2;

  //   artsGuid_t B_edt = artsEdtCreateWithEpoch((artsEdt_t)computeB,
  //   currentNode,
  //                                             2, Bparams, depCount,
  //                                             epochGuid);

  //   // Add dependencies to B[i] EDT through events
  //   // Dependency: A[i]
  //   artsAddDependence(A_event[i], B_edt, 0);
  //   if (i > 0) {
  //     // Dependency: A[i-1]
  //     artsAddDependence(A_event[i - 1], B_edt, 1);
  //   }
  //   artsGetFromArrayDb(B_edt, 2, B_array, i);
  //   // artsSignalEdt(B_edt, depCount - 1, B_db[i]); // Dependency: B[i]
  //   // DataBlock
  // }
}

void parallelDoneEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                     artsEdtDep_t depv[]) {
  printf("---------- Parallel EDT finished ----------\n");
}

void computeEDT(int N, artsArrayDb_t *A_array, artsArrayDb_t *B_array) {
  printf("---------- Compute EDT ----------\n");
  // Context
  unsigned int currentNode = artsGetCurrentNode();
  artsGuid_t epochGuid = artsGetCurrentEpochGuid();

  /// Create parallel EDT
  artsGuid_t parallelDoneEdtGuid =
      artsEdtCreate((artsEdt_t)parallelDoneEdt, currentNode, 0, NULL, 1);
  artsGuid_t parallelDoneEpochGuid =
      artsInitializeAndStartEpoch(parallelDoneEdtGuid, 0);

  // unsigned int workers = artsGetTotalWorkers();
  // uint64_t parallelParams[1] = {(uint64_t)N};
  // for(unsigned int i=0; i<workers; i++) {
  //   artsGuid_t parallelEdtGuid = artsEdtCreateWithEpoch(
  //     (artsEdt_t)parallelEdt, i, 1, parallelParams, 0,
  //     parallelDoneEpochGuid);
  //   artsSignalEdt(parallelEdtGuid, 0, NULL_GUID);
  // }

  /// paramv is N
  // uint64_t paramv[1] = {(uint64_t)N};
  // artsGatherArrayDbEpoch(A_array, printDataBlockA, 0, 1, paramv, 0, epochGuid);
  // artsGatherArrayDbEpoch(B_array, printDataBlockB, 0, 1, paramv, 0, epochGuid);

  /// As of now, let's create only one instance of parallelEdt
  uint64_t parallelParams[1] = {(uint64_t)N};
  uint64_t parallelDeps = 2;
  artsGuid_t parallelEdtGuid = artsEdtCreateWithEpoch(
      (artsEdt_t)parallelEdt, currentNode, 1, parallelParams, parallelDeps,
      parallelDoneEpochGuid);

  /// Signal the dependencies
  artsSignalArrayDb(A_array, parallelEdtGuid, 0);
  artsSignalArrayDb(B_array, parallelEdtGuid, 1);

  /// Wait for the epoch to finish
  artsWaitOnHandle(parallelDoneEpochGuid);
  printf("---------- Compute EDT finished ----------\n");
}

void finishMainEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                   artsEdtDep_t depv[]) {
  printf("--------------------------------------\n");
  printf("The program has finished its execution\n");
  printf("--------------------------------------\n");
  artsShutdown();
}

// Main EDT: Sets up the epoch, creates DataBlocks, events, and tasks
void mainEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
             artsEdtDep_t depv[]) {
  printf("---------- Main EDT ----------\n");
  unsigned int currentNode = artsGetCurrentNode();
  // Create a finish EDT to end the epoch
  artsGuid_t finishMainEdtGuid =
      artsEdtCreate((artsEdt_t)finishMainEdt, currentNode, 0, NULL, 1);
  artsGuid_t finishMainEpochGuid =
      artsInitializeAndStartEpoch(finishMainEdtGuid, 0);

  /// Define the size of the computation
  int N = 10;

  // Allocate arrays for DataBlocks and events
  double *A = (double *)artsMalloc(N * sizeof(double));
  double *B = (double *)artsMalloc(N * sizeof(double));
  /// Initialize A with numbers from 0 to N-1
  for (int i = 0; i < N; i++) {
    A[i] = i;
    B[i] = 0;
  }

  // Create DataBlocks for A and B
  artsGuid_t A_guid = artsReserveGuidRoute(ARTS_DB_PIN, currentNode);
  artsGuid_t B_guid = artsReserveGuidRoute(ARTS_DB_PIN, currentNode);
  artsArrayDb_t *A_array =
      artsNewLocalArrayDbWithGuid(A_guid, sizeof(double), N, (void *)A);
  artsArrayDb_t *B_array =
      artsNewLocalArrayDbWithGuid(B_guid, sizeof(double), N, (void *)B);

  /// We dont need the arrays anymore
  // artsFree(A);
  // artsFree(B);

  computeEDT(N, A_array, B_array);
  // artsWaitOnHandle(finishMainEpochGuid);
  printf("---------- Main EDT finished ----------\n");
}

// initPerNode: Reserved for node-specific initializations
void initPerNode(unsigned int nodeId, int argc, char **argv) {
  if (nodeId == 0) {
    unsigned int currentNode = artsGetCurrentNode();
    // Create a main EDT to set up the tasks
    uint64_t mainparams[] = {};
    artsEdtCreate(mainEdt, currentNode, 1, mainparams, 0);
  }

  printf("Node %u initialized.\n", nodeId);
}

// initPerWorker: Reserved for worker-specific initializations
void initPerWorker(unsigned int nodeId, unsigned int workerId, int argc,
                   char **argv) {}

int main(int argc, char **argv) {
  artsRT(argc, argv); // Start the ARTS runtime
  return 0;
}