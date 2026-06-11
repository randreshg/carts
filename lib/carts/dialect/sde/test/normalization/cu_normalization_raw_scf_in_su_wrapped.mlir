// RUN: not %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REJECT

// Raw scf/source compute sitting directly inside an SU body makes the SU not
// scheduling-only. This is an SDE dialect invariant now, so parsing/verifying
// the op rejects it before any cleanup pass can treat invalid SDE as input.

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

// REJECT: 'scf.for' op is directly inside an sde.su_iterate body
// REJECT: SU bodies are scheduling-only and may contain only direct-boundary CUs
