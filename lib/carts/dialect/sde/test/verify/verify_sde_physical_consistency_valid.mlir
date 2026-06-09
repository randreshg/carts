// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-physical-consistency)' \
// RUN:   | %FileCheck %s

// A committed physical plan that agrees with its arrayLayout write fact, fits
// the realized schedule, and (here, before rank expansion) stores into the
// budget-grain block extent on the owner dims passes the consistency gate.

// CHECK-LABEL: func.func @consistent_owner_tile_plan
// CHECK: sde.su_iterate
// CHECK: physicalBlockShape = [512, 512]
func.func @consistent_owner_tile_plan(%A: memref<512x512xf64>) {
  %c0 = arith.constant 0 : index
  %c512 = arith.constant 512 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0, %c0) to (%c512, %c512) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %j] : memref<512x512xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, budgetBlockShape = [512, 512],
                     kind = "block_parallel", ownerDims = [0, 1],
                     role = "write"}],
     iterationTopology = #sde.iteration_topology<owner_tile>,
     logicalWorkerSlice = [512, 512],
     physicalBlockShape = [512, 512],
     physicalOwnerDims = [0, 1]}
  return
}

// An SU with no physical plan is out of scope and accepted unchanged.
// CHECK-LABEL: func.func @no_physical_plan_skipped
// CHECK: sde.su_iterate
func.func @no_physical_plan_skipped(%A: memref<512x512xf64>) {
  %c0 = arith.constant 0 : index
  %c512 = arith.constant 512 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.000000e+00 : f64
  sde.su_iterate (%c0, %c0) to (%c512, %c512) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      memref.store %cst, %A[%i, %j] : memref<512x512xf64>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, budgetBlockShape = [512, 512],
                     kind = "block_parallel", ownerDims = [0, 1],
                     role = "write"}]}
  return
}
