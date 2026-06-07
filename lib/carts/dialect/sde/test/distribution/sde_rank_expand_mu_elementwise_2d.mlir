// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Structural carrier: a committed single-contiguous-owner elementwise BLOCK
// plan (physicalOwnerDims=[0], physicalBlockShape=[16,64]) rank-expands the
// sde.mu_alloc result memref so the block grid is the leading dim of the TYPE,
// and rewrites every CU memref.load/store into the physical
// [block, intra-block, ...] coordinate system via div/mod localization. The
// chained verify-sde-mu-layout pass proves ownerDims == recover(structure).

// CHECK-LABEL: func.func @rank_expand_elementwise_2d
// The carrier is the TYPE (memref<8x16x64xf32>) with NO owner-dim/block-shape
// attribute on the mu_alloc (no `{` before the `:` => no attr-dict).
// CHECK: sde.mu_alloc : memref<8x16x64xf32>
// CHECK: sde.mu_alloc : memref<8x16x64xf32>
// Owner index %i is split into block (divui) and intra-block (remui) coords;
// the non-owner index %j passes through.
// CHECK: %[[BIDA:.*]] = arith.divui %{{.*}}, %c16
// CHECK: %[[OFFA:.*]] = arith.remui %{{.*}}, %c16
// CHECK: memref.load %{{.*}}[%[[BIDA]], %[[OFFA]], %{{.*}}] : memref<8x16x64xf32>
// CHECK: %[[BIDC:.*]] = arith.divui %{{.*}}, %c16
// CHECK: %[[OFFC:.*]] = arith.remui %{{.*}}, %c16
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[BIDC]], %[[OFFC]], %{{.*}}] : memref<8x16x64xf32>

func.func @rank_expand_elementwise_2d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  %C = sde.mu_alloc : memref<128x64xf32>
  sde.cu_region <parallel> {
    sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
    ^bb0(%i: index):
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        memref.store %v, %C[%i, %j] : memref<128x64xf32>
      }
      sde.yield
    } {physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
    sde.yield
  }
  return
}
