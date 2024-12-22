

/// clang++ taskwithdeps_arts.c -O3 -g3 -march=native -o $@ -I/home/randres/projects/carts/.install/arts/include -L/home/randres/projects/carts/.install/arts/lib -larts -lrdmacm
#include "arts.h"
#include "artsRT.h"
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
  artsGuid_t B_i_db = (artsGuid_t)paramv[1];

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

// Main EDT: Sets up the epoch, creates DataBlocks, events, and tasks
void mainEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
             artsEdtDep_t depv[]) {
  // Context
  unsigned int currentNode = artsGetCurrentNode();
  artsGuid_t epochGuid = artsGetCurrentEpochGuid();

  /// Define the size of the computation
  int N = 10;

  double *A = (double *)malloc(N * sizeof(double));
  double *B = (double *)malloc(N * sizeof(double));

  /// Initialize A with numbers from 0 to N-1
  for (int i = 0; i < N; i++) {
    A[i] = i;
    B[i] = 0;
  }

  // Allocate arrays for DataBlocks and events
  artsGuid_t *A_db = (artsGuid_t *)artsMalloc(N * sizeof(artsGuid_t));
  artsGuid_t *B_db = (artsGuid_t *)artsMalloc(N * sizeof(artsGuid_t));

  
  // ToDo: Create API to create multiple DataBlocks at once
  for (int i = 0; i < N; i++) {
    // Create DataBlocks for A[i] and B[i]
    double *A_ptr, *B_ptr;
    A_db[i] = artsDbCreate((void **)&A_ptr, sizeof(double), ARTS_DB_PIN);
    B_db[i] = artsDbCreate((void **)&B_ptr, sizeof(double), ARTS_DB_PIN);
    /// Initialize DataBlocks
    A_ptr[0] = i;       // A[i] initialized to i
    B_ptr[0] = 0.0;     // B[i] initialized to 0
  }




  artsGuid_t *A_event = (artsGuid_t *)artsMalloc(N * sizeof(artsGuid_t));

  // Preallocate all DataBlocks and events
  for (int i = 0; i < N; i++) {
    // Create DataBlocks for A[i] and B[i]
    double *A_ptr, *B_ptr;
    A_db[i] = artsDbCreate((void **)&A_ptr, sizeof(double), ARTS_DB_PIN);
    B_db[i] = artsDbCreate((void **)&B_ptr, sizeof(double), ARTS_DB_PIN);

    // Create latch event for A[i]
    A_event[i] = artsEventCreate(currentNode, 1); // latchCount = 1
  }

  // Preallocate all DataBlocks and events
  for (int i = 0; i < N; i++) {
      // Create DataBlocks for A[i] and B[i]
      double *A_ptr, *B_ptr;
      A_db[i] = artsDbCreate((void **)&A_ptr, sizeof(double), ARTS_DB_PIN);
      B_db[i] = artsDbCreate((void **)&B_ptr, sizeof(double), ARTS_DB_PIN);

      // Initialize DataBlocks
      A_ptr[0] = i;       // A[i] initialized to i
      B_ptr[0] = 0.0;     // B[i] initialized to 0
      // Create latch event for A[i]
      A_event[i] = artsEventCreate(currentNode, 1); // latchCount = 1
  }

  

  // Cleanup
  // artsFree(A_db);
  // artsFree(B_db);
  // artsFree(A_event);
}

/// Parallel EDT
void parallelEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                 artsEdtDep_t depv[]) {
  printf("Parallel EDT\n");

  /// Single region
  if (artsGetCurrentWorker() != 0)
    return;

  int N = paramv[0];
  unsigned int currentNode = artsGetCurrentNode();
  
  // Create tasks for A[i] and B[i]
  for (int i = 0; i < N; i++) {
      // Task 1: Compute A[i]
      uint64_t Aparams[3] = {(uint64_t)i, (uint64_t)A_db[i],
                            (uint64_t)A_event[i]};
      artsGuid_t A_edt = artsEdtCreateWithEpoch((artsEdt_t)computeA,
                                                currentNode, 3, Aparams,
                                                1, // Dependency on A[i] DataBlock
                                                epochGuid);

      // Signal the dependency to A[i] EDT
      artsSignalEdt(A_edt, 0, A_db[i]);

      // Task 2: Compute B[i]
      uint64_t Bparams[2] = {(uint64_t)i, (uint64_t)B_db[i]};
      int depCount =
          (i > 0) ? 3 : 2; // Dependencies: A[i], (A[i-1] if i > 0), and B[i]

      artsGuid_t B_edt =
          artsEdtCreateWithEpoch((artsEdt_t)computeB, currentNode, 2,
                                Bparams, depCount, epochGuid);

      // Add dependencies to B[i] EDT through events
      artsAddDependence(A_event[i], B_edt, 0); // Dependency: A[i]
      if (i > 0) {
        artsAddDependence(A_event[i - 1], B_edt, 1); // Dependency: A[i-1]
      }
      artsSignalEdt(B_edt, depCount - 1, B_db[i]); // Dependency: B[i] DataBlock
    }
}



void finishEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
               artsEdtDep_t depv[]) {
  printf("The program has finished its execution\n");
  artsShutdown();
}

// initPerNode: Reserved for node-specific initializations
void initPerNode(unsigned int nodeId, int argc, char **argv) {
  if (nodeId == 0) {
    int N = 10; // Define the size of the computation
    unsigned int currentNode = artsGetCurrentNode();
    // Create a finish EDT to end the epoch
    artsGuid_t finishEdtGuid =
        artsEdtCreate((artsEdt_t)finishEdtGuid, currentNode, 0, NULL, 1);
    artsGuid_t epochGuid = artsInitializeAndStartEpoch(finishEdtGuid, 0);

    // Create a main EDT to set up the tasks
    uint64_t Mainparams[1] = {(uint64_t)N};
    artsEdtCreateWithEpoch(mainEdt, currentNode, 1, Mainparams , 1, epochGuid);
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