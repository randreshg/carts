// RUN: not %carts-compile %s \
// RUN:   --pass-pipeline='builtin.module(verify-sde-physical-consistency)' 2>&1 \
// RUN:   | %FileCheck %s

// The jacobi-for class: the arrayLayout says a 2-D [512, 512] owner-tile budget
// over owner dims [0, 1], but the physical plan stamps a row strip
// ([1280, 10240], owner [0]). Both owner-dim block extents (1280, 10240) coarsen
// past the [512, 512] budget grain, so the stale plan fails closed before CODIR.

// CHECK: physicalBlockShape is coarser than the committed node-agnostic budget
func.func @jacobi_for_row_strip_over_owner_tile(%A: memref<10240x10240xf64>) {
  %c0 = arith.constant 0 : index
  %c10240 = arith.constant 10240 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0, %c0) to (%c10240, %c10240) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %j] : memref<10240x10240xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, budgetBlockShape = [512, 512],
                     kind = "block_parallel", ownerDims = [0, 1],
                     role = "write"}],
     iterationTopology = #sde.iteration_topology<owner_strip>,
     physicalBlockShape = [1280, 10240],
     physicalOwnerDims = [0]}
  return
}

// -----

// Block coarser than the node-agnostic budget grain on an owner dim, even with
// matching owner dims.
// CHECK: physicalBlockShape is coarser than the committed node-agnostic budget
func.func @block_coarser_than_budget(%A: memref<10240x10240xf64>) {
  %c0 = arith.constant 0 : index
  %c10240 = arith.constant 10240 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0, %c0) to (%c10240, %c10240) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %j] : memref<10240x10240xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, budgetBlockShape = [512, 512],
                     kind = "block_parallel", ownerDims = [0, 1],
                     role = "write"}],
     iterationTopology = #sde.iteration_topology<owner_tile>,
     logicalWorkerSlice = [1024, 512],
     physicalBlockShape = [1024, 512],
     physicalOwnerDims = [0, 1]}
  return
}

// -----

// Plan well-formedness: owner dim out of range of physicalBlockShape.
// CHECK: physicalOwnerDims must index physicalBlockShape dimensions
func.func @owner_dim_out_of_range(%A: memref<512xf64>) {
  %c0 = arith.constant 0 : index
  %c512 = arith.constant 512 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0) to (%c512) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i] : memref<512xf64>
      sde.yield
    }
    sde.yield
  } {physicalBlockShape = [512],
     physicalOwnerDims = [3]}
  return
}

// -----

// Schedule consistency: more owner dims than realized loop dims.
// CHECK: physical plan names more owner dimensions than realized SDE loop
func.func @more_owner_dims_than_loops(%A: memref<512x512xf64>) {
  %c0 = arith.constant 0 : index
  %c512 = arith.constant 512 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0) to (%c512) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %i] : memref<512x512xf64>
      sde.yield
    }
    sde.yield
  } {physicalBlockShape = [512, 512],
     physicalOwnerDims = [0, 1]}
  return
}
