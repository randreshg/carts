// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' -o %t.mlir
// RUN: %carts-compile %t.mlir --pass-pipeline='builtin.module(verify-sde)'
// RUN: %carts-compile %t.mlir --pass-pipeline='builtin.module(sde-cu-normalization)' \
// RUN:   | %FileCheck %s

// The pass emits cu_region<single> with no iter_args, whose printed form omits
// the implicit sde.yield. This pins that the printed output round-trips through a
// print/parse boundary: reparsing it (the cu_region parser must synthesize the
// omitted terminator without crashing) and re-verifying succeeds, and a second
// normalization of the reparsed IR is a no-op.

module {
  func.func @roundtrip(%A: memref<8xf32>) {
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

// CHECK-LABEL: func @roundtrip
// CHECK-COUNT-1: sde.cu_region <single>
// CHECK-NOT: sde.cu_region <single>
