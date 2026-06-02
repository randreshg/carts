// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir,storage-planning,verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts)' \
// RUN:   | %FileCheck %s --implicit-check-not=codir.codelet --implicit-check-not=sde.

module {
  func.func @tokenizes_aligned_owner_tile_write_read_barrier(%A: memref<8x8xf32>,
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

      sde.su_barrier {barrierEliminated, barrierReason = #sde.barrier_reason<required_memory>}

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

  func.func @preserves_barrier_for_transposed_owner_tile_window(%A: memref<8x8xf32>,
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

      // The successor reads the same root through transposed owner dims, so
      // convert-sde-to-codir must fail closed and keep the ARTS barrier.
      sde.su_barrier {barrierEliminated, barrierReason = #sde.barrier_reason<required_memory>}

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
}

// CHECK-LABEL: func.func @tokenizes_aligned_owner_tile_write_read_barrier
// CHECK: scf.for
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: arts.db_acquire[<out>]
// CHECK: arts.edt <task>
// CHECK-NOT: arts.barrier
// CHECK: scf.for
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: arts.db_acquire[<in>]
// CHECK: arts.edt <task>
// CHECK-LABEL: func.func @preserves_barrier_for_transposed_owner_tile_window
// CHECK: scf.for
// CHECK: arts.edt <task>
// CHECK: arts.barrier
// CHECK: scf.for
