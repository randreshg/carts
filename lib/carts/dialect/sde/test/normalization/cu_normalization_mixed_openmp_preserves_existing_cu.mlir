// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REJECT
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=NORM
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)'

// A function mixing already-converted OpenMP work (the cu_region<parallel> /
// su_iterate / cu_region<parallel> nest) with a raw non-OpenMP host loop. Only
// the raw host loop is missing a CU, so verify-sde REJECTS the raw module.
// sde-cu-normalization wraps ONLY the host loop in a cu_region<single> and
// leaves the existing OpenMP CU/SU structure unchanged.

module {
  func.func @mixed(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.cu_region <parallel> {
      sde.su_iterate (%c0) to (%c8) step (%c1) {
      ^bb0(%i: index):
        sde.cu_region <parallel> {
          scf.for %j = %c0 to %c8 step %c1 {
            %v = memref.load %A[%j] : memref<8xf32>
            memref.store %v, %A[%j] : memref<8xf32>
          }
          sde.yield
        }
        sde.yield
      }
      sde.yield
    }
    scf.for %k = %c0 to %c8 step %c1 {
      %w = memref.load %A[%k] : memref<8xf32>
      memref.store %w, %A[%k] : memref<8xf32>
    }
    return
  }
}

// REJECT: source executable work outside any CU

// The existing OpenMP-derived structure is preserved verbatim, and exactly one
// new conservative single-CU is introduced for the host loop.
// NORM-LABEL: func @mixed
// NORM:         sde.cu_region <parallel> {
// NORM:           sde.su_iterate
// NORM:             sde.cu_region <parallel> {
// NORM:               scf.for
// NORM:         sde.cu_region <single> {
// NORM:           scf.for
// NORM-NOT:     sde.cu_region <single>
