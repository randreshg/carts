// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REJECT
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=NORM
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)'

// A non-OpenMP host init loop at function scope, in an SDE-bearing function (it
// also holds an sde.mu_alloc), is source executable work outside any CU.
// verify-sde REJECTS it before normalization; sde-cu-normalization wraps the
// loop in a conservative cu_region<single> and verify-sde ACCEPTS the result.
// Index plumbing, constants, and the sde.mu_alloc stay at function scope.

module {
  func.func @host_init(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %B = sde.mu_alloc : memref<8xf32>
    %z = arith.constant 0.000000e+00 : f32
    scf.for %i = %c0 to %c8 step %c1 {
      memref.store %z, %B[%i] : memref<8xf32>
    }
    return
  }
}

// REJECT: source executable work outside any CU

// The index plumbing, constants, and sde.mu_alloc stay at function scope (before
// the CU), and exactly one conservative single-CU is introduced (no
// over-wrapping).
// NORM-LABEL: func @host_init
// NORM:         arith.constant 8 : index
// NORM:         sde.mu_alloc
// NORM:         arith.constant 0.0
// NORM:         sde.cu_region <single> {
// NORM:           scf.for
// NORM:             memref.store
// NORM-NOT:      sde.cu_region <single>
