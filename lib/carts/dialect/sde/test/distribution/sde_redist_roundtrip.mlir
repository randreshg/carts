// RUN: %carts-compile %s --pass-pipeline='builtin.module()' 2>&1 | %FileCheck %s

// sde.redist carries the full geometric movement-family taxonomy and round-trips
// through parse/print/verify, including replicated endpoints (empty owner dims),
// halo widths, and multi-owner block layouts. CODIR maps each family to a
// concrete transport mechanically.

// CHECK-LABEL: func.func @redist_taxonomy
func.func @redist_taxonomy(%A: memref<128x64xf32>) {
  // CHECK: sde.redist <reduce_scatter_like> %{{.*}} : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64] cost 1024
  sde.redist <reduce_scatter_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64] cost 1024
  // CHECK: sde.redist <all_gather_like> %{{.*}} : memref<128x64xf32> from owner [0] block [16, 64] to owner [] block [128, 64]
  sde.redist <all_gather_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [] block [128, 64]
  // CHECK: sde.redist <broadcast_like> %{{.*}} : memref<128x64xf32> from owner [0] block [16, 64] to owner [] block [128, 64]
  sde.redist <broadcast_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [] block [128, 64]
  // CHECK: sde.redist <all_to_all_like> %{{.*}} : memref<128x64xf32> from owner [0] block [16, 64] to owner [1] block [128, 16]
  sde.redist <all_to_all_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [1] block [128, 16]
  // CHECK: sde.redist <halo_like> %{{.*}} : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64] halo [1, 0]
  sde.redist <halo_like> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [16, 64] halo [1, 0]
  // CHECK: sde.redist <phase_redist> %{{.*}} : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [32, 64]
  sde.redist <phase_redist> %A : memref<128x64xf32> from owner [0] block [16, 64] to owner [0] block [32, 64]
  // CHECK: sde.redist <reduce_scatter_like> %{{.*}} : memref<128x64xf32> from owner [0, 1] block [16, 16] to owner [0, 1] block [16, 16]
  sde.redist <reduce_scatter_like> %A : memref<128x64xf32> from owner [0, 1] block [16, 16] to owner [0, 1] block [16, 16]
  return
}
