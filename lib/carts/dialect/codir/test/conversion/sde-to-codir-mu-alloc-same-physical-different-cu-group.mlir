// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir)' \
// RUN:   | %FileCheck %s

// Two users of the same SDE MU may use different CU grouping slices while
// sharing the same physical DB/MU layout. Physical layout compatibility must
// compare owner dims, block shape, halo, and topology, not logicalWorkerSlice.

module {
  func.func @mu_alloc_same_physical_layout_different_cu_group(%A: memref<8x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %tmp = sde.mu_alloc : memref<8x8xf32>

    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end = arith.addi %tile_i, %c4 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end = arith.addi %tile_j, %c4 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %v = memref.load %A[%i, %j] : memref<8x8xf32>
            memref.store %v, %tmp[%i, %j] : memref<8x8xf32>
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4],
         physicalOwnerDims = [0, 1]}

      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end = arith.addi %tile_i, %c4 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end = arith.addi %tile_j, %c4 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %v = memref.load %A[%i, %j] : memref<8x8xf32>
            memref.store %v, %tmp[%i, %j] : memref<8x8xf32>
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [8, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4],
         physicalOwnerDims = [0, 1]}

      sde.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @mu_alloc_same_physical_layout_different_cu_group
// CHECK: codir.codelet
// CHECK-SAME: logical_worker_slice = [4, 4]
// CHECK: codir.codelet
// CHECK-SAME: logical_worker_slice = [8, 4]
