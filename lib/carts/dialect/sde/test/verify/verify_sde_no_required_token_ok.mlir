// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'

// PASS: a valid MU/CU/SU structure that uses NO sde.mu_token and no slice op.
// verify-sde requires no access-window carrier; %A is accessed directly inside
// the CU.
module {
  func.func @no_required_token() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %A = sde.mu_alloc : memref<8xf32>
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      sde.cu_region <parallel> {
        %z = arith.constant 0.000000e+00 : f32
        memref.store %z, %A[%i] : memref<8xf32>
        sde.yield
      }
      sde.yield
    }
    return
  }
}
