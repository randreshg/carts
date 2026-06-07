// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)'

// Schedule/index plumbing (index constants and index arithmetic) is permitted
// outside a CU by verify-sde, so it already passes the verifier and the pass
// must leave it untouched: no new conservative single-CU is introduced. The only
// real compute already lives inside an existing cu_region<parallel>. The pass
// only ever creates cu_region<single>, so its absence proves a clean no-op.

module {
  func.func @plumbing_only(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %B = sde.mu_alloc : memref<8xf32>
    %n = arith.addi %c8, %c1 : index
    sde.su_iterate (%c0) to (%n) step (%c1) {
    ^bb0(%i: index):
      %off = arith.muli %i, %c1 : index
      sde.cu_region <parallel> {
        scf.for %j = %c0 to %c8 step %c1 {
          %v = memref.load %A[%off] : memref<8xf32>
          memref.store %v, %A[%off] : memref<8xf32>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }
}

// CHECK-LABEL: func @plumbing_only
// CHECK-NOT: sde.cu_region <single>
