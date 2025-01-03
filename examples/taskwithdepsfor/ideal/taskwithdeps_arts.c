// clang taskwithdeps_arts.c -O3 -g0 -march=native
// -I/home/randres/projects/arts/.install/arts/include -o taskwithdeps_arts
// -L/home/randres/projects/arts/.install/arts/lib -larts -lrdmacm
#include "arts.h"
#include "artsRT.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void computeA(uint32_t paramc, uint64_t *paramv, uint32_t depc,
              artsEdtDep_t depv[]) {
  // paramv[0] = i, paramv[1] = A[i] eventGuid
  unsigned int i = (unsigned int)paramv[0];
  artsGuid_t eventGuid = (artsGuid_t)paramv[1];

  // Access the DataBlock for A[i]
  double *A_i = (double *)depv[0].ptr;
  artsGuid_t A_i_guid = depv[0].guid;

  // Compute A[i]
  A_i[0] = i * 2.0;

  printf("------------------------\n"
         "--- Compute A[%u] = %f - Guid: "
         "%lu\n------------------------\n",
         i, A_i[0], A_i_guid);
  // Signal the event associated with A[i] after computation
  artsEventSatisfySlot(eventGuid, A_i_guid, ARTS_EVENT_LATCH_DECR_SLOT);
}

void computeB(uint32_t paramc, uint64_t *paramv, uint32_t depc,
              artsEdtDep_t depv[]) {
  unsigned int i = (unsigned int)paramv[0];

  // Access the DataBlocks for A[i], A[i-1] (via events), and B[i]
  double *A_i = (double *)depv[1].ptr;
  double A_im1_val = 0.0;
  if (i > 0) {
    double *A_im1 = (double *)depv[2].ptr;
    A_im1_val = A_im1[0];
  }
  double *B_i = (double *)depv[0].ptr;

  // Compute B[i]
  B_i[0] = A_i[0] + A_im1_val;
  printf("------------------------\n"
         "--- Compute B[%u] = %f\n"
         "------------------------\n",
         i, B_i[0]);
}

void printDataBlockA(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                     artsEdtDep_t depv[]) {
  for (int i = 0; i < paramv[0]; i++) {
    double *data = (double *)depv[i].ptr;
    printf("A[%d]: %f\n", i, *data);
  }
}

void printDataBlockB(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                     artsEdtDep_t depv[]) {
  for (int i = 0; i < paramv[0]; i++) {
    double *data = (double *)depv[i].ptr;
    printf("B[%d]: %f\n", i, *data);
  }
}

/// Parallel EDT
void parallelEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                 artsEdtDep_t depv[]) {
  printf("---------- Parallel EDT started ----------\n");
  /// Context
  unsigned int currentNode = artsGetCurrentNode();
  artsGuid_t curentEpoch = artsGetCurrentEpochGuid();

  /// Parameters
  int N = paramv[0];

  /// Convert from depv to DataBlocks
  artsDataBlock A_array[N], B_array[N];
  artsDbCreateArrayFromDeps(A_array, N, depv, 0);
  artsDbCreateArrayFromDeps(B_array, N, depv, N);

  // Preallocate all DataBlocks and events
  artsGuid_t *A_event = (artsGuid_t *)artsMalloc(N * sizeof(artsGuid_t));
  for (int i = 0; i < N; i++)
    A_event[i] = artsEventCreate(currentNode, 1);

  // Create tasks for A[i] and B[i]
  for (int i = 0; i < N; i++) {
    // Task 1: Compute A[i]
    uint64_t Aparams[] = {(uint64_t)i, (uint64_t)A_event[i]};
    artsGuid_t A_edt = artsEdtCreateWithEpoch((artsEdt_t)computeA, currentNode,
                                              2, Aparams, 1, curentEpoch);

    // Task 2: Compute B[i]
    uint64_t Bparams[] = {(uint64_t)i};
    // Dependencies: A[i], (A[i-1] if i > 0), and B[i]
    int depCount = (i > 0) ? 3 : 2;
    artsGuid_t B_edt = artsEdtCreateWithEpoch(
        (artsEdt_t)computeB, currentNode, 2, Bparams, depCount, curentEpoch);

    // Add dependencies to B[i] EDT through events
    // Dependency: A[i]
    artsAddDependence(A_event[i], B_edt, 1);
    // Dependency: A[i-1]
    if (i > 0)
      artsAddDependence(A_event[i - 1], B_edt, 2);

    /// Signal values after dependencies are added
    artsSignalEdt(A_edt, 0, A_array[i].guid);
    artsSignalEdt(B_edt, 0, B_array[i].guid);
  }
}

void parallelDoneEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                     artsEdtDep_t depv[]) {
  printf("---------- Parallel EDT finished ----------\n");
}

void computeEDT(int N, artsDataBlock *A_array, artsDataBlock *B_array) {
  printf("---------- Compute EDT ----------\n");
  /// Context
  unsigned int currentNode = artsGetCurrentNode();
  artsGuid_t curentEpoch = artsGetCurrentEpochGuid();

  /// Create parallel EDT
  artsGuid_t parallelDoneEdtGuid =
      artsEdtCreate((artsEdt_t)parallelDoneEdt, currentNode, 0, NULL, 1);
  artsGuid_t parallelDoneEpochGuid =
      artsInitializeAndStartEpoch(parallelDoneEdtGuid, 0);

  /// As of now, let's create only one instance of parallelEdt
  uint64_t parallelParams[1] = {(uint64_t)N};
  uint64_t parallelDeps = 2 * N;
  artsGuid_t parallelEdtGuid = artsEdtCreateWithEpoch(
      (artsEdt_t)parallelEdt, currentNode, 1, parallelParams, parallelDeps,
      parallelDoneEpochGuid);

  /// Signal the dependencies
  artsSignalDbs(A_array, parallelEdtGuid, 0, N);
  artsSignalDbs(B_array, parallelEdtGuid, N, N);

  /// Wait for the parallel epoch to finish
  artsWaitOnHandle(parallelDoneEpochGuid);

  /// Print the arrays
  uint64_t paramv[1] = {(uint64_t)N};
  /// Print A
  artsGuid_t printAEdtGuid = artsEdtCreateWithEpoch(
      (artsEdt_t)printDataBlockA, currentNode, 1, paramv, N, curentEpoch);
  artsSignalDbs(A_array, printAEdtGuid, 0, N);
  /// Print B
  artsGuid_t printBEdtGuid = artsEdtCreateWithEpoch(
      (artsEdt_t)printDataBlockB, currentNode, 1, paramv, N, curentEpoch);
  artsSignalDbs(B_array, printBEdtGuid, 0, N);

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
  int N = 5;

  // Allocate arrays for DataBlocks and events
  double *A = (double *)artsMalloc(N * sizeof(double));
  double *B = (double *)artsMalloc(N * sizeof(double));
  /// Initialize A with numbers from 0 to N-1
  for (int i = 0; i < N; i++) {
    A[i] = i;
    B[i] = 0;
  }

  // Create DataBlocks for A and B
  artsDataBlock A_array[N], B_array[N];
  artsDbCreateArray(A_array, sizeof(double), ARTS_DB_PIN, N, (void *)A);
  artsDbCreateArray(B_array, sizeof(double), ARTS_DB_PIN, N, (void *)B);

  // Call the computeEDT
  computeEDT(N, A_array, B_array);

  // Wait for the finish EDT to end the epoch
  artsWaitOnHandle(finishMainEpochGuid);
  // We dont need the arrays anymore
  artsFree(A);
  artsFree(B);
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