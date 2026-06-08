// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Once movement is explicit as sde.redist, redistribution is owned by
// sde-redistribute / verify-sde-redistribute.

// CHECK-LABEL: func.func @retires_coarse
// CHECK: sde.redist <reduce_scatter_like>

func.func @retires_coarse() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc : memref<128x64xf32>
  sde.redist <reduce_scatter_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64] cost 1024
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<matmul>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
    sde.yield
  }
  return
}
