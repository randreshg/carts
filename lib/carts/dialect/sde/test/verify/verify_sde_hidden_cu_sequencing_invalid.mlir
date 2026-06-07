// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde)' 2>&1 \
// RUN:   | %FileCheck %s

// FAIL: two conflicting compute units (write %X then read %X via mu_token on the
// shared %X) with no explicit ordering between them rely on hidden textual order.
// CUs are async/schedulable by default.
module {
  func.func @hidden_cu_sequencing() {
    %X = sde.mu_alloc : memref<8xf32>
    %tw = sde.mu_token <write> %X : memref<8xf32> -> !sde.token<memref<8xf32>>
    sde.cu_work (%tw : !sde.token<memref<8xf32>>) {
    ^bb0(%xw: memref<8xf32>):
      %c0 = arith.constant 0 : index
      %z = arith.constant 0.000000e+00 : f32
      memref.store %z, %xw[%c0] : memref<8xf32>
      sde.yield
    }
    %tr = sde.mu_token <read> %X : memref<8xf32> -> !sde.token<memref<8xf32>>
    sde.cu_work (%tr : !sde.token<memref<8xf32>>) {
    ^bb0(%xr: memref<8xf32>):
      %c0 = arith.constant 0 : index
      %v = memref.load %xr[%c0] : memref<8xf32>
      sde.yield
    }
    return
  }
}

// CHECK: hidden textual order
