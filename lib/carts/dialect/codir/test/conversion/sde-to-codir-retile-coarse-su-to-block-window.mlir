// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir)' \
// RUN:   | %FileCheck %s

// A committed block/window may be finer than a previously tiled scheduling
// step. SDE-to-CODIR must retile the cloned SU body to that window before ARTS
// receives one block DB per EDT.

// CHECK-LABEL: func.func @retile_coarse_su_to_committed_block
// CHECK: %[[OUTER_C4:.*]] = arith.constant 4 : index
// CHECK: scf.for {{.*}} step %[[OUTER_C4]]
// CHECK: codir.codelet
// CHECK-SAME: logical_worker_slice = [4]
// CHECK-SAME: tile_shape = [4]
// CHECK: ^bb0
// CHECK-DAG: %[[LOCAL_C4:.*]] = arith.constant 4 : index
// CHECK-DAG: %[[LOCAL_C1:.*]] = arith.constant 1 : index
// CHECK: %[[RAW_END:.*]] = arith.addi {{.*}}, %[[LOCAL_C4]] : index
// CHECK: %[[LOCAL_END:.*]] = arith.minui %[[RAW_END]],
// CHECK: scf.for {{.*}} to %[[LOCAL_END]] step %[[LOCAL_C4]]
// CHECK: scf.for {{.*}} to %[[LOCAL_END]] step %[[LOCAL_C1]]
func.func @retile_coarse_su_to_committed_block(%input: memref<64xf32>,
                                               %output: memref<64xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %c64 = arith.constant 64 : index
  sde.su_iterate (%c0) to (%c64) step (%c16) schedule(<static>)
      classification(<elementwise>) {
  ^bb0(%i: index):
    %tile_limit = arith.addi %i, %c16 : index
    %tile_upper = arith.minui %tile_limit, %c64 : index
    sde.cu_region <single> {
      scf.for %j = %i to %tile_upper step %c1 {
        %v = memref.load %input[%j] : memref<64xf32>
        memref.store %v, %output[%j] : memref<64xf32>
      }
    }
    sde.yield
  } {physicalOwnerDims = [0], physicalBlockShape = [4],
     logicalWorkerSlice = [4],
     iterationTopology = #sde.iteration_topology<owner_strip>,
     pattern = #sde.pattern<uniform>}
  return
}

// CHECK-LABEL: func.func @grouped_owner_tile_clones_su_local_bounds
// CHECK: codir.codelet
// CHECK-SAME: logical_worker_slice = [8, 4]
// CHECK: ^bb0
// CHECK: %[[I_LIMIT:.*]] = arith.addi {{.*}}, {{.*}} : index
// CHECK: %[[I_END:.*]] = arith.minui %[[I_LIMIT]],
// CHECK: scf.for {{.*}} to %[[I_END]]
func.func @grouped_owner_tile_clones_su_local_bounds(
    %input: memref<16x16xf32>, %output: memref<16x16xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c16 = arith.constant 16 : index
  sde.su_iterate (%c0, %c0) to (%c16, %c16) step (%c4, %c4)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    %i_limit = arith.addi %i, %c4 : index
    %i_end = arith.minui %i_limit, %c16 : index
    sde.cu_region <single> {
      scf.for %ii = %i to %i_end step %c1 {
        %j_limit = arith.addi %j, %c4 : index
        %j_end = arith.minui %j_limit, %c16 : index
        scf.for %jj = %j to %j_end step %c1 {
          %v = memref.load %input[%ii, %jj] : memref<16x16xf32>
          memref.store %v, %output[%ii, %jj] : memref<16x16xf32>
        }
      }
    }
    sde.yield
  } {physicalOwnerDims = [0, 1], physicalBlockShape = [4, 4],
     logicalWorkerSlice = [8, 4],
     iterationTopology = #sde.iteration_topology<owner_tile>,
     pattern = #sde.pattern<uniform>}
  return
}
