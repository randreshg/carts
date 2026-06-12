// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout)' 2>&1 | %FileCheck %s

// Structural carrier: committed single-contiguous-owner elementwise BLOCK facts
// (physicalOwnerDims=[0], physicalBlockShape=[16,64]) rank-expand the
// written sde.mu_alloc result memref so the block grid is the leading dim of
// the TYPE, and rewrites the CU store into the physical [block, intra-block,
// ...] coordinate system via div/mod localization. The chained
// verify-sde-mu-layout pass proves ownerDims == recover(structure).

// CHECK-LABEL: func.func @rank_expand_elementwise_2d
// The written carrier is the TYPE (memref<8x16x64xf32>) with NO
// owner-dim/block-shape attribute on the mu_alloc.
// CHECK: sde.mu_alloc : memref<128x64xf32>
// CHECK: sde.mu_alloc : memref<8x16x64xf32>
// Read-only input stays in its source memref. The owner index %i is split for
// the write into block (divui) and intra-block (remui) coords; the non-owner
// index %j passes through.
// CHECK: memref.load %{{.*}}[%{{.*}}, %{{.*}}] : memref<128x64xf32>
// CHECK: %[[BIDA:.*]] = arith.divui %{{.*}}, %c16
// CHECK: %[[OFFA:.*]] = arith.remui %{{.*}}, %c16
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[BIDA]], %[[OFFA]], %{{.*}}] : memref<8x16x64xf32>

func.func @rank_expand_elementwise_2d() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c128 = arith.constant 128 : index
  %A = sde.mu_alloc : memref<128x64xf32>
  %C = sde.mu_alloc : memref<128x64xf32>
  sde.su_iterate (%c0) to (%c128) step (%c1) classification(<elementwise>) {
  ^bb0(%i: index):
    sde.array_layout_root write %C : memref<128x64xf32> array_id(0)
    sde.cu_region <single> {
      scf.for %j = %c0 to %c64 step %c1 {
        %v = memref.load %A[%i, %j] : memref<128x64xf32>
        memref.store %v, %C[%i, %j] : memref<128x64xf32>
    }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 64], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 8 : i64, ownerDims = [0], role = "write"}], physicalOwnerDims = [0], physicalBlockShape = [16, 64]}
  return
}

// CHECK-LABEL: func.func @rank_expand_owner_tile_2d
// CHECK: sde.mu_alloc : memref<128x64xf32>
// CHECK: sde.mu_alloc : memref<8x4x16x16xf32>
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
    sde.array_layout_root write %C : memref<128x64xf32> array_id(0)
    sde.cu_region <single> {
      %v = memref.load %A[%i, %j] : memref<128x64xf32>
      memref.store %v, %C[%i, %j] : memref<128x64xf32>
      sde.yield
    }
    sde.yield
  } {arrayLayout = [{arrayId = 0 : i64, blockShape = [16, 16], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 32 : i64, ownerDims = [0, 1], role = "write"}], physicalOwnerDims = [0, 1], physicalBlockShape = [16, 16]}
  return
}
