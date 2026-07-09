// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-layout-coherence)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @coherent_layout
// CHECK: sde.array_layout write array_id(0) owner [0] block [256] logical [1024]
func.func @coherent_layout() {
  %c0 = arith.constant 0 : index
  %c1024 = arith.constant 1024 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 0.0 : f64
  %A = sde.mu_alloc : memref<1024xf64>
  sde.su_iterate (%c0) to (%c1024) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout write array_id(0) owner [0] block [256] logical [1024]
    sde.array_layout_root write %A : memref<1024xf64> array_id(0)
    sde.cu_region <parallel> {
      memref.store %cst, %A[%i] : memref<1024xf64>
      sde.yield
    }
    sde.yield
  }
  return
}
