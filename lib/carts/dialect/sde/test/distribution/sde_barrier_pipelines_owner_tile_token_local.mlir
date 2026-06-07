// RUN: %carts-compile %s --pass-pipeline='builtin.module(barrier-elimination)' --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// BarrierElimination can remove a required-memory barrier only when SDE proves
// the dependency is token-local to the committed owner-tile block. A transposed
// read window keeps a real barrier plan.

// CHECK-LABEL: // -----// IR Dump After BarrierElimination (barrier-elimination) //----- //
// CHECK-LABEL: func.func @eliminates_owner_tile_token_local_barrier
// CHECK: sde.su_iterate
// CHECK: sde.su_barrier {barrierEliminated, barrierReason = #sde.barrier_reason<required_memory>}
// CHECK: sde.su_iterate
// CHECK-LABEL: func.func @preserves_transposed_owner_tile_barrier
// CHECK: sde.su_iterate
// CHECK: sde.su_barrier
// CHECK-SAME: barrierReason = #sde.barrier_reason<timestep_stage_boundary>
// CHECK-NOT: barrierEliminated
// CHECK: sde.su_iterate
// CHECK-LABEL: func.func @eliminates_3d_owner_tile_token_local_barrier
// CHECK: sde.su_iterate
// CHECK: sde.su_barrier {barrierEliminated, barrierReason = #sde.barrier_reason<required_memory>}
// CHECK: sde.su_iterate
// CHECK-LABEL: func.func @preserves_3d_transposed_owner_tile_barrier
// CHECK: sde.su_iterate
// CHECK: sde.su_barrier
// CHECK-SAME: barrierReason = #sde.barrier_reason<timestep_stage_boundary>
// CHECK-NOT: barrierEliminated
// CHECK: sde.su_iterate

module {
  func.func @eliminates_owner_tile_token_local_barrier(%A: memref<8x8xf32>,
                                                       %B: memref<8x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %tmp = sde.mu_alloc : memref<8x8xf32>

    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
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

      sde.su_barrier

      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %v = memref.load %tmp[%i, %j] : memref<8x8xf32>
            memref.store %v, %B[%i, %j] : memref<8x8xf32>
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4],
         physicalOwnerDims = [0, 1]}
      sde.yield
    }
    return
  }

  func.func @preserves_transposed_owner_tile_barrier(%A: memref<8x8xf32>,
                                                     %B: memref<8x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %tmp = sde.mu_alloc : memref<8x8xf32>

    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
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

      sde.su_barrier

      sde.su_iterate (%c0, %c0) to (%c8, %c8) step (%c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %v = memref.load %tmp[%j, %i] : memref<8x8xf32>
            memref.store %v, %B[%i, %j] : memref<8x8xf32>
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4],
         physicalOwnerDims = [0, 1]}
      sde.yield
    }
    return
  }

  func.func @eliminates_3d_owner_tile_token_local_barrier(%A: memref<8x8x8xf32>,
                                                          %B: memref<8x8x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %tmp = sde.mu_alloc : memref<8x8x8xf32>

    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0, %c0) to (%c8, %c8, %c8) step (%c4, %c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index, %tile_k: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %tile_k_end_raw = arith.addi %tile_k, %c4 : index
            %tile_k_end = arith.minui %tile_k_end_raw, %c8 : index
            scf.for %k = %tile_k to %tile_k_end step %c1 {
              %v = memref.load %A[%i, %j, %k] : memref<8x8x8xf32>
              memref.store %v, %tmp[%i, %j, %k] : memref<8x8x8xf32>
            }
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4, 4],
         physicalOwnerDims = [0, 1, 2]}

      sde.su_barrier

      sde.su_iterate (%c0, %c0, %c0) to (%c8, %c8, %c8) step (%c4, %c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index, %tile_k: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %tile_k_end_raw = arith.addi %tile_k, %c4 : index
            %tile_k_end = arith.minui %tile_k_end_raw, %c8 : index
            scf.for %k = %tile_k to %tile_k_end step %c1 {
              %v = memref.load %tmp[%i, %j, %k] : memref<8x8x8xf32>
              memref.store %v, %B[%i, %j, %k] : memref<8x8x8xf32>
            }
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4, 4],
         physicalOwnerDims = [0, 1, 2]}
      sde.yield
    }
    return
  }

  func.func @preserves_3d_transposed_owner_tile_barrier(%A: memref<8x8x8xf32>,
                                                        %B: memref<8x8x8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %c8 = arith.constant 8 : index
    %tmp = sde.mu_alloc : memref<8x8x8xf32>

    sde.cu_region <parallel> {
      sde.su_iterate (%c0, %c0, %c0) to (%c8, %c8, %c8) step (%c4, %c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index, %tile_k: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %tile_k_end_raw = arith.addi %tile_k, %c4 : index
            %tile_k_end = arith.minui %tile_k_end_raw, %c8 : index
            scf.for %k = %tile_k to %tile_k_end step %c1 {
              %v = memref.load %A[%i, %j, %k] : memref<8x8x8xf32>
              memref.store %v, %tmp[%i, %j, %k] : memref<8x8x8xf32>
            }
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4, 4],
         physicalOwnerDims = [0, 1, 2]}

      sde.su_barrier

      sde.su_iterate (%c0, %c0, %c0) to (%c8, %c8, %c8) step (%c4, %c4, %c4) classification(<elementwise>) {
      ^bb0(%tile_i: index, %tile_j: index, %tile_k: index):
        %tile_i_end_raw = arith.addi %tile_i, %c4 : index
        %tile_i_end = arith.minui %tile_i_end_raw, %c8 : index
        scf.for %i = %tile_i to %tile_i_end step %c1 {
          %tile_j_end_raw = arith.addi %tile_j, %c4 : index
          %tile_j_end = arith.minui %tile_j_end_raw, %c8 : index
          scf.for %j = %tile_j to %tile_j_end step %c1 {
            %tile_k_end_raw = arith.addi %tile_k, %c4 : index
            %tile_k_end = arith.minui %tile_k_end_raw, %c8 : index
            scf.for %k = %tile_k to %tile_k_end step %c1 {
              %v = memref.load %tmp[%j, %i, %k] : memref<8x8x8xf32>
              memref.store %v, %B[%i, %j, %k] : memref<8x8x8xf32>
            }
          }
        }
        sde.yield
      } {iterationTopology = #sde.iteration_topology<owner_tile>,
         logicalWorkerSlice = [4, 4, 4],
         pattern = #sde.pattern<uniform>,
         physicalBlockShape = [4, 4, 4],
         physicalOwnerDims = [0, 1, 2]}
      sde.yield
    }
    return
  }
}
