#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "arts.h"
#include "artsRT.h"

#include "artsRT.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
clang -O3 taskwithdeps.c -o taskwithdeps \
-I/home/randres/projects/carts/.install/arts/include \
-L/home/randres/projects/carts/.install/arts/lib -larts \
-L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib \
-lrdmacm

*/

//---------------------------------------------------------------------
// initPerWorker: Worker-specific initialization
void initPerWorker(unsigned int nodeId, unsigned int workerId, int argc,
                   char **argv) {}

//---------------------------------------------------------------------
// edt1: Initializes A[i] = i.
// Dependencies:
//  - depv[0]: the DB pointer for A[i] (from artsAddDependence)
// Parameters (paramv):
//  - paramv[0]: index i
//  - paramv[1]: persistent event GUID for A[i] (to be satisfied)
void edt1(unsigned int paramc, uint64_t *paramv, uint32_t depc,
          artsEdtDep_t depv[]) {
  unsigned int i = (unsigned int)paramv[0];
  int *A_i = (int *)depv[0].ptr;
  *A_i = i;
  printf("Task %d - 0: Initializing A[%d] = %d\n", i, i, *A_i);

  // Signal the persistent event for A[i] by decrementing its latch count.
  // This will release any EDT waiting on the updated A[i].
  artsPersistentEventDecrementLatch((artsGuid_t)paramv[1], depv[0].guid);
}

//---------------------------------------------------------------------
// edt2: For i == 0. Computes B[0] = A[0] + 5.
// Dependencies:
//  - depv[0]: the DB pointer for B[0] (output)
//  - depv[1]: the value from A_events[0] (updated A[i])
// Parameters (paramv):
//  - paramv[0]: index i
//  - paramv[1]: persistent event GUID for B[i] (to be satisfied)
void edt2(unsigned int paramc, uint64_t *paramv, uint32_t depc,
          artsEdtDep_t depv[]) {
  unsigned int i = (unsigned int)paramv[0];
  int *B_0 = (int *)depv[0].ptr;
  int *A_0 = (int *)depv[1].ptr;
  *B_0 = *A_0 + 5;
  printf("Task %d - 1: Computing B[%d] = %d\n", i, i, *B_0);

  // Signal the persistent event for B[0] by decrementing its latch.
  artsPersistentEventDecrementLatch((artsGuid_t)paramv[1], depv[0].guid);
}

//---------------------------------------------------------------------
// edt3: For i > 0. Computes B[i] = A[i] + B[i-1] + 5.
// Dependencies:
//  - depv[0]: the DB pointer for B[i] (output)
//  - depv[1]: the value from A_events[i] (updated A[i])
//  - depv[2]: the value from B_events[i-1] (updated B[i-1])
// Parameters (paramv):
//  - paramv[0]: index i
//  - paramv[1]: persistent event GUID for B[i] (to be satisfied)
void edt3(unsigned int paramc, uint64_t *paramv, uint32_t depc,
          artsEdtDep_t depv[]) {
  unsigned int i = (unsigned int)paramv[0];
  int *B_i = (int *)depv[0].ptr;
  int *A_i = (int *)depv[1].ptr;
  int *B_im1 = (int *)depv[2].ptr;
  *B_i = *A_i + *B_im1 + 5;
  printf("Task %d - 2: Computing B[%d] = %d\n", i, i, *B_i);

  // Signal the persistent event for B[i] by decrementing its latch.
  artsPersistentEventDecrementLatch((artsGuid_t)paramv[1], depv[0].guid);
}

//---------------------------------------------------------------------
// doneEdt: Called when the epoch completes.
void doneEdt(unsigned int paramc, uint64_t *paramv, uint32_t depc,
             artsEdtDep_t depv[]) {
  printf("-----------------\nMain function DONE\n-----------------\n");
}

