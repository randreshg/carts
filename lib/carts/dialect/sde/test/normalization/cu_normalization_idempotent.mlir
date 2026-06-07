// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=ONCE
// RUN: %carts-compile %s \
// RUN:   --pass-pipeline='builtin.module(sde-cu-normalization,sde-cu-normalization)' \
// RUN:   | %FileCheck %s --check-prefix=TWICE

// Running the pass a second time is a no-op: once the host loop is inside a
// cu_region<single>, that loop body is a CU body and its work is no longer
// "outside a CU", so the second run wraps nothing. Both runs yield exactly one
// single-CU (no nested double-wrap).

module {
  func.func @host_loop(%A: memref<8xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %B = sde.mu_alloc : memref<8xf32>
    scf.for %i = %c0 to %c8 step %c1 {
      %v = memref.load %A[%i] : memref<8xf32>
      memref.store %v, %A[%i] : memref<8xf32>
    }
    return
  }
}

// ONCE-LABEL: func @host_loop
// ONCE-COUNT-1: sde.cu_region <single>
// ONCE-NOT: sde.cu_region <single>

// TWICE-LABEL: func @host_loop
// TWICE-COUNT-1: sde.cu_region <single>
// TWICE-NOT: sde.cu_region <single>
