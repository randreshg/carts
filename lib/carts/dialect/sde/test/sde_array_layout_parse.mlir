// RUN: %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// CHECK-LABEL: func.func @array_layout_parse
// CHECK: sde.array_layout write array_id(0) owner [0] block [2, 4] logical [8, 4]
// CHECK: sde.array_layout read array_id(1) owner ({{.*}} : index) block [4, 4] logical [16, 16]
// CHECK: sde.array_layout read array_id(2) owner [] block [8] logical [8]
func.func @array_layout_parse() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index
  %A = sde.mu_alloc : memref<8x4xf64>
  %B = sde.mu_alloc : memref<16x16xf64>
  %C = sde.mu_alloc : memref<8xf64>
  sde.su_iterate (%c0) to (%c8) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout write array_id(0) owner [0] block [2, 4] logical [8, 4]
    sde.array_layout read array_id(1) owner (%c0 : index) block [4, 4] logical [16, 16]
    sde.array_layout read array_id(2) owner [] block [8] logical [8]
    sde.array_layout_root write %A : memref<8x4xf64> array_id(0)
    sde.array_layout_root read %B : memref<16x16xf64> array_id(1)
    sde.array_layout_root read %C : memref<8xf64> array_id(2)
    sde.yield
  }
  return
}
