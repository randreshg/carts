// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REJECT
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=NORM
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization,verify-sde)'

// Raw scf/source compute sitting directly inside an SU body makes the SU not
// scheduling-only. verify-sde REJECTS it; sde-cu-normalization moves the loop
// into a cu_region<single> inside the SU body, leaving the SU to schedule a CU.
// The su_iterate scheduling structure itself is preserved.

module {
  func.func @raw_in_su(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    sde.su_iterate (%c0) to (%c8) step (%c1) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c8 step %c1 {
        %v = memref.load %A[%j] : memref<8xf32>
        memref.store %v, %A[%j] : memref<8xf32>
      }
      sde.yield
    }
    return
  }
}

// REJECT: raw scf/source compute directly inside an SU body

// NORM-LABEL: func @raw_in_su
// NORM:         sde.su_iterate
// NORM:           sde.cu_region <single> {
// NORM:             scf.for
// NORM:               memref.load
// NORM:               memref.store
