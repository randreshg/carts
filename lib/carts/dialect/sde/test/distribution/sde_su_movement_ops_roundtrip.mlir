// RUN: %carts-compile %s --pass-pipeline='builtin.module(canonicalize)' 2>&1 | %FileCheck %s

// The first-class SU-scope movement ops (Part 2): owner-preserving stencil halo
// and owned-axis reduce-scatter. Movement ops are legal only as direct children
// of sde.su_distribute. They round-trip and survive canonicalize (NOT Pure /
// MemWrite, so not DCE'd).

// CHECK-LABEL: func.func @su_movement_roundtrip
// CHECK: sde.su_distribute <owner_compute>
// CHECK: sde.su_halo %{{.*}} : memref<8x4xf64> array_id(0) owner [0] block [2, 4] halo [1, 0]
// CHECK: sde.su_reduce_scatter %{{.*}} : memref<8x4xf64> array_id(0) owner [0] block [2, 4] reduce 0 kind <add>
// CHECK: sde.su_all_to_all %{{.*}} : memref<8x4xf64> array_id(0) source owner [0] block [2, 4] target owner [0] block [4, 4]
func.func @su_movement_roundtrip(%A: memref<8x4xf64>) {
  sde.su_distribute <owner_compute> {
    sde.su_halo %A : memref<8x4xf64> array_id(0) owner [0] block [2, 4] halo [1, 0]
    sde.su_reduce_scatter %A : memref<8x4xf64> array_id(0) owner [0] block [2, 4] reduce 0 kind <add>
    sde.su_all_to_all %A : memref<8x4xf64> array_id(0) source owner [0] block [2, 4] target owner [0] block [4, 4]
  }
  return
}
