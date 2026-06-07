// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-coarse-avoidance)' 2>&1 | %FileCheck %s

// Diagnose: a dynamic owner extent cannot be statically rank-expanded into a
// block grid. The verifier fails closed with a reason.

// CHECK: error: {{.*}}dynamic MU shape has no static block grid

func.func @diagnose_dynamic(%n: index) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %cst = arith.constant 0.0 : f32
  %A = sde.mu_alloc(%n) : memref<?x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%n) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j] : memref<?x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
    sde.yield
  }
  return
}