//---------------------------------------------------------------------
// initPerNode: Node-specific initialization (for node 0).
// All arrays for persistent events and DB guids are allocated locally.
// Only minimal information is passed to the EDTs.
void initPerNode(unsigned int nodeId, int argc, char **argv) {
  if (nodeId != 0)
    return;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s N\n", argv[0]);
    return;
  }
  int N = atoi(argv[1]);

  // Allocate local arrays for persistent event GUIDs and DB GUIDs.
  artsGuid_t A_events[N], B_events[N];
  artsGuid_t A_guid[N], B_guid[N];
  int *A_ptr[N], *B_ptr[N];

  // Create DataBlocks for each A[i] and B[i] (DB GUIDs are stored).
  for (int i = 0; i < N; i++) {
    A_guid[i] = artsReserveGuidRoute(ARTS_DB_PIN, nodeId);
    A_ptr[i] = (int *)artsDbCreateWithGuid(A_guid[i], sizeof(int));
    B_guid[i] = artsReserveGuidRoute(ARTS_DB_PIN, nodeId);
    B_ptr[i] = (int *)artsDbCreateWithGuid(B_guid[i], sizeof(int));
  }

  srand(time(NULL));
  printf("-----------------\nMain function\n-----------------\n");

  // Create persistent events (initialize with latch count = 0).
  for (int i = 0; i < N; i++) {
    A_events[i] = artsPersistentEventCreate(nodeId, 0, A_guid[i]);
    B_events[i] = artsPersistentEventCreate(nodeId, 0, B_guid[i]);
  }

  // Create the "done" EDT and start the epoch.
  artsGuid_t doneEdtGuid =
      artsEdtCreate((artsEdt_t)doneEdt, nodeId, 0, NULL, 1);
  artsGuid_t epochGuid = artsInitializeAndStartEpoch(doneEdtGuid, 0);

  // For each index, create the appropriate EDTs.
  for (int i = 0; i < N; i++) {
    printf("- Iteration #%d\n", i);
    // edt1
    // Parameter vector: { i, A_events[i]}
    uint64_t paramv1[2] = {(uint64_t)i, (uint64_t)A_events[i]};
    printf("Creating EDT1\n");
    artsGuid_t edt1_guid = artsEdtCreateWithEpoch((artsEdt_t)edt1, nodeId, 2,
                                                  paramv1, 1, epochGuid);
    artsAddDependenceToPersistentEvent(A_events[i], edt1_guid, 0);
    /// There is a definition of A[i]
    artsPersistentEventIncrementLatch(A_events[i], A_guid[i]);

    if (i == 0) {
      // Parameter vector: { 0, B_events[0]}
      uint64_t paramv2[3] = {0, (uint64_t)B_events[0]};
      printf("Creating EDT2\n");
      artsGuid_t edt2_guid = artsEdtCreateWithEpoch((artsEdt_t)edt2, nodeId, 2,
                                                    paramv2, 2, epochGuid);
      artsAddDependenceToPersistentEvent(B_events[i], edt2_guid, 0);
      artsAddDependenceToPersistentEvent(A_events[i], edt2_guid, 1);
      /// There is a definition of B[i]
      artsPersistentEventIncrementLatch(B_events[i], B_guid[i]);
    } else {
      // edt3
      // Parameter vector: { i, B_events[i]}
      uint64_t paramv3[2] = {(uint64_t)i, (uint64_t)B_events[i]};
      printf("Creating EDT3\n");
      artsGuid_t edt3_guid = artsEdtCreateWithEpoch((artsEdt_t)edt3, nodeId, 3,
                                                    paramv3, 3, epochGuid);
      artsAddDependenceToPersistentEvent(B_events[i], edt3_guid, 0);
      artsAddDependenceToPersistentEvent(A_events[i], edt3_guid, 1);
      artsAddDependenceToPersistentEvent(B_events[i - 1], edt3_guid, 2);
      /// There is a definition of B[i]
      artsPersistentEventIncrementLatch(B_events[i], B_guid[i]);
    }
  }

  // Wait for the epoch to complete (all EDTs to finish).
  artsWaitOnHandle(epochGuid);
  printf("Final arrays:\n");
  // Print A and B
  for (int i = 0; i < N; i++) {
    printf("A[%d] = %d, B[%d] = %d\n", i, *A_ptr[i], i, *B_ptr[i]);
  }
  printf("-----------------\nMain function DONE\n-----------------\n");
  artsShutdown();
}

//---------------------------------------------------------------------
// main: Start the ARTS runtime.
int main(int argc, char **argv) {
  artsRT(argc, argv);
  return 0;
}