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
