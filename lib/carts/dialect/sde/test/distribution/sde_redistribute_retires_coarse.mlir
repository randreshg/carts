// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// A matmul-classified MU is a coarse last resort to verify-sde-coarse-avoidance
// (see sde_coarse_avoidance_diagnose_matmul.mlir for the error direction). Once
// its movement is explicit as sde.redist, its redistribution is owned by
// sde-redistribute / verify-sde-redistribute, so the coarse-avoidance gate
// accepts it (exit 0) instead of diagnosing a last resort.

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
