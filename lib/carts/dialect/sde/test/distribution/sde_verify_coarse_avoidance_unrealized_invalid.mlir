// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// verify-sde-coarse-avoidance rejects a flat MU that IS in the block-grid realize
// scope (a committed single-contiguous-owner elementwise plan proving a real grid)
// but was left coarse: sde-coarse-avoidance should have rank-expanded it.

// CHECK: error: {{.*}}realize scope but left coarse

func.func @unrealized() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      memref.store %cst, %A[%i] : memref<1024xf32>
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
    sde.yield
  }
  return
}
