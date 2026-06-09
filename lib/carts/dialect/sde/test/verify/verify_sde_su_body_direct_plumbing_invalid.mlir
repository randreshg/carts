// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)' \
// RUN:   | %FileCheck %s --check-prefix=NORM

module {
  func.func @direct_plumbing_in_su() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      %zero = arith.constant 0 : index
      %unused = arith.addi %zero, %i : index
      sde.yield
    }
    return
  }
}

// CHECK: directly inside an sde.su_iterate body
// CHECK: SU bodies are scheduling-only and may contain only CUs, sde.su_barrier, and the sde.yield terminator

// NORM-LABEL: func.func @direct_plumbing_in_su
// NORM:         sde.su_iterate
// NORM:           sde.cu_region <single> {
// NORM:             arith.addi
