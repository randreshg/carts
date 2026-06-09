// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir)' \
// RUN:   | %FileCheck %s

// When the committed block grain is no finer than the scheduling step, CODIR
// does not retile: it clones the SU body's already-correct local tile bounds
// into the codelet verbatim. These are the legitimate success siblings of the
// fail-closed retile rejection in sde-to-codir-retile-coarse-su-to-block-window.

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

// CHECK-LABEL: func.func @owner_strip_keeps_integer_induction_cast
// CHECK: codir.codelet
// CHECK: arith.index_cast {{.*}} : index to i32
// CHECK: arith.addi {{.*}} : i32
// CHECK: arith.index_cast {{.*}} : i32 to index
func.func @owner_strip_keeps_integer_induction_cast(
    %input: memref<16x16xf64>, %output: memref<16x16xf64>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c16 = arith.constant 16 : index
  %cst = arith.constant 0.000000e+00 : f64
  %c1_i32 = arith.constant 1 : i32
  sde.su_iterate (%c0) to (%c16) step (%c1) schedule(<static>)
      classification(<matmul>) {
  ^bb0(%i: index):
    sde.cu_region <parallel> {
      %i_i32 = arith.index_cast %i : index to i32
      %next_i32 = arith.addi %i_i32, %c1_i32 : i32
      %j_lb = arith.index_cast %next_i32 : i32 to index
      scf.for %j = %j_lb to %c16 step %c1 {
        %sum = scf.for %k = %c0 to %c16 step %c1 iter_args(%acc = %cst) -> (f64) {
          %lhs = memref.load %input[%i, %k] : memref<16x16xf64>
          %rhs = memref.load %input[%j, %k] : memref<16x16xf64>
          %prod = arith.mulf %lhs, %rhs : f64
          %next = arith.addf %acc, %prod : f64
          scf.yield %next : f64
        }
        memref.store %sum, %output[%i, %j] : memref<16x16xf64>
      }
    }
    sde.yield
  } {physicalOwnerDims = [0], physicalBlockShape = [1, 16],
     logicalWorkerSlice = [1, 16],
     iterationTopology = #sde.iteration_topology<owner_strip>,
     pattern = #sde.pattern<matmul>}
  return
}
