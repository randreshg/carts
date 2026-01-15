/*
 * CARTS Mixed Access Pattern Test
 * Demonstrates LULESH-like access patterns that trigger DbCopy/DbSync:
 *
 * 1. Node-parallel loop: Direct write x[i] = ... (CHUNKED partitioning)
 *    - Workers write contiguous chunks of node data
 *
 * 2. Element-parallel loop: Indirect read x[nodelist[elem][j]] (FINE-GRAINED)
 *    - Each element reads 4 nodes via indirection array
 *    - Access pattern is irregular, requires fine-grained DB
 *
 * This mixed pattern (chunked write + fine-grained read) triggers:
 * - DbCopy: Creates fine-grained copy of chunked allocation
 * - DbSync: Synchronizes data before indirect reads
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "arts/Utils/Testing/CartsTest.h"

#define NODES_PER_ELEM 4

// Sequential version for verification
static void compute_seq(int numNodes, int numElems, double *nodeData,
                        double *elemData, int (*nodelist)[NODES_PER_ELEM]) {
  // Phase 1: Initialize node data (direct access)
  for (int i = 0; i < numNodes; i++) {
    nodeData[i] = (double)(i * i) * 0.01;
  }

  // Phase 2: Compute element values from nodes (indirect access)
  for (int e = 0; e < numElems; e++) {
    double sum = 0.0;
    for (int j = 0; j < NODES_PER_ELEM; j++) {
      int nodeIdx = nodelist[e][j];
      sum += nodeData[nodeIdx];
    }
    elemData[e] = sum / NODES_PER_ELEM;
  }
}

// Parallel version with #pragma omp parallel for
static void compute_parallel(int numNodes, int numElems, double *nodeData,
                             double *elemData, int (*nodelist)[NODES_PER_ELEM]) {
  // Phase 1: Node-parallel initialization - CHUNKED access
  // Each worker writes a contiguous chunk of nodes
#pragma omp parallel for
  for (int i = 0; i < numNodes; i++) {
    nodeData[i] = (double)(i * i) * 0.01;
  }

  // Phase 2: Element-parallel computation - INDIRECT access
  // Each element reads scattered nodes via nodelist indirection
  // This triggers DbCopy (chunked -> fine-grained) + DbSync
#pragma omp parallel for
  for (int e = 0; e < numElems; e++) {
    double sum = 0.0;
    for (int j = 0; j < NODES_PER_ELEM; j++) {
      int nodeIdx = nodelist[e][j];
      sum += nodeData[nodeIdx];
    }
    elemData[e] = sum / NODES_PER_ELEM;
  }
}

// Build a simple 2D quad mesh connectivity
// Elements are quads with 4 nodes each
static void build_mesh(int nx, int ny, int (*nodelist)[NODES_PER_ELEM]) {
  int numNodesX = nx + 1;
  for (int ey = 0; ey < ny; ey++) {
    for (int ex = 0; ex < nx; ex++) {
      int elem = ey * nx + ex;
      // Counter-clockwise node ordering for quad element
      nodelist[elem][0] = ey * numNodesX + ex;         // bottom-left
      nodelist[elem][1] = ey * numNodesX + ex + 1;     // bottom-right
      nodelist[elem][2] = (ey + 1) * numNodesX + ex + 1; // top-right
      nodelist[elem][3] = (ey + 1) * numNodesX + ex;   // top-left
    }
  }
}

int main(void) {
  CARTS_TIMER_START();

#ifdef SIZE
  int nx = SIZE, ny = SIZE;
#else
  int nx = 50, ny = 50;
#endif

  int numNodesX = nx + 1;
  int numNodesY = ny + 1;
  int numNodes = numNodesX * numNodesY;
  int numElems = nx * ny;

  printf("Mixed Access Test: %d nodes, %d elements\n", numNodes, numElems);
  printf("Pattern: chunked node writes + fine-grained indirect reads\n");

  // Allocate arrays
  double *nodeData = (double *)malloc(numNodes * sizeof(double));
  double *nodeData_seq = (double *)malloc(numNodes * sizeof(double));
  double *elemData = (double *)malloc(numElems * sizeof(double));
  double *elemData_seq = (double *)malloc(numElems * sizeof(double));
  int (*nodelist)[NODES_PER_ELEM] =
      (int (*)[NODES_PER_ELEM])malloc(numElems * sizeof(*nodelist));

  // Initialize
  for (int i = 0; i < numNodes; i++) {
    nodeData[i] = 0.0;
    nodeData_seq[i] = 0.0;
  }
  for (int e = 0; e < numElems; e++) {
    elemData[e] = 0.0;
    elemData_seq[e] = 0.0;
  }

  // Build mesh connectivity
  build_mesh(nx, ny, nodelist);

  // Run sequential version for verification
  printf("Running sequential version...\n");
  compute_seq(numNodes, numElems, nodeData_seq, elemData_seq, nodelist);

  // Run parallel version
  printf("Running parallel version...\n");
  compute_parallel(numNodes, numElems, nodeData, elemData, nodelist);

  // Compare results
  double error = 0.0;
  double max_error = 0.0;
  for (int e = 0; e < numElems; e++) {
    double diff = elemData_seq[e] - elemData[e];
    error += diff * diff;
    if (fabs(diff) > max_error) {
      max_error = fabs(diff);
    }
  }
  error = sqrt(error / numElems);

  printf("RMS error: %e\n", error);
  printf("Max error: %e\n", max_error);

  // Free arrays
  free(nodeData);
  free(nodeData_seq);
  free(elemData);
  free(elemData_seq);
  free(nodelist);

  if (error < 1e-10) {
    CARTS_TEST_PASS();
  } else {
    CARTS_TEST_FAIL("mixed_access verification failed");
  }
}
