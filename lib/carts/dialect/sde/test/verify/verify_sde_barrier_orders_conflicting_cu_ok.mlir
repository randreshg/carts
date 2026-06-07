// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde)'

// PASS: two conflicting compute units (write %X then read %X via mu_token on the
// shared %X) are ordered by an explicit plain sde.su_barrier between them.
module {
  func.func @barrier_orders_cu() {
    %X = sde.mu_alloc : memref<8xf32>
    %tw = sde.mu_token <write> %X : memref<8xf32> -> !sde.token<memref<8xf32>>
    sde.cu_work (%tw : !sde.token<memref<8xf32>>) {
    ^bb0(%xw: memref<8xf32>):
      %c0 = arith.constant 0 : index
      %z = arith.constant 0.000000e+00 : f32
      memref.store %z, %xw[%c0] : memref<8xf32>
      sde.yield
    }
    sde.su_barrier
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
