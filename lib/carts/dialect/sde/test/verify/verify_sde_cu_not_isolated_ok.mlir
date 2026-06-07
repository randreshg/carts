// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'

// PASS: cu_region is NOT IsolatedFromAbove; its body references the outer memref
// %A directly, with no sde.mu_token. verify-sde must not require isolation at
// the SDE boundary (CODIR is the first isolation boundary).
module {
  func.func @cu_not_isolated(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.cu_region <parallel> {
      scf.for %j = %c0 to %c8 step %c1 {
        %v = memref.load %A[%j] : memref<8xf32>
        memref.store %v, %A[%j] : memref<8xf32>
      }
      sde.yield
    }
    return
  }
}
