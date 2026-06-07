// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-cu-normalization)' 2>&1 \
// RUN:   | %FileCheck %s

// A source-compute value (the loaded scalar) that escapes the conservative CU
// that would wrap it — here consumed by func.return, outside any wrappable span.
// An sde.cu_region result is tied 1:1 to an iter_args input and cannot express an
// output-only value, so the pass FAILS CLOSED with a diagnostic rather than
// fabricate a results/yield contract. The dumped IR confirms nothing was wrapped
// (no sde.cu_region introduced). The function holds an sde.mu_alloc so it is in
// scope (an SDE-bearing function).

module {
  func.func @escape(%A: memref<8xf32>) -> f32 {
    %c0 = arith.constant 0 : index
    %B = sde.mu_alloc : memref<8xf32>
    %v = memref.load %A[%c0] : memref<8xf32>
    return %v : f32
  }
}

// CHECK: 'memref.load' op defines a value used outside the conservative CU
// CHECK-NOT: sde.cu_region
