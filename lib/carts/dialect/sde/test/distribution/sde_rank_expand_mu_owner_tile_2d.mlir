// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// ND structural carrier (multi-owner owner-tile): a committed elementwise BLOCK
// plan (physicalOwnerDims=[0,1], physicalBlockShape=[16,16]) rank-expands the
// sde.mu_alloc result memref so the 2-D block grid is the leading TWO dims of
// the TYPE, and rewrites every CU memref.load/store into the physical
// [bi, bj, ii, jj] coordinate system via per-owner div/mod localization. The
// chained verify-sde-mu-layout pass proves ownerDims == recover(structure) for
// BOTH owner dims. This is the jacobi-for-init shape (2-D data-parallel) that
// the single-contiguous-owner path could not realize.

// CHECK-LABEL: func.func @rank_expand_owner_tile_2d
// The written carrier is the TYPE (memref<8x4x16x16xf32>) with NO
// owner-dim/block-shape attribute on the mu_alloc.
// CHECK: sde.mu_alloc : memref<128x64xf32>
// CHECK: sde.mu_alloc : memref<8x4x16x16xf32>
// The read-only input stays in its source memref. Both owner indices are split
// for the write into block (divui) and intra-block (remui) coords.
// CHECK: memref.load %{{.*}}[%{{.*}}, %{{.*}}] : memref<128x64xf32>
// CHECK: arith.divui
// CHECK: arith.divui
// CHECK: arith.remui
// CHECK: arith.remui
// CHECK: memref.store %{{.*}}, %{{.*}}[%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}] : memref<8x4x16x16xf32>

func.func @rank_expand_owner_tile_2d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  %C = sde.mu_alloc : memref<128x64xf32>
  sde.su_iterate (%c0, %c0) to (%c128, %c64) step (%c1, %c1)
      classification(<elementwise>) {
  ^bb0(%i: index, %j: index):
    sde.cu_region <single> {
      %v = memref.load %A[%i, %j] : memref<128x64xf32>
      memref.store %v, %C[%i, %j] : memref<128x64xf32>
      sde.yield
    }
    sde.yield
  } {physicalOwnerDims = [0, 1], physicalBlockShape = [16, 16]}
  return
}
